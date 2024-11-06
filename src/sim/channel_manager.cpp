#include "sim/channel_manager.h"
#include <iostream>

std::atomic<bool> ChannelManager::name_attached{false};

ChannelManager::ChannelManager(const std::string& channel_name)
    : channel_name(channel_name), server_coid(-1) {
}

ChannelManager::~ChannelManager() {
    if (server_coid != -1) {
        name_detach(nullptr, 0);
    }
}

bool ChannelManager::initialize() {
    // Connect to the server channel
    server_coid = name_open(channel_name.c_str(), 0);
    if (server_coid == -1) {
        perror("ChannelManager: Failed to connect to server");
        return false;
    }
    return true;
}

bool ChannelManager::sendMessage(const ATCMessage& message) {
    if (server_coid == -1) {
        std::cerr << "ChannelManager: Server connection is not established." << std::endl;
        return false;
    }

    int result = MsgSend(server_coid, &message, sizeof(ATCMessage), nullptr, 0);
    if (result == -1) {
        perror("ChannelManager: Failed to send message");
        return false;
    }
    return true;
}

