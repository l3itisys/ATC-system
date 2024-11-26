#ifndef ATC_QNX_CHANNEL_H
#define ATC_QNX_CHANNEL_H

#include "channel.h"
#include "message_types.h"
#include <string>
#include <mutex>
#include <thread>
#include <map>
#include <functional>

struct _name_attach;
typedef struct _name_attach name_attach_t;

namespace atc {
namespace comm {

using MessageHandler = std::function<void(const Message&)>;

class QnxChannel : public IChannel {
public:
    explicit QnxChannel(const std::string& channel_name);
    ~QnxChannel() override;

    // IChannel interface implementation
    bool initialize(bool as_server) override;
    bool sendMessage(const Message& message) override;
    bool receiveMessage(Message& message, int timeout_ms) override;

    void registerHandler(MessageType type, MessageHandler handler);
    void unregisterHandlers(MessageType type);
    bool isConnected() const;
    std::string getChannelName() const;
    void logChannelStatus() const;

private:
    bool initializeServer();
    bool initializeClient();
    void startReceiveThread();
    void stopReceiveThread();
    void handleMessage(const Message& msg);
    void cleanup();

    std::string channel_name_;
    int channel_id_;
    int connection_id_;
    name_attach_t* attach_ptr_;
    mutable std::mutex channel_mutex_;
    bool receive_thread_running_;
    bool is_server_;
    std::thread receive_thread_;
    mutable std::mutex handlers_mutex_;
    std::multimap<MessageType, MessageHandler> message_handlers_;
};

} // namespace comm
} // namespace atc

#endif // ATC_QNX_CHANNEL_H
