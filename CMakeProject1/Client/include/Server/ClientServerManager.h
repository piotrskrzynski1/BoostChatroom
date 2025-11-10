#pragma once
#include <iostream>
#include <boost/asio.hpp>
#include <memory>
#include <functional> //  For std::function
#include <MessageTypes/Interface/IMessage.hpp>
#include <Server/MessageReciever.h>
#include <MessageTypes/Utilities/FileTransferQueue.h>
#include <MessageTypes/File/FileMessage.h> // For the callback signature

class ClientServerManager : public std::enable_shared_from_this<ClientServerManager>
{
private:
    MessageReciever textMessageReceiver_;
    MessageReciever fileMessageReceiver_;

    std::shared_ptr<FileTransferQueue> file_queue_;

    void handle_connect(const boost::system::error_code& error,
                        const std::string& socket_name,
                        const std::shared_ptr<boost::asio::ip::tcp::socket>& socket);

    std::function<void(const std::string&)> on_text_message_;
    std::function<void(std::shared_ptr<FileMessage>)> on_file_message_;

protected:
    boost::asio::ip::tcp::endpoint endpoint;
    boost::asio::ip::tcp::endpoint endpoint_file;
    std::shared_ptr<boost::asio::ip::tcp::socket> client_socket;
    std::shared_ptr<boost::asio::ip::tcp::socket> client_file_socket;

public:
    explicit ClientServerManager(boost::asio::io_context& io, std::string ip, unsigned short int textport,
                                 unsigned short int fileport);

    void Disconnect() const;
    void Message(TextTypes type, const std::shared_ptr<IMessage>& message) const;

    // --- File Queue API ---
    uint64_t EnqueueFile(const std::filesystem::path& path) const;
    std::vector<FileTransferQueue::Item> FileQueueSnapshot() const;

    void CancelAndReconnectFileSocket();
    boost::asio::io_context& io_context_;

    // --- Queue control commands ---
    void PauseQueue() const;
    void ResumeQueue() const;
    void CancelFile(uint64_t id) const;
    void RetryFile(uint64_t id) const;

    void set_on_text_message_callback(std::function<void(const std::string&)> cb);
    void set_on_file_message_callback(std::function<void(std::shared_ptr<FileMessage>)> cb);
};