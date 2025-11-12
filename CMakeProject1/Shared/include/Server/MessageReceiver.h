#pragma once
#include <boost/asio.hpp>
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>

// Forward declaration
class IMessage;
enum class TextTypes : uint32_t;

class MessageReceiver
{
public:
    // Generic callback that receives the raw message
    using MessageCallback = std::function<void(
        std::shared_ptr<boost::asio::ip::tcp::socket>,
        std::shared_ptr<IMessage>)>;

    void start_read_header(std::shared_ptr<boost::asio::ip::tcp::socket> socket,
                          const std::shared_ptr<std::string>& napis = nullptr);

    /**
     * @brief Register a callback for a specific message type
     * This is the NEW extensible way - no code changes needed for new message types!
     */
    void register_handler(TextTypes type, MessageCallback callback);


private:
    void start_read_body(const std::shared_ptr<std::vector<char>>& header_buffer,
                        uint64_t body_length,
                        const std::shared_ptr<boost::asio::ip::tcp::socket>& socket,
                        const std::shared_ptr<std::string>& napis);

    void handle_read_message(const std::shared_ptr<std::vector<char>>& buffer,
                            const boost::system::error_code& error,
                            std::size_t bytes_transferred,
                            const std::shared_ptr<boost::asio::ip::tcp::socket>& socket,
                            const std::shared_ptr<std::string>& napis);

    // Map of message type -> callback
    std::unordered_map<TextTypes, MessageCallback> handlers_;
};