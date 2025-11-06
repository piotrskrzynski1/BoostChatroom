#pragma once
#include <iostream>
#include <boost/asio.hpp>
#include <memory>
#include <MessageTypes/Interface/IMessage.hpp>
#include <Server/MessageReciever.h>
class ClientServerManager {
private:
	MessageReciever messageReceiver_;
	void handle_connect(const boost::system::error_code& error,
										 const std::string& socket_name,
										 const std::shared_ptr<boost::asio::ip::tcp::socket>& socket);
	protected:
	boost::asio::ip::tcp::endpoint endpoint;
	boost::asio::ip::tcp::endpoint endpoint_file;
	std::shared_ptr<boost::asio::ip::tcp::socket> client_socket;
	std::shared_ptr<boost::asio::ip::tcp::socket> client_file_socket;

	public:
	void Disconnect() const;
	explicit ClientServerManager(boost::asio::io_context& io);
	void Message(TextTypes type, const std::shared_ptr<IMessage>& message) const;
};