#pragma once
#include "MessageTypes/Interface/IMessage.hpp"
#include <string>
#include <vector>


class TextMessage final : public IMessage
{
public:
    TextMessage() = default;
    explicit TextMessage(const std::string& text);

    std::vector<char> serialize() const override;
    void deserialize(const std::vector<char>& data) override;
    std::string to_string() const override;

    std::vector<char> to_data_send() const override;
    void save_file() const override;

private:
    std::vector<char> text_;
};
