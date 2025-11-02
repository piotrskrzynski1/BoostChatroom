#include <Server/MessageReciever.h>
#include <MessageTypes/TextMessage.h>
#include <cstring>
#include <iostream>

#ifdef _DEBUG
#define LOG(x) std::cout << "[DEBUG] " << x << std::endl
#else
#define LOG(x) ((void)0)
#endif

void MessageReciever::handle_read_text(std::shared_ptr<std::vector<char>> buffer,
    const boost::system::error_code& error,
    std::size_t bytes_transferred,
    std::shared_ptr<boost::asio::ip::tcp::socket> socket,
    std::shared_ptr<std::string> napis)
{
    if (!error)
    {
        buffer->resize(bytes_transferred);
        try {
            TextMessage received_msg;
            received_msg.deserialize(*buffer);
            std::string localText = received_msg.to_string();

            std::cout << localText << std::endl;

            if (on_message) {
                on_message(socket, localText);
            }
            else {
                std::cerr << "on_message not set" << std::endl;
            }
            if (napis) *napis = localText;
        }
        catch (const std::exception& e) {
            std::cerr << "Deserialization error: " << e.what() << std::endl;
        }

        this->start_read_header(socket);
    }
    else if (error == boost::asio::error::eof)
    {
        std::cout << "Server closed the connection.\n";
    }
    else
    {
        std::cerr << "Read error: " << error.message() << std::endl;
    }
}

void MessageReciever::start_read_header(std::shared_ptr<boost::asio::ip::tcp::socket> socket,
    std::shared_ptr<std::string> napis)
{
    const size_t header_size = sizeof(uint32_t) * 2;
    auto header_buffer = std::make_shared<std::vector<char>>(header_size);

    boost::asio::async_read(*socket, boost::asio::buffer(*header_buffer),
        [header_buffer, header_size, this, socket, napis](const boost::system::error_code& err, std::size_t bytes_transferred)
    {
        if (err)
        {
            handle_read_text(header_buffer, err, bytes_transferred, socket, napis);
            return;
        }

        uint32_t body_length = 0;
        if (bytes_transferred >= header_size) {
            std::memcpy(&body_length, header_buffer->data() + sizeof(uint32_t), sizeof(uint32_t));
            body_length = ntohl(body_length);
        }
        else {
            boost::system::error_code ec = boost::asio::error::operation_aborted;
            handle_read_text(header_buffer, ec, bytes_transferred, socket, napis);
            return;
        }

        if (body_length == 0)
        {
            handle_read_text(header_buffer, boost::system::error_code(), header_buffer->size(), socket, napis);
        }
        else
        {
            start_read_body(header_buffer, body_length, socket, napis);
        }
    });
}

void MessageReciever::start_read_body(std::shared_ptr<std::vector<char>> header_buffer,
    uint32_t body_length,
    std::shared_ptr<boost::asio::ip::tcp::socket> socket,
    std::shared_ptr<std::string> napis)
{
    auto body_buffer = std::make_shared<std::vector<char>>(body_length);

    boost::asio::async_read(*socket, boost::asio::buffer(*body_buffer),
        [header_buffer, body_buffer, this, socket, napis](const boost::system::error_code& err, std::size_t bytes_transferred)
    {
        if (err)
        {
            handle_read_text(body_buffer, err, bytes_transferred, socket, napis);
            return;
        }

        auto fulldata = std::make_shared<std::vector<char>>(*header_buffer);
        fulldata->insert(fulldata->end(), body_buffer->begin(), body_buffer->begin() + bytes_transferred);
        handle_read_text(fulldata, boost::system::error_code(), fulldata->size(), socket, napis);
    });
}

void MessageReciever::set_on_message_callback(std::function<void(std::shared_ptr<boost::asio::ip::tcp::socket>, const std::string&)> callback) {
    on_message = std::move(callback);
};
