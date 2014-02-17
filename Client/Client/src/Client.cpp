#include "Client.h"

const std::string errorIO     = "An error occured during I/O operation: ";
const std::string emptyInput  = "Empty query!";
const std::string greeting    = "\t\t\tWelcome to the client program!";
const std::string errorConnect = "Invalid address! Can't connect to server.";
const std::string helpMessage = "You can use following commands to communicate with server:\n\
add <tag> <value>, get <tag>, set <tag> <value>.\n\nType \"quit\" to disconnect from server \
and \"help\" to display help message.\n";

Client::Client()
{
	std::cout << greeting << std::endl << std::endl;
	connected = false;
	WSADATA wsaData;
	int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (result != NO_ERROR)
	{
		exit(1);
	}
}

Client::~Client()
{
	closesocket(clientSocket);
	WSACleanup();
}

void Client::connectToServer(std::string address, int port)
{
	sockaddr_in serverAddress;
	hostent	* server;
	
	clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (clientSocket == INVALID_SOCKET)
	{
		return;
	}

	server = gethostbyname(address.c_str());
	if (server == NULL)
	{
		std::cout << errorConnect << std::endl;
		closesocket(clientSocket);
		return;
	}

	ZeroMemory((char *) &serverAddress, sizeof(serverAddress));
	serverAddress.sin_family = AF_INET;
	CopyMemory((char *)&serverAddress.sin_addr.s_addr,
		(char *)server->h_addr, server->h_length);
	serverAddress.sin_port = htons(port);
	if (connect(clientSocket, reinterpret_cast<const sockaddr *>(&serverAddress), 
		sizeof(serverAddress)) == SOCKET_ERROR)
	{
		std::cout << "Couldn't connect to: " << address << std::endl;
		closesocket(clientSocket);
		return;
	}
	connected = true;
}

void Client::communicate()
{
	if (isConnected() == false)
	{
		return;
	}
	
	std::cout << std::endl << helpMessage << std::endl;
	
	std::string message;
	message.reserve(maxBufferLen - 1);
	int bytesSent = 0, bytesReceived = 0;
	
	while (true)
	{
		std::cout << "->";

		std::getline(std::cin, message);
		
		if (message == "quit")
		{
			break;
		}
		else if (message == "help")
		{
			std::cout << helpMessage << std::endl;
			continue;
		}
		else if (message.empty())
		{
			std::cout << emptyInput << std::endl << std::endl;
			continue;
		}

		bytesSent = send(clientSocket, &message[0], message.size(), 0);
		if (bytesSent == SOCKET_ERROR)
		{
			std::cout << errorIO << WSAGetLastError()
				<< std::endl;
			return;
		}
		
		bytesReceived = recv(clientSocket, &message[0], maxBufferLen - 1, 0);
		if (bytesReceived == SOCKET_ERROR)
		{
			std::cout << errorIO << WSAGetLastError()
				<< std::endl;
			return;
		}
		
		std::cout << &message[0] << std::endl << std::endl;
	}
}