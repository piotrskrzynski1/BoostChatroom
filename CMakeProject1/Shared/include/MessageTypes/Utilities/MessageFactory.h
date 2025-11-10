// MessageFactory.h
#pragma once
#include <memory>
#include <vector>
#include <cstdint>
#include <MessageTypes/Interface/IMessage.hpp>

class MessageFactory {
public:
    /**
     * @brief creates a message from a given id
     * @param id Message::TextTypes id
     */
    static std::unique_ptr<IMessage> create_from_id(TextTypes id);
};
