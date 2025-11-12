#include <Server/MessageReceiver.h>
#include <MessageTypes/Text/TextMessage.h>
#include <MessageTypes/File/FileMessage.h>
#include <cstring>
#include <iostream>
#include <MessageTypes/Utilities/HeaderHelper.hpp>
#include <MessageTypes/Utilities/MessageFactory.h>

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
                                      const std::shared_ptr<boost::asio::ip::tcp::socket>& socket)
{
    auto body_buffer = std::make_shared<std::vector<char>>(static_cast<size_t>(body_length));

    boost::asio::async_read(*socket, boost::asio::buffer(*body_buffer),
        [header_buffer, body_buffer, this, socket](const boost::system::error_code& err, std::size_t bytes_transferred)
        {
            if (err)
            {
                handle_read_message(body_buffer, err, bytes_transferred, socket);
                return;
            }

            auto fulldata = std::make_shared<std::vector<char>>(*header_buffer);
            fulldata->insert(fulldata->end(), body_buffer->begin(),
                             body_buffer->begin() + bytes_transferred);

            handle_read_message(fulldata, boost::system::error_code(), fulldata->size(), socket);
        });
}

void MessageReceiver::handle_read_message(
    const std::shared_ptr<std::vector<char>>& buffer,
    const boost::system::error_code& error,
    std::size_t bytes_transferred,
    const std::shared_ptr<boost::asio::ip::tcp::socket>& socket
    )
{
    if (error) {
        if (error == boost::asio::error::eof) {
            std::cout << "Client closed the connection.\n";
            return;
        }
        if (error == boost::asio::error::operation_aborted)
        {
            std::cout << "connection canceled.\n" << std::endl;
            return;
        }
        if (error) {
            std::cerr << "Read error: " << error.message() << std::endl;
            return;
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
        const std::vector<char> data(buffer->begin(), buffer->begin() + bytes_transferred);

        // Deserialize
        message->deserialize(data);

        // Look up and invoke the registered handler for this message type
        auto it = handlers_.find(type);
        if (it != handlers_.end() && it->second) {
            // Convert unique_ptr to shared_ptr for the callback
            std::shared_ptr<IMessage> msg_shared(message.release());
            it->second(socket, msg_shared);
        } else {
            #ifdef _DEBUG
            std::cerr << "No handler registered for message type: " << id << std::endl;
            #endif
        }

    }
    catch (const std::exception& e) {
        std::cerr << "Deserialization error: " << e.what() << std::endl;
    }

    this->start_read_header(socket);
}


void MessageReceiver::start_read_header(std::shared_ptr<boost::asio::ip::tcp::socket> socket)
{
    // TODO potential spam and memory overflow (we dont limit message size) (TODO also in FileMessage.cpp)
    constexpr size_t header_size = sizeof(uint32_t) + sizeof(uint64_t);
    auto header_buffer = std::make_shared<std::vector<char>>(header_size);

    boost::asio::async_read(*socket, boost::asio::buffer(*header_buffer),
        [header_buffer, this, socket](const boost::system::error_code& err, std::size_t bytes_transferred)
        {
            if (err) {
                handle_read_message(header_buffer, err, bytes_transferred, socket);
                return;
            }

            if (bytes_transferred < header_size) {
                boost::system::error_code ec = boost::asio::error::operation_aborted;
                handle_read_message(header_buffer, ec, bytes_transferred, socket);
                return;
            }

            uint32_t id;
            uint64_t body_length;
            std::memcpy(&id, header_buffer->data(), sizeof(uint32_t));
            id = ntohl(id);
            std::memcpy(&body_length, header_buffer->data() + sizeof(uint32_t), sizeof(uint64_t));
            body_length = ntohll(body_length);   // 64-bit conversion

            if (body_length != 0) {
                start_read_body(header_buffer, body_length, socket);
                return;
            }

            handle_read_message(header_buffer, boost::system::error_code(),
                                header_buffer->size(), socket);
        });
}

void MessageReceiver::register_handler(TextTypes type, MessageCallback callback)
{
    handlers_[type] = std::move(callback);
}
