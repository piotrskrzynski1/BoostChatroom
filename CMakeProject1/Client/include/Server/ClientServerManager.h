#pragma once
#include <iostream>
#include <boost/asio.hpp>
#include <memory>
#include <MessageTypes/Interface/IMessage.hpp>
#include <Server/MessageReciever.h>
class ClientServerManager {
private:
	MessageReciever messageReciever_;
	void handle_connect(const boost::system::error_code& error);
	protected:
	boost::asio::ip::tcp::endpoint endpoint;
	std::shared_ptr<boost::asio::ip::tcp::socket> client_socket;

	public:
	void Disconnect();
	ClientServerManager(boost::asio::io_context& io);
	void Message(std::shared_ptr<IMessage> message);
};