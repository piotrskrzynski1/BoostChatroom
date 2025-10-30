#pragma once
#include <string>
#include <boost/asio.hpp>
#include <MessageTypes/Interface/IMessage.hpp>
#include <MessageTypes/TextMessage.h>
using boost::asio::ip::tcp;
class ServerManager {
	private:
		void DoAccept(std::shared_ptr<tcp::acceptor> acceptor);
	protected:
		unsigned int port;

		std::string address;
		boost::asio::io_context io_context;
		void SendMessage(std::shared_ptr<tcp::socket> socket, std::shared_ptr<IMessage> message, const boost::system::error_code& error);
	public:
		ServerManager(int port, std::string&& ipAddress);
		void StartServer();
		int GetPort();
		std::string GetIpAddress();
};