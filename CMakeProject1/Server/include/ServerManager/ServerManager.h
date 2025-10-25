#pragma once
#include <string>
class ServerManager {
	private:
		int port;
		std::string address;
	public:
		ServerManager(int port, std::string&& ipAddress);
		void CreateServerLoop();
		int GetPort();
		std::string GetIpAddress();
};