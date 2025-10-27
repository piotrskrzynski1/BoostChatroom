#pragma once
#include "MessageTypes/Interface/IMessage.hpp"
#include <string>
#include <vector>


class TextMessage : public IMessage {
public:
    TextMessage() = default;
    TextMessage(const std::string& text);

    std::vector<char> serialize() const override;
    void deserialize(const std::vector<char>& data) override;
    std::string to_string() const override;

private:
    std::string text_;
};
