#ifndef ATC_CONTROL_COMPUTER_H
#define ATC_CONTROL_COMPUTER_H

#include "core/violation_detector.h"
#include "core/aircraft.h"
#include "communication/qnx_channel.h"
#include "common/types.h"
#include "common/constants.h"

#include <vector>
#include <memory>
#include <thread>
#include <atomic>

namespace atc
{

    class ControlComputer
    {
    public:
        std::vector<AircraftState> getAircraftStates() const;

        ControlComputer();
        ~ControlComputer();

        void initializeSystem();
        void start();
        void stop();

    private:
        void handleMessages();
        void handleViolations();
        void logData();
        void periodicLoggingTask();

        std::shared_ptr<ViolationDetector> violation_detector_;
        std::vector<std::shared_ptr<Aircraft>> aircraft_;
        std::shared_ptr<comm::QnxChannel> channel_;
        std::thread message_thread_;
        std::thread logging_thread_;
        std::atomic<bool> running_;
    };

}

#endif // ATC_CONTROL_COMPUTER_H