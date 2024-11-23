#include "communication/qnx_channel.h"
#include <sys/neutrino.h>
#include <sys/iofunc.h>
#include <sys/dispatch.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#include <fcntl.h>

namespace atc {
namespace comm {

QnxChannel::QnxChannel(const std::string& channel_name)
    : channel_name_(channel_name)
    , channel_id_(-1)
    , connection_id_(-1)
    , attach_ptr_(nullptr) {
}

QnxChannel::~QnxChannel() {
    cleanup();
}

bool QnxChannel::initialize() {
    std::lock_guard<std::mutex> lock(channel_mutex_);

    // First attempt to detach any existing name
    if (attach_ptr_) {
        name_detach(attach_ptr_, 0);
        attach_ptr_ = nullptr;
    }

    // Create the channel
    if (!createChannel()) {
        return false;
    }

    // Attach to our own channel as a client
    connection_id_ = ConnectAttach(0, 0, channel_id_, _NTO_SIDE_CHANNEL, 0);
    if (connection_id_ == -1) {
        std::cerr << "Failed to attach to channel: " << strerror(errno) << std::endl;
        cleanup();
        return false;
    }

    std::cout << "Successfully initialized communication channel" << std::endl;
    return true;
}

bool QnxChannel::createChannel() {
    std::cout << "Creating channel..." << std::endl;

    // Create a channel with fixed priority
    channel_id_ = ChannelCreate(_NTO_CHF_FIXED_PRIORITY);
    if (channel_id_ == -1) {
        std::cerr << "Failed to create channel: " << strerror(errno)
                  << " (errno: " << errno << ")" << std::endl;
        return false;
    }

    std::cout << "Channel created with ID: " << channel_id_ << std::endl;

    // Wait a bit to ensure any old channels are cleaned up
    usleep(100000);  // 100ms delay

    // Attach name to the channel
    attach_ptr_ = name_attach(nullptr, channel_name_.c_str(), channel_id_);
    if (attach_ptr_ == nullptr) {
        std::cerr << "Failed to attach name to channel: " << strerror(errno)
                  << " (errno: " << errno << ")" << std::endl;
        cleanup();
        return false;
    }

    std::cout << "Successfully attached name '" << channel_name_
              << "' to channel" << std::endl;
    return true;
}

void QnxChannel::cleanup() {
    if (connection_id_ != -1) {
        ConnectDetach(connection_id_);
        connection_id_ = -1;
    }

    if (attach_ptr_ != nullptr) {
        name_detach(attach_ptr_, 0);
        attach_ptr_ = nullptr;
    }

    if (channel_id_ != -1) {
        ChannelDestroy(channel_id_);
        channel_id_ = -1;
    }
}

bool QnxChannel::sendMessage(const Message& message) {
    std::lock_guard<std::mutex> lock(channel_mutex_);

    if (connection_id_ == -1) {
        return false;
    }

    int result = MsgSend(connection_id_, &message, sizeof(Message), nullptr, 0);
    if (result == -1) {
        if (errno != ETIMEDOUT) {
            std::cerr << "Failed to send message: " << strerror(errno) << std::endl;
        }
        return false;
    }

    return true;
}

bool QnxChannel::receiveMessage(Message& message, int timeout_ms) {
    std::lock_guard<std::mutex> lock(channel_mutex_);

    if (channel_id_ == -1) {
        return false;
    }

    _msg_info msg_info;
    int rcvid = MsgReceive(channel_id_, &message, sizeof(Message), &msg_info);

    if (rcvid == -1) {
        if (errno != ETIMEDOUT) {
            std::cerr << "Failed to receive message: " << strerror(errno) << std::endl;
        }
        return false;
    }

    MsgReply(rcvid, EOK, nullptr, 0);
    return true;
}

}
}
