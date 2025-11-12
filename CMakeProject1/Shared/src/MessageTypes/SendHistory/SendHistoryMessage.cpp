#include "MessageTypes/SendHistory/SendHistoryMessage.h"
#include <stdexcept>
#include "MessageTypes/Utilities/HeaderHelper.hpp"

// In MessageTypes/SendHistory/SendHistoryMessage.cpp
// In MessageTypes/SendHistory/SendHistoryMessage.cpp

std::vector<char> SendHistoryMessage::serialize() const
{
    constexpr uint32_t id = static_cast<uint32_t>(TextTypes::SendHistory);

    // Use sizeof(uint32_t) for the payload length
    // because we must use append_u32() to send the port.
    const uint64_t payload_length = sizeof(uint32_t);

    std::vector<char> buffer;
    buffer.reserve(sizeof(id) + sizeof(payload_length) + payload_length);

    Utils::HeaderHelper::append_u32(buffer, id);
    Utils::HeaderHelper::append_u64(buffer, payload_length);

    // casted to uint32_t, padded with two zero bytes.
    Utils::HeaderHelper::append_u32(buffer, static_cast<uint32_t>(file_port_));

    return buffer;
}

void SendHistoryMessage::deserialize(const std::vector<char>& data)
{
    // Check minimum size based on new payload size (uint32_t)
    if (data.size() < sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint32_t))
        throw std::runtime_error("SendHistoryMessage: message too short");

    size_t offset = 0;

    uint32_t id = 0;
    Utils::HeaderHelper::read_u32(data, offset, id);
    offset += sizeof(uint32_t);

    if (id != static_cast<uint32_t>(TextTypes::SendHistory))
        throw std::runtime_error("SendHistoryMessage: wrong message id");

    uint64_t payload_length = 0;
    Utils::HeaderHelper::read_u64(data, offset, payload_length);
    offset += sizeof(uint64_t);

    // The expected payload length is now sizeof(uint32_t).
    if (payload_length != sizeof(uint32_t))
        throw std::runtime_error("SendHistoryMessage: unexpected payload length");

    //  Read the entire 4-byte port container using read_u32
    uint32_t port_container = 0;
    Utils::HeaderHelper::read_u32(data, offset, port_container);
    offset += sizeof(uint32_t);

    // The port (uint16_t) is contained in the lower 16 bits of the uint32_t.
    // Assign the result back to the uint16_t member variable.
    file_port_ = static_cast<uint16_t>(port_container);

}

std::string SendHistoryMessage::to_string() const
{
    return "[SendHistory from file port: " + std::to_string(file_port_) + "]";
}

std::vector<char> SendHistoryMessage::to_data_send() const
{
    return {};
}

void SendHistoryMessage::save_file() const
{
    // nic a nic
}