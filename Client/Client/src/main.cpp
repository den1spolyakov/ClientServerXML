#include "Client.h"

int main()
{
	Client client;
	std::string address;
	std::cout << "Enter server address: ";
	std::getline(std::cin, address);
	client.connectToServer(address, defaultPort);
	if (client.isConnected())
	{
		std::cout << "\nYou've connected successfully!\n";
		client.communicate();
	}
	std::cout << std::endl << "Press enter to finish...";
	std::cin.get();
	return 0;
}