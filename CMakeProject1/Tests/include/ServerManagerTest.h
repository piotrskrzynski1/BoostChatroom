#include "Server/ServerManager.h"

class TestableServerManager : public ServerManager {
public:
    using ServerManager::Broadcast;
    using ServerManager::ServerManager;

};