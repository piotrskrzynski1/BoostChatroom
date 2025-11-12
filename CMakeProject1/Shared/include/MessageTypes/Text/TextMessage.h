#pragma once
#include "MessageTypes/Interface/IMessage.hpp"
#include <string>
#include <vector>

#include "Server/MessageSender.h"


class TextMessage : public IMessage
{
public:
    TextMessage() = default;
    explicit TextMessage(const std::string& text);

    std::vector<char> serialize() const override;
    void deserialize(const std::vector<char>& data) override;
    std::string to_string() const override;

    std::vector<char> to_data_send() const override;
    void save_file() const override;

    void dispatch_send(
    const std::shared_ptr<boost::asio::ip::tcp::socket>& text_socket,
    std::shared_ptr<FileTransferQueue> file_queue,
    boost::system::error_code& ec) override;
private:
    std::vector<char> text_;
};
