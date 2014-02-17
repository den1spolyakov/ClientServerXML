#include "Server.h"

ClientContext::ClientContext()
{
	pol = new OVERLAPPED;
	wbuf = new WSABUF;
	ZeroMemory(pol, sizeof(OVERLAPPED));
	ZeroMemory(buffer, maxBufferLen);	

	socket = SOCKET_ERROR;
	wbuf->buf = buffer;
	wbuf->len = maxBufferLen;
	opCode = 0;
	nTotalBytes = 0;
	nSentBytes = 0;
}

ClientContext::~ClientContext()
{
	while (!HasOverlappedIoCompleted(pol))
	{
		Sleep(0);
	}
	closesocket(socket);
	delete pol;
	delete wbuf;
}

void ClientContext::resetWsaBuff()
{
	zeroBuffer();
	wbuf->buf = buffer;
	wbuf->len = maxBufferLen;
}

Server::Server(HANDLE & h)
{
	if (initialize() == false)
	{
		exit(1);
	}

	shutdownEvent = h;
	
	sockaddr_in serverAddress;
	listenSocket = WSASocket(AF_INET, SOCK_STREAM,
		0, nullptr, 0, WSA_FLAG_OVERLAPPED);
	if (listenSocket == INVALID_SOCKET)
	{
		closesocket(listenSocket);
		deinitialize();
		exit(1);
	}

	parser.setPath(filePath);
	
	ZeroMemory((char *) & serverAddress, sizeof(serverAddress));
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr.s_addr = INADDR_ANY;
	serverAddress.sin_port = htons(defaultPort);

	if ((bind(listenSocket, (sockaddr *)&serverAddress,
		sizeof(serverAddress)) == SOCKET_ERROR) || 
		(listen(listenSocket, SOMAXCONN) == SOCKET_ERROR))
	{
		closesocket(listenSocket);
		deinitialize();
	}

	acceptEvent = WSACreateEvent();
	if (acceptEvent == WSA_INVALID_EVENT)
	{
		closesocket(listenSocket);
		deinitialize();
	}

	if (WSAEventSelect(listenSocket, acceptEvent, FD_ACCEPT) == SOCKET_ERROR)
	{
		WSACloseEvent(acceptEvent);
		closesocket(listenSocket);
		deinitialize();
	}
	acceptThread = std::thread(&Server::acceptThreadFunction, this, this);
}

Server::~Server()
{
	clean();
	closesocket(listenSocket);
	deinitialize();
}

bool Server::initialize()
{
	nThreads = workersPerCore * getNumberOfProcessors();

	WSADATA wsaData;

	int result = WSAStartup(MAKEWORD(2,2), &wsaData);
	if ((result != NO_ERROR) || (initializeIOCP() == false))
	{
		return false;
	}
	return true;
}

bool Server::initializeIOCP()
{
	ioCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
	if (ioCompletionPort == nullptr)
	{
		return false;
	}
	for (int i = 0; i < nThreads; i++)
	{
		workerThreads.push_back(std::thread(&Server::workerThread, this, this, i + 1));
	}
	return true;
}

void Server::clean()
{
	acceptThread.join();
	for (int i = 0; i < nThreads; i++)
	{
		PostQueuedCompletionStatus(ioCompletionPort, 0, (DWORD)nullptr, nullptr);
	}

	for (auto it = workerThreads.begin(); it != workerThreads.end(); it++)
	{
		it->join();
	}

	WSACloseEvent(acceptEvent);
	clearClientList();
}

void Server::deinitialize()
{
	CloseHandle(ioCompletionPort);
	WSACleanup();
}

void Server::clearClientList()
{
	std::lock_guard<std::mutex> lock(listMutex);
	for (auto it = clientContexts.begin(); it != clientContexts.end(); it++)
	{
		delete *it;
	}
	clientContexts.clear();

}

int Server::getNumberOfProcessors()
{
	static int processors = 0;
	if (processors == 0)
	{
		SYSTEM_INFO info;
		GetSystemInfo(&info);
		processors = info.dwNumberOfProcessors;
	}
	return processors;
}


void Server::acceptThreadFunction(Server * s)
{
	SOCKET listenSocket = s->listenSocket;
	WSANETWORKEVENTS WSAEvents;
	while (WAIT_OBJECT_0 != WaitForSingleObject(shutdownEvent, 0))
	{
		if (WSA_WAIT_TIMEOUT != WSAWaitForMultipleEvents(1, &acceptEvent, 
			FALSE, timeoutInterval, FALSE))
		{
			WSAEnumNetworkEvents(listenSocket, acceptEvent, &WSAEvents);
			if ((WSAEvents.lNetworkEvents & FD_ACCEPT) && 
				(WSAEvents.iErrorCode[FD_ACCEPT_BIT] == 0))
			{
				acceptConnection(listenSocket);
			}
		}
	}
}

void Server::acceptConnection(SOCKET listenSocket)
{
	sockaddr_in clientAddress;
	int length = sizeof(clientAddress);
	SOCKET socket = accept(listenSocket, (sockaddr *) &clientAddress, &length);
	if (socket == INVALID_SOCKET)
	{
		return;
	}

	ClientContext * clientContext = new ClientContext;
	clientContext->setOpCode(OP_READ);
	clientContext->setSocket(socket);

	addToClientList(clientContext);

	if (associateWithIOCP(clientContext) == true)
	{
		clientContext->setOpCode(OP_WRITE);
		WSABUF * w_buf = clientContext->getWsaBuff();
		OVERLAPPED * w_pol = clientContext->getOverlapped();
		DWORD dwFlags = 0;
		DWORD dwBytes = 0;

		int bytesReceived = WSARecv(clientContext->getSocket(), w_buf, 1, 
			&dwBytes, &dwFlags, w_pol, nullptr);
	}
}

bool Server::associateWithIOCP(ClientContext * client)
{
	HANDLE temp = CreateIoCompletionPort((HANDLE)client->getSocket(), 
		ioCompletionPort, (DWORD)client, 0);
	if (temp == nullptr)
	{
		removeFromClientList(client);
		return false;
	}
	return true;
}

void Server::addToClientList(ClientContext * client)
{
	std::lock_guard<std::mutex> lock(listMutex);
	clientContexts.push_back(client);
}

void Server::removeFromClientList(ClientContext * client)
{
	std::lock_guard<std::mutex> lock(listMutex);
	for (auto it = clientContexts.begin(); it != clientContexts.end(); it++)
	{
		if (client == *it)
		{
			clientContexts.erase(it);
			delete client;
			break;
		}
	}
}

void Server::workerThread(Server * server, int id)
{
	void *lpContext = nullptr;
	OVERLAPPED * overlapped = nullptr;
	ClientContext * clientContext = nullptr;
	DWORD bytesTransfered = 0;
	int bytesReceived = 0;
	int bytesSent = 0;
	DWORD bytes = 0, flags = 0;

	while (WAIT_OBJECT_0 != WaitForSingleObject(shutdownEvent, 0))
	{
		BOOL bReturn = GetQueuedCompletionStatus(ioCompletionPort, &bytesTransfered,
			(LPDWORD)&lpContext, &overlapped, INFINITE);
		if (lpContext == nullptr)
		{
			break;
		}
		clientContext = (ClientContext *)lpContext;
		if ((FALSE == bReturn) || ((TRUE == bReturn) && (0 == bytesTransfered)))
		{
			removeFromClientList(clientContext);
			continue;
		}

		WSABUF *p_wbuf = clientContext->getWsaBuff();
		OVERLAPPED * p_ol = clientContext->getOverlapped();

		switch (clientContext->getOpCode())
		{
		case OP_READ:
			clientContext->increaseSentBytes(bytesTransfered);

			if (clientContext->getSentBytes() < clientContext->getTotalBytes())
			{
				clientContext->setOpCode(OP_READ);
				p_wbuf->buf += clientContext->getSentBytes();
				p_wbuf->len = clientContext->getTotalBytes() - clientContext->getSentBytes();
				flags = 0;
				bytesSent = WSASend(clientContext->getSocket(), p_wbuf, 1,
					&bytes, flags, p_ol, nullptr);
				if ((bytesSent == SOCKET_ERROR) && (WSA_IO_PENDING != WSAGetLastError()))
				{
					removeFromClientList(clientContext);
				}
			}
			else
			{
				clientContext->setOpCode(OP_WRITE);
				clientContext->resetWsaBuff();
				flags = 0;
				bytesReceived = WSARecv(clientContext->getSocket(), p_wbuf, 1,
					&bytes, &flags, p_ol, nullptr);
				if ((bytesReceived == SOCKET_ERROR) && (WSA_IO_PENDING != WSAGetLastError()))
				{
					removeFromClientList(clientContext);
				}
			}
			break;
		case OP_WRITE:
		{
			char sbuffer[maxBufferLen];
			clientContext->getBuffer(sbuffer);
			std::string result = parser.processQuery(sbuffer);

			clientContext->setOpCode(OP_READ);
			clientContext->setTotalBytes(result.size() + 1);
			clientContext->setSentBytes(0);

			p_wbuf->len = result.size() + 1;
			clientContext->setBuffer(&result[0]);

			bytesSent = WSASend(clientContext->getSocket(), p_wbuf, 1,
				&bytes, flags, p_ol, nullptr);
			if ((bytesSent == SOCKET_ERROR) && (WSA_IO_PENDING != WSAGetLastError()))
			{
				removeFromClientList(clientContext);
			}
			break;
		}
		default:
			break;
		}
	}
}

