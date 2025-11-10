#pragma once
#include <string>
#include <boost/asio.hpp>
#include <MessageTypes/Interface/IMessage.hpp>
#include <mutex>
#include <vector>
#include <Server/MessageReciever.h>
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

    std::deque<HistoryMessage> message_history_;
    std::mutex history_mutex_;

    std::vector<std::thread> threads;
    std::vector<std::shared_ptr<tcp::socket>> text_port_clients_;
    std::vector<std::shared_ptr<tcp::socket>> file_port_clients_;
    std::mutex text_port_clients_mutex_;
    std::mutex file_port_clients_mutex_;
    MessageReciever messageReciever_;
    MessageReciever fileReciever;

    // per-file-client transfer queues
    std::unordered_map<std::uintptr_t, std::shared_ptr<FileTransferQueue>> file_queues_;
    std::mutex file_queues_mutex_;

    bool serverup_ = false;

    void AcceptTextConnection(const std::shared_ptr<tcp::acceptor>& acceptor);
    void AcceptFileConnection(const std::shared_ptr<tcp::acceptor>& acceptor);
    void Broadcast(const std::shared_ptr<tcp::socket>& sender, const std::string& text);

    // helpers for per-client file queues
    std::shared_ptr<FileTransferQueue> GetOrCreateFileQueueForSocket(const std::shared_ptr<tcp::socket>& sock);
    void RemoveFileQueueForSocket(const std::shared_ptr<tcp::socket>& sock);
    void SetStatusUP(bool status);

protected:
    int port;
    int fileport;
    std::string address;
    boost::asio::io_context io_context;
    std::shared_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
    std::shared_ptr<boost::asio::ip::tcp::acceptor> file_acceptor_;
    void Broadcast(const std::shared_ptr<tcp::socket>& sender, const std::shared_ptr<std::vector<char>>& rawData);

public:
    std::string GetIpAddress();

    ServerManager(int port, int fileport, std::string&& ipAddress);
    void StartServer();
    void StopServer();
    static std::string GetSocketIP(const std::shared_ptr<tcp::socket>& sock);
    void AcceptConnection(const std::shared_ptr<tcp::acceptor>& acceptor,
                          std::vector<std::shared_ptr<tcp::socket>>& client_list, std::mutex& client_list_mutex,
                          MessageReciever& receiver, bool sendGreeting, const TextTypes& clientType);
    int GetPort() const;
    int GetFilePort() const;
    bool GetStatusUP() const;
};
