#pragma once

#pragma warning(disable: 4996)

#include <stdlib.h>
#include <iostream>
#include <winsock2.h>
#include <vector>
#include <thread>

#include "ParserWrapper.h"

#pragma comment(lib, "Ws2_32.lib")

#define OP_READ     0
#define OP_WRITE    1

const int workersPerCore  = 1;
const int maxBufferLen    = 256;
const int timeoutInterval = 100;
const int defaultPort     = 8084;

const std::string filePath = "pairs.xml";

class ClientContext;

class Server
{
private:
	HANDLE shutdownEvent;
	HANDLE ioCompletionPort;
	WSAEVENT acceptEvent;
	SOCKET listenSocket;

	std::vector<ClientContext * > clientContexts;
	std::vector<std::thread> workerThreads;
	std::thread acceptThread;
	std::mutex listMutex;

	int nThreads;
	ParserWrapper parser;	
public:
	Server(HANDLE & h);
	~Server();
	bool initializeIOCP();
	bool initialize();
	void clean();
	void deinitialize();
	void acceptThreadFunction(Server * server);
	void acceptConnection(SOCKET listenSocket);
	bool associateWithIOCP(ClientContext * client);
	void workerThread(Server * server, int id);
	void addToClientList(ClientContext * client);
	void removeFromClientList(ClientContext * client);
	void clearClientList();
	int  getNumberOfProcessors();
};

class ClientContext
{
private:
	OVERLAPPED * pol;
	WSABUF * wbuf;
	SOCKET socket;
	int	nTotalBytes;
	int nSentBytes;
	int opCode;
	char buffer[maxBufferLen];
public:
	ClientContext();
	~ClientContext();
	void   resetWsaBuff();
	void   setOpCode(int code) { opCode = code; }
	int    getOpCode() const { return opCode; }
	void   setTotalBytes(int bytes) { nTotalBytes = bytes; }
	int    getTotalBytes() const { return nTotalBytes; }
	void   setSentBytes(int bytes) { nSentBytes = bytes; }
	void   increaseSentBytes(int bytes) { nSentBytes += bytes; }
	int    getSentBytes() const { return nSentBytes; }
	void   setSocket(SOCKET s) { socket = s; }		
	SOCKET getSocket() const { return socket; }
	void   setBuffer(char * b) { strcpy(buffer, b); }
	void   getBuffer(char * b) const { strcpy(b, buffer); }
	void   zeroBuffer() { ZeroMemory(buffer, maxBufferLen); }
	void   setWsaBuffLen(int len) { wbuf->len = len; }
	int    getWsaBuffLen() const { return wbuf->len; }
	WSABUF * getWsaBuff() { return wbuf; }
	OVERLAPPED * getOverlapped() { return pol; }
};