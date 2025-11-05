// TextMessage.cpp
#include "MessageTypes/TextMessage.h"

#include <cstdint>
#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif
// Constructor
TextMessage::TextMessage(const std::string& text) : text_(text)
{
}

// Serialize string into bytes
std::vector<char> TextMessage::serialize() const
{
    uint32_t id = (uint32_t)TextTypes::Text;
    uint32_t length = text_.size();

    id = htonl(id);
    length = htonl(length);

    size_t total_size = sizeof(id) + sizeof(length) + length;

    std::vector<char> buffer;
    buffer.reserve(total_size);

    const char* id_bytes = reinterpret_cast<const char*>(&id);
    buffer.insert(buffer.end(), id_bytes, id_bytes + sizeof(id));
    const char* length_bytes = reinterpret_cast<const char*>(&length);
    buffer.insert(buffer.end(), length_bytes, length_bytes + sizeof(length));
    buffer.insert(buffer.end(), text_.begin(), text_.end());
    return buffer;
}

void TextMessage::deserialize(const std::vector<char>& data)
{
    if (data.size() < (2 * sizeof(uint32_t))) return;


    uint32_t net_id;
    std::copy(data.begin(), data.begin() + sizeof(uint32_t), reinterpret_cast<char*>(&net_id));
    uint32_t id = ntohl(net_id);

    uint32_t net_length;
    std::copy(data.begin() + sizeof(uint32_t), data.begin() + (2 * sizeof(uint32_t)),
              reinterpret_cast<char*>(&net_length));
    uint32_t length = ntohl(net_length);

    size_t expected_data_start = 2 * sizeof(uint32_t);
    if (data.size() < expected_data_start + length) return;


    text_ = std::string(data.begin() + expected_data_start, data.begin() + expected_data_start + length);
}

// Return human-readable string
std::string TextMessage::to_string() const
{
    return text_;
}
