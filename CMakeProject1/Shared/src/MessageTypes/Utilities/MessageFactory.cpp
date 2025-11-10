#include "MessageTypes/Utilities/MessageFactory.h"

#include <MessageTypes/Text/TextMessage.h>
#include <MessageTypes/File/FileMessage.h>
#include <stdexcept>
#include <memory>

std::unique_ptr<IMessage> MessageFactory::create_from_id(TextTypes id)
{
    switch (const uint32_t int_id = static_cast<uint32_t>(id)) {
    case static_cast<uint32_t>(TextTypes::Text):
        return std::make_unique<TextMessage>();
    case static_cast<uint32_t>(TextTypes::File):
        return std::make_unique<FileMessage>();
    default:
        throw std::runtime_error("Unknown message type ID: " + std::to_string(int_id));
    }
}
