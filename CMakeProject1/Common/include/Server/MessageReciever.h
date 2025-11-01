#pragma once
#include <iostream>
#include <memory>
#include <boost/asio.hpp>

class MessageReciever {
private:
	void start_read_body(std::shared_ptr<std::vector<char>> header_buffer, uint32_t body_length, std::shared_ptr<boost::asio::ip::tcp::socket> socket);
	void handle_read_text(std::shared_ptr<std::vector<char>> buffer,
			const boost::system::error_code& error,
			std::size_t bytes_transferred, std::shared_ptr<boost::asio::ip::tcp::socket> socket);
public:
			void start_read_header(std::shared_ptr<boost::asio::ip::tcp::socket> socket);
	
};