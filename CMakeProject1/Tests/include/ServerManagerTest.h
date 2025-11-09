#pragma once
#include "Server/ServerManager.h"

class TestableServerManager : public ServerManager {
public:
    using ServerManager::Broadcast;  // expose private functions

    template<typename... Args>
    explicit TestableServerManager(Args&&... args)
        : ServerManager(std::forward<Args>(args)...) {}

};
