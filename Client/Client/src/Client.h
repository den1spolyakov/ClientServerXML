#pragma once

#include <stdlib.h>
#include <iostream>
#include <string>
#include <winsock2.h>

#pragma warning(disable: 4996)

#pragma comment(lib, "Ws2_32.lib")

const unsigned maxBufferLen = 256;
const unsigned defaultPort = 8084;

class Client
{
private:
	SOCKET clientSocket;
	bool connected;
public:
	Client();
	~Client();
	void connectToServer(std::string address, int port);
	void communicate();
	bool isConnected() const { return connected; }
};
