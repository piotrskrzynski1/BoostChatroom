#pragma once
#include "MessageTypes/Interface/IMessage.hpp"
#include <string>
#include <vector>


class TextMessage final : public IMessage {
public:
    TextMessage() = default;
    explicit TextMessage(const std::string& text);

    [[nodiscard]] std::vector<char> serialize() const override;
    void deserialize(const std::vector<char>& data) override;
    [[nodiscard]] std::string to_string() const override;

    [[nodiscard]] std::vector<char> to_data_send() const override;
private:
    std::vector<char> text_;
};
