#pragma once
#include "MessageTypes/Interface/IMessage.hpp"

class SendHistoryMessage : public IMessage
{
private:
    unsigned short file_port_ = 0;  // Client's file socket port

public:
    SendHistoryMessage() = default;
    explicit SendHistoryMessage(unsigned short file_port) : file_port_(file_port) {}

    unsigned short get_file_port() const { return file_port_; }

    std::vector<char> serialize() const override;
    void deserialize(const std::vector<char>& data) override;
    std::string to_string() const override;
    std::vector<char> to_data_send() const override;
    void save_file() const override;
};