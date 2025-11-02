#pragma once
#include <string>
#include <boost/asio.hpp>
#include <MessageTypes/Interface/IMessage.hpp>
#include <MessageTypes/TextMessage.h>
#include <mutex>
#include <vector>
#include <Server/MessageReciever.h>
using boost::asio::ip::tcp;
class ServerManager {

	private:
		std::vector<std::shared_ptr<tcp::socket>> clients_;
		std::mutex clients_mutex_;
		MessageReciever messageReciever_;
		void DoAccept(std::shared_ptr<tcp::acceptor> acceptor);
		void Broadcast(std::shared_ptr<tcp::socket> sender, const std::string& text);
	protected:
		unsigned int port;

		std::string address;
		boost::asio::io_context io_context;
	public:
		ServerManager(int port, std::string&& ipAddress);
		void StartServer();
		int GetPort();
		std::string GetIpAddress();
};