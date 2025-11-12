#pragma once
#include <vector>
#include <string>
#include <boost/asio/ip/tcp.hpp>
#include "Server/MessageReceiver.h"

class FileTransferQueue;

enum class TextTypes : uint32_t
{
    Text = 0,
    File = 1,
    SendHistory = 2
};

class IMessage : public std::enable_shared_from_this<IMessage>
{
public:
    virtual ~IMessage() = default;

    /**
     * @brief Convert message to bytes to send over socket
     **/
    virtual std::vector<char> serialize() const = 0;

    /**
     * @brief Load message from bytes received
     **/
    virtual void deserialize(const std::vector<char>& data) = 0;

    /**
     * @brief Get a string value for the given message (could be anything)
     **/
    virtual std::string to_string() const = 0;

    /**
     * @brief Get a vector<char> object to send over the network.
     **/
    virtual std::vector<char> to_data_send() const = 0;
    /**
     * @brief Save file to desktop
     */
    virtual void save_file() const = 0;

    virtual void dispatch_send(
    const std::shared_ptr<boost::asio::ip::tcp::socket>& text_socket,
    std::shared_ptr<FileTransferQueue> file_queue,
    boost::system::error_code& ec) = 0;
};
