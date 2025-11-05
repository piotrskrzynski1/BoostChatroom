#pragma once
#include <vector>
#include <string>
#include <cstdint>

enum class TextTypes : uint32_t {
    Text,
    Image
};

class IMessage {
public:
    virtual ~IMessage() = default;

    // Convert message to bytes to send over socket
    virtual std::vector<char> serialize() const = 0;

    // Load message from bytes received
    virtual void deserialize(const std::vector<char>& data) = 0;

    // Optional: get a human-readable representation
    virtual std::string to_string() const = 0;
};