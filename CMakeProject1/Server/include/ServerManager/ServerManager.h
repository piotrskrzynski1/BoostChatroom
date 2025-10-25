#pragma once
#include <string>
#include <boost/asio.hpp>
using boost::asio::ip::tcp;
class ServerManager {
	private:
		void StartServer();
	protected:
		unsigned int port;
		std::string address;
		boost::asio::io_context io_context;
	public:
		ServerManager(int port, std::string&& ipAddress);
		void CreateServerLoop();
		void SendHelloMessage(std::shared_ptr<tcp::socket> socket, const boost::system::error_code& error);
		int GetPort();
		std::string GetIpAddress();
};