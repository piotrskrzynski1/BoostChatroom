#pragma once
#include <string>
#include <boost/asio.hpp>
#include <MessageTypes/Interface/IMessage.hpp>
#include <mutex>
#include <vector>
#include <Server/MessageReceiver.h>
#include <unordered_map>
#include <memory>
#include <filesystem>
#include <MessageTypes/Utilities/FileTransferQueue.h>

using boost::asio::ip::tcp;

class ServerManager
{
    friend class TestableServerManager;

private:
    static constexpr size_t MAX_HISTORY_MESSAGES = 100;

    using HistoryMessage = std::shared_ptr<IMessage>;

    //history for the current chatroom to send to a textsocket after someone joins
    std::deque<HistoryMessage> message_history_;
    std::mutex history_mutex_;

    //threads used to run the multithreaded IO_Context
    std::vector<std::thread> threads;
    std::vector<std::shared_ptr<tcp::socket>> text_port_clients_;
    std::vector<std::shared_ptr<tcp::socket>> file_port_clients_;
    std::mutex text_port_clients_mutex_;
    std::mutex file_port_clients_mutex_;
    //helper classes that recieve and parse data from sockets
    MessageReceiver messageReciever_;
    MessageReceiver fileReciever;

    int port;
    int fileport;
    std::string address;
    boost::asio::io_context io_context;
    std::shared_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
    std::shared_ptr<boost::asio::ip::tcp::acceptor> file_acceptor_;

    // per-file-client transfer queues
    std::unordered_map<std::uintptr_t, std::shared_ptr<FileTransferQueue>> file_queues_;
    std::mutex file_queues_mutex_;

    //server status
    bool serverup_ = false;

    /**
    * @brief Starts accepting file messages via an acceptor, text connection wrapper for the AcceptConnection() method
    **/
    void AcceptTextConnection(const std::shared_ptr<tcp::acceptor>& acceptor);
    /**
   * @brief Starts accepting file messages via an acceptor, file connection wrapper for the AcceptConnection() method
   **/
    void AcceptFileConnection(const std::shared_ptr<tcp::acceptor>& acceptor);

    // helpers for per-client file queues
    std::shared_ptr<FileTransferQueue> GetOrCreateFileQueueForSocket(const std::shared_ptr<tcp::socket>& sock);
    void RemoveFileQueueForSocket(const std::shared_ptr<tcp::socket>& sock);
    void SetStatusUP(bool status);

    /**
    *  @brief Broadcasts a specific text message to every client connected to the chatroom except the sender
    **/
    void Broadcast(const std::shared_ptr<tcp::socket>& sender, const std::string& text);
    /**
    *  @brief Broadcasts a specific file message to every client connected to the chatroom except the sender
    **/
    void Broadcast(const std::shared_ptr<tcp::socket>& sender, const std::shared_ptr<std::vector<char>>& rawData);

public:
    std::string GetIpAddress();

    ServerManager(int port, int fileport, std::string&& ipAddress);
    void StartServer();
    void StopServer();
    static std::string GetSocketIP(const std::shared_ptr<tcp::socket>& sock);
    /**
 * @brief Asynchronously accept one incoming connection, register it, start receiving messages/files,
 *        optionally send a greeting + recent message history, and re-arm the acceptor.
 *
 * This function calls async_accept(...) on `acceptor` and installs a handler that:
 *  - validates the accept result and ignores expected shutdown errors,
 *  - deduplicates and adds the accepted socket to `client_list` (protected by `client_list_mutex`),
 *  - for file clients, creates/gets a per-socket FileTransferQueue,
 *  - starts the receiver (calls receiver.start_read_header),
 *  - optionally sends a greeting and the message history to the new client when `sendGreeting` is true,
 *  - pushes file/text entries into message_history_ where appropriate,
 *  - re-arms itself (calls AcceptConnection again) unless io_context was stopped or acceptor shutdown is detected.
 *
 * Thread-safety: caller must provide the correct mutex that protects `client_list`. `acceptor` and `receiver`
 * must outlive the asynchronous handler.
 *
 * @param acceptor            shared_ptr<tcp::acceptor> Acceptor to accept the connection on (must remain valid).
 * @param client_list         std::vector<std::shared_ptr<tcp::socket>>&
 *                            Container holding connected client sockets; the accepted socket will be added here
 *                            (guarded by client_list_mutex).
 * @param client_list_mutex   std::mutex& Mutex protecting `client_list`.
 * @param receiver            MessageReciever& The object responsible for parsing incoming data from the socket;
 *                            receiver.start_read_header(...) is invoked for the new socket.
 * @param sendGreeting        bool If true, send a hello message and the stored message history to the newly
 *                            connected socket (used for text clients; typically false for file-only clients).
 * @param clientType          const TextTypes& Enum/value describing the client type (e.g., Text vs File).
 *                            Used by the handler to perform client-type-specific logic (e.g. create a file queue).
 */
    void AcceptConnection(const std::shared_ptr<tcp::acceptor>& acceptor,
                          std::vector<std::shared_ptr<tcp::socket>>& client_list, std::mutex& client_list_mutex,
                          MessageReceiver& receiver, bool sendGreeting, const TextTypes& clientType);
    int GetPort() const;
    int GetFilePort() const;
    bool GetStatusUP() const;
};
