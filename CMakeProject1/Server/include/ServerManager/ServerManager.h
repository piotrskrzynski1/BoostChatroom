#pragma once
#include <string>
#include <boost/asio.hpp>
#include <MessageTypes/Interface/IMessage.hpp>
#include <MessageTypes/TextMessage.h>
using boost::asio::ip::tcp;
class ServerManager {
	protected:
		unsigned int port;
		std::string address;
		boost::asio::io_context io_context;
	public:
		ServerManager(int port, std::string&& ipAddress);
		void StartServer();
		void SendMessage(std::shared_ptr<tcp::socket> socket, std::shared_ptr<IMessage> message, const boost::system::error_code& error);
		int GetPort();
		std::string GetIpAddress();
};