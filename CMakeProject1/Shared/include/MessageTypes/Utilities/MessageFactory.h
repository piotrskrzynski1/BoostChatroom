// MessageFactory.h
#pragma once
#include <memory>
#include <vector>
#include <cstdint>
#include <MessageTypes/Interface/IMessage.hpp>

class MessageFactory {
public:
    static std::unique_ptr<IMessage> create_from_id(uint32_t id);
};
