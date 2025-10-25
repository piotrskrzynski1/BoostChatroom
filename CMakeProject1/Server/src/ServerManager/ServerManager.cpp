#include <ServerManager/ServerManager.h>
#include <boost/asio.hpp>
ServerManager::ServerManager(int port, std::string&& ipAddress) {
	this->port = port;
	this->address = std::move(ipAddress);
}
void ServerManager::CreateServerLoop() {
	boost::asio::io_context io;
}
int ServerManager::GetPort() {
	return this->port;
}
std::string ServerManager::GetIpAddress() {
	return this->address;
}