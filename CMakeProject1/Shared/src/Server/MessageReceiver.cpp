#include <Server/MessageReceiver.h>
#include <MessageTypes/Text/TextMessage.h>
#include <cstring>
#include <iostream>
#include <MessageTypes/Utilities/HeaderHelper.hpp>
#include <MessageTypes/Utilities/MessageFactory.h>
#include "MessageTypes/File/FileMessage.h"

#ifdef _DEBUG
#define LOG(x) std::cout << "[DEBUG] " << x << std::endl
#else
#define LOG(x) ((void)0)
#endif

// --- helper for 64-bit endianness ---
static uint64_t ntohll(const uint64_t value)
{
    static constexpr int num = 1;
    if (*reinterpret_cast<const char*>(&num) == 1)
    {
        // little endian → swap 32-bit halves
        return (static_cast<uint64_t>(ntohl(static_cast<uint32_t>(value & 0xFFFFFFFFULL))) << 32) |
               ntohl(static_cast<uint32_t>(value >> 32));
    }
    return value; // already big-endian
}

// ------------------------------------------------------------------

void MessageReceiver::start_read_body(const std::shared_ptr<std::vector<char>>& header_buffer,
                                      uint64_t body_length,
                                      const std::shared_ptr<boost::asio::ip::tcp::socket>& socket,
                                      const std::shared_ptr<std::string>& napis)
{
    auto body_buffer = std::make_shared<std::vector<char>>(static_cast<size_t>(body_length));

    boost::asio::async_read(*socket, boost::asio::buffer(*body_buffer),
        [header_buffer, body_buffer, this, socket, napis](const boost::system::error_code& err, std::size_t bytes_transferred)
        {
            if (err)
            {
                handle_read_message(body_buffer, err, bytes_transferred, socket, napis);
                return;
            }

            auto fulldata = std::make_shared<std::vector<char>>(*header_buffer);
            fulldata->insert(fulldata->end(), body_buffer->begin(),
                             body_buffer->begin() + bytes_transferred);

            handle_read_message(fulldata, boost::system::error_code(), fulldata->size(), socket, napis);
        });
}

void MessageReceiver::handle_read_message(
    const std::shared_ptr<std::vector<char>>& buffer,
    const boost::system::error_code& error,
    std::size_t bytes_transferred,
    const std::shared_ptr<boost::asio::ip::tcp::socket>& socket,
    const std::shared_ptr<std::string>& napis)
{
    if (error) {
        if (error == boost::asio::error::eof) {
            std::cout << "Client closed the connection.\n";
        } else if (error == boost::asio::error::operation_aborted)
        {
            std::cout << "connection canceled.\n" << std::endl;
        }
        else {
            std::cerr << "Read error: " << error.message() << std::endl;
        }

        if (socket->is_open())
        {
            boost::system::error_code ec;
            socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
            socket->close(ec);
            if (ec)
            {
                std::cerr << ec.what() << std::endl;
            }
        }
        return;
    }

    try {
        uint32_t id;
        Utils::HeaderHelper::read_u32(*buffer, 0, id);

        // Create the correct message type
        auto type = static_cast<TextTypes>(id);
        std::unique_ptr<IMessage> message = MessageFactory::create_from_id(type);
        // Use only the bytes actually received
        std::vector<char> data(buffer->begin(), buffer->begin() + bytes_transferred);
        // Deserialize
        message->deserialize(data);



        // Print and save depending on message type
        if (auto textMsg = dynamic_cast<TextMessage*>(message.get())) {
            std::string msg_str = textMsg->to_string();
            std::cout << msg_str << std::endl;
            if (on_message_text) on_message_text(socket, msg_str);
        }
        else if (auto fileMsg = dynamic_cast<FileMessage*>(message.get())) {

            std::cout << fileMsg->to_string() << std::endl;
            if (on_message_file) {
                auto msg_bytes = std::make_shared<std::vector<char>>(message->serialize());
                on_message_file(socket, msg_bytes);
            }
        }
        else {
            std::cerr << "Unknown message type after factory: ID " << id << std::endl;
        }

        if (napis)
            *napis = std::string(buffer->begin(), buffer->end());
    }
    catch (const std::exception& e) {
        std::cerr << "Deserialization error: " << e.what() << std::endl;
    }

    this->start_read_header(socket);
}


void MessageReceiver::start_read_header(std::shared_ptr<boost::asio::ip::tcp::socket> socket,
                                        const std::shared_ptr<std::string>& napis)
{
    // TODO potential spam and memory overflow (we dont limit message size) (TODO also in FileMessage.cpp)
    constexpr size_t header_size = sizeof(uint32_t) + sizeof(uint64_t);
    auto header_buffer = std::make_shared<std::vector<char>>(header_size);

    boost::asio::async_read(*socket, boost::asio::buffer(*header_buffer),
        [header_buffer, this, socket, napis](const boost::system::error_code& err, std::size_t bytes_transferred)
        {
            if (err) {
                handle_read_message(header_buffer, err, bytes_transferred, socket, napis);
                return;
            }

            if (bytes_transferred < header_size) {
                boost::system::error_code ec = boost::asio::error::operation_aborted;
                handle_read_message(header_buffer, ec, bytes_transferred, socket, napis);
                return;
            }

            uint32_t id;
            uint64_t body_length;
            std::memcpy(&id, header_buffer->data(), sizeof(uint32_t));
            id = ntohl(id);
            std::memcpy(&body_length, header_buffer->data() + sizeof(uint32_t), sizeof(uint64_t));
            body_length = ntohll(body_length);   // 64-bit conversion

            if (body_length != 0) {
                start_read_body(header_buffer, body_length, socket, napis);
                return;
            }

            handle_read_message(header_buffer, boost::system::error_code(),
                                header_buffer->size(), socket, napis);
        });
}

void MessageReceiver::set_on_message_callback(
    std::function<void(std::shared_ptr<boost::asio::ip::tcp::socket>, const std::string&)> callback)
{
    on_message_text = std::move(callback);
}

void MessageReceiver::set_on_file_callback(
    std::function<void(std::shared_ptr<boost::asio::ip::tcp::socket>, const std::shared_ptr<std::vector<char>>&)> callback)
{
    on_message_file = std::move(callback);
}
