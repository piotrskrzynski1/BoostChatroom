// TextMessage.cpp
#include <MessageTypes/Text/TextMessage.h>
#include <iostream>
#include <boost/asio/buffer.hpp>

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include <MessageTypes/Utilities/HeaderHelper.hpp>

// Constructor
TextMessage::TextMessage(const std::string& text) : text_({text.begin(),text.end()})
{
    if (text_.size() > std::numeric_limits<uint64_t>::max()) {
        throw std::runtime_error("Text too long to serialize");
    }
}

// Serialize string into bytes
std::vector<char> TextMessage::serialize() const
{
    constexpr uint32_t id = static_cast<uint32_t>(TextTypes::Text);
    const uint64_t length = text_.size();

    const size_t total_size = sizeof(id) + sizeof(length) + length;

    std::vector<char> buffer;
    buffer.reserve(total_size);

    Utils::HeaderHelper::append_u32(buffer, id);
    Utils::HeaderHelper::append_u64(buffer, length);
    buffer.insert(buffer.end(), text_.begin(), text_.end());

    return buffer;
}


void TextMessage::deserialize(const std::vector<char>& data)
{
    if (data.size() < sizeof(uint32_t) + sizeof(uint64_t))
    {
        throw std::runtime_error("Message is too short in deserialzation");
    }
    if (data.size() < sizeof(uint32_t) + sizeof(uint64_t)) return;

    uint32_t id;
    Utils::HeaderHelper::read_u32(data, 0, id);

    uint64_t length = 0;
    Utils::HeaderHelper::read_u64(data, sizeof(uint32_t), length);

    size_t offset = sizeof(uint32_t) + sizeof(uint64_t);
    if (data.size() < offset + length) return;

    text_.assign(data.begin() + offset, data.begin() + offset + length);
}


// Return human-readable string
std::string TextMessage::to_string() const
{
    return {text_.data(),text_.size()};
}

std::vector<char> TextMessage::to_data_send() const
{
    return {text_.data(),text_.data()+text_.size()};

}

void TextMessage::save_file() const
{
}
