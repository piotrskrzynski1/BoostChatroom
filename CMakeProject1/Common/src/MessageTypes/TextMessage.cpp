// TextMessage.cpp
#include "MessageTypes/TextMessage.h"

// Constructor
TextMessage::TextMessage(const std::string& text) : text_(text)
{
}

// Serialize string into bytes
std::vector<char> TextMessage::serialize() const {
    return std::vector<char>(text_.begin(), text_.end());
}

// Deserialize bytes into string
void TextMessage::deserialize(const std::vector<char>& data) {
    text_ = std::string(data.begin(), data.end());
}

// Return human-readable string
std::string TextMessage::to_string() const {
    return text_;
}
