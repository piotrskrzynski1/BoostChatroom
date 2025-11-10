#pragma once
#include <cstdint>
#include <vector>
#include <string>

enum class TextTypes : uint32_t
{
    Text = 0,
    File = 1,
};

class IMessage
{
public:
    virtual ~IMessage() = default;

    // Convert message to bytes to send over socket
    virtual std::vector<char> serialize() const = 0;

    // Load message from bytes received
    virtual void deserialize(const std::vector<char>& data) = 0;

    // Optional: get a human-readable representation
    virtual std::string to_string() const = 0;

    virtual std::vector<char> to_data_send() const = 0;
    virtual void save_file() const = 0;
};
