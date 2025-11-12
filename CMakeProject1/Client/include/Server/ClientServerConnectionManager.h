#pragma once
#include <iostream>
#include <boost/asio.hpp>
#include <memory>
#include <functional> //  For std::function
#include <MessageTypes/Interface/IMessage.hpp>
#include <Server/MessageReceiver.h>
#include <MessageTypes/Utilities/FileTransferQueue.h>
#include <MessageTypes/File/FileMessage.h> // For the callback signature

class ClientServerConnectionManager : public std::enable_shared_from_this<ClientServerConnectionManager>
{
private:
    MessageReceiver textMessageReceiver_;
    MessageReceiver fileMessageReceiver_;

    std::shared_ptr<FileTransferQueue> file_queue_;

    /**
     * @brief Handles connection to a specified socket
     * @param error boost asio errors
     * @param socket_name a unique socket name
     * @param socket a boost asio socket to connect with
     */
    void handle_connect(const boost::system::error_code& error,
                        const std::string& socket_name,
                        const std::shared_ptr<boost::asio::ip::tcp::socket>& socket);



protected:
    boost::asio::ip::tcp::endpoint endpoint;
    boost::asio::ip::tcp::endpoint endpoint_file;
    std::shared_ptr<boost::asio::ip::tcp::socket> client_socket;
    std::shared_ptr<boost::asio::ip::tcp::socket> client_file_socket;

public:
    explicit ClientServerConnectionManager(boost::asio::io_context& io, std::string ip, unsigned short int textport,
                                 unsigned short int fileport);

    void Disconnect() const;
    void Message(TextTypes type, const std::shared_ptr<IMessage>& message) const;

    // --- File Queue API ---
    /**
     * @brief Enqueue a file into the FileTransferQueue queue.
     **/
    uint64_t EnqueueFile(const std::filesystem::path& path) const;
    /**
     * @brief History of the file queue
     **/
    std::vector<FileTransferQueue::Item> FileQueueSnapshot() const;

    /**
     * @brief A helper function to shutdown and reconnect the file socket
     **/
    void CancelAndReconnectFileSocket();
    boost::asio::io_context& io_context_;

    // --- Queue control commands ---
    void PauseQueue() const;
    void ResumeQueue() const;
    /**
     * @brief Cancel sending a specific file with the FileTransferQueue id
     * @param id id of the file held by the FileTransferQueue
     **/
    void CancelFile(uint64_t id) const;
    /**
    * @brief Retry sending a specific file with the id
    * @param id id of the file held by the FileTransferQueue
    **/
    void RetryFile(uint64_t id) const;
};