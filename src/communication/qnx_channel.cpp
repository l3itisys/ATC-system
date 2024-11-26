#include "communication/qnx_channel.h"
#include "common/logger.h"
#include <sys/neutrino.h>
#include <sys/iofunc.h>
#include <sys/dispatch.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <cstring>

namespace atc {
namespace comm {

QnxChannel::QnxChannel(const std::string& channel_name)
    : channel_name_(channel_name)
    , channel_id_(-1)
    , connection_id_(-1)
    , attach_ptr_(nullptr)
    , receive_thread_running_(false)
    , is_server_(false) {

    Logger::getInstance().log("Creating QNX channel: " + channel_name_);
}

QnxChannel::~QnxChannel() {
    cleanup();
}

bool QnxChannel::initialize(bool as_server) {
    std::lock_guard<std::mutex> lock(channel_mutex_);
    is_server_ = as_server;

    try {
        if (is_server_) {
            return initializeServer();
        } else {
            return initializeClient();
        }
    } catch (const std::exception& e) {
        Logger::getInstance().log("Failed to initialize channel: " + std::string(e.what()));
        cleanup();
        return false;
    }
}

bool QnxChannel::initializeServer() {
    // Create channel
    channel_id_ = ChannelCreate(0);
    if (channel_id_ == -1) {
        Logger::getInstance().log("Failed to create channel: " + std::string(strerror(errno)));
        return false;
    }

    // Attach name to channel
    attach_ptr_ = name_attach(NULL, channel_name_.c_str(), 0);
    if (attach_ptr_ == nullptr) {
        Logger::getInstance().log("Failed to attach name to channel: " +
                                std::string(strerror(errno)));
        return false;
    }

    // Start receive thread
    startReceiveThread();

    Logger::getInstance().log("Server channel initialized successfully: " + channel_name_);
    return true;
}

bool QnxChannel::initializeClient() {
    // Open connection to the server
    connection_id_ = name_open(channel_name_.c_str(), 0);
    if (connection_id_ == -1) {
        Logger::getInstance().log("Failed to connect to server: " +
                                std::string(strerror(errno)));
        return false;
    }

    Logger::getInstance().log("Client channel initialized successfully: " + channel_name_);
    return true;
}

void QnxChannel::cleanup() {
    std::lock_guard<std::mutex> lock(channel_mutex_);

    // Stop receive thread
    stopReceiveThread();

    // Cleanup QNX resources
    if (connection_id_ != -1) {
        name_close(connection_id_);
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

    Logger::getInstance().log("Channel cleaned up: " + channel_name_);
}

bool QnxChannel::sendMessage(const Message& message) {
    std::lock_guard<std::mutex> lock(channel_mutex_);

    if (connection_id_ == -1) {
        Logger::getInstance().log("Cannot send message: channel not connected");
        return false;
    }

    int status = MsgSend(connection_id_, &message, sizeof(Message), NULL, 0);

    if (status == -1) {
        Logger::getInstance().log("Failed to send message: " + std::string(strerror(errno)));
        return false;
    }

    return true;
}

bool QnxChannel::receiveMessage(Message& message, int timeout_ms) {
    if (!is_server_) {
        Logger::getInstance().log("Cannot receive messages on client channel");
        return false;
    }

    int rcvid;
    if (timeout_ms > 0) {
        struct _msg_info info;
        struct _pulse pulse;
        rcvid = MsgReceivePulse(channel_id_, &pulse, sizeof(pulse), &info);
        if (rcvid == -1 && errno == ETIMEDOUT) {
            return false;
        }
    } else {
        rcvid = MsgReceive(channel_id_, &message, sizeof(Message), NULL);
    }

    if (rcvid == -1) {
        Logger::getInstance().log("Failed to receive message: " + std::string(strerror(errno)));
        return false;
    }

    if (rcvid > 0) {
        // Received a message
        MsgReply(rcvid, EOK, NULL, 0);
        return true;
    }

    return false;
}

void QnxChannel::startReceiveThread() {
    if (!is_server_ || receive_thread_running_) return;

    receive_thread_running_ = true;
    receive_thread_ = std::thread([this]() {
        Logger::getInstance().log("Message receive thread started");

        while (receive_thread_running_) {
            Message msg;
            if (receiveMessage(msg, 100)) {  // 100ms timeout
                handleMessage(msg);
            }
        }

        Logger::getInstance().log("Message receive thread stopped");
    });

    // Set thread priority if necessary
    struct sched_param param;
    param.sched_priority = 10;  // Adjust priority as needed
    pthread_setschedparam(receive_thread_.native_handle(), SCHED_RR, &param);
}

void QnxChannel::stopReceiveThread() {
    if (!receive_thread_running_) return;

    receive_thread_running_ = false;
    if (receive_thread_.joinable()) {
        receive_thread_.join();
    }
}

void QnxChannel::handleMessage(const Message& msg) {
    try {
        std::lock_guard<std::mutex> lock(handlers_mutex_);

        // Find and execute registered handlers for this message type
        auto range = message_handlers_.equal_range(msg.type);
        for (auto it = range.first; it != range.second; ++it) {
            it->second(msg);
        }

        // Log message handling
        std::ostringstream oss;
        oss << "Handled message from " << msg.sender_id
            << " (Type: " << static_cast<int>(msg.type) << ")";
        Logger::getInstance().log(oss.str());

    } catch (const std::exception& e) {
        Logger::getInstance().log("Error handling message: " + std::string(e.what()));
    }
}

void QnxChannel::registerHandler(MessageType type, MessageHandler handler) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    message_handlers_.emplace(type, handler);
}

void QnxChannel::unregisterHandlers(MessageType type) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    message_handlers_.erase(type);
}

bool QnxChannel::isConnected() const {
    std::lock_guard<std::mutex> lock(channel_mutex_);
    return (connection_id_ != -1 || channel_id_ != -1);
}

std::string QnxChannel::getChannelName() const {
    return channel_name_;
}

void QnxChannel::logChannelStatus() const {
    std::ostringstream oss;
    oss << "Channel Status [" << channel_name_ << "]:\n"
        << "  Role: " << (is_server_ ? "Server" : "Client") << "\n"
        << "  Channel ID: " << channel_id_ << "\n"
        << "  Connection ID: " << connection_id_ << "\n"
        << "  Receive Thread: " << (receive_thread_running_ ? "Running" : "Stopped") << "\n"
        << "  Registered Handlers: " << message_handlers_.size();

    Logger::getInstance().log(oss.str());
}

} // namespace comm
} // namespace atc
