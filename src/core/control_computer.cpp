#include "core/control_computer.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>

namespace atc
{

    ControlComputer::ControlComputer()
        : violation_detector_(std::make_shared<ViolationDetector>()),
          channel_(std::make_shared<comm::QnxChannel>("CONTROL_COMPUTER")),
          running_(false) {}

    ControlComputer::~ControlComputer()
    {
        stop();
    }

    std::vector<AircraftState> ControlComputer::getAircraftStates() const {
    std::vector<AircraftState> states;
    std::lock_guard<std::mutex> lock(mutex_);  // If accessing shared resources
    for (const auto& aircraft : aircraft_) {
        states.push_back(aircraft->getState());
    }
    return states;
}


    void ControlComputer::initializeSystem()
    {
        // Initialize the communication channel
        if (!channel_->initialize())
        {
            throw std::runtime_error("Failed to initialize communication channel");
        }

        std::cout << "ControlComputer: Communication channel initialized\n";

        // Load initial aircraft data (hardcoded or from a file for now)
        aircraft_.push_back(std::make_shared<Aircraft>("FLIGHT1", Position{10000, 20000, 20000}, Velocity{-300, 0, 0}));
        aircraft_.push_back(std::make_shared<Aircraft>("FLIGHT2", Position{90000, 20000, 20000}, Velocity{300, 0, 0}));

        // Add aircraft to the violation detector
        for (auto &aircraft : aircraft_)
        {
            violation_detector_->addAircraft(aircraft);
        }

        std::cout << "ControlComputer: Aircraft initialized and added to violation detector\n";
    }

    void ControlComputer::start()
    {
        if (running_)
            return;

        running_ = true;

        // Start aircraft threads
        for (auto &aircraft : aircraft_)
        {
            aircraft->start();
        }

        // Start violation detector
        violation_detector_->start();

        // Start message handler thread
        message_thread_ = std::thread(&ControlComputer::handleMessages, this);

        // Start logging thread
        logging_thread_ = std::thread(&ControlComputer::periodicLoggingTask, this);

        std::cout << "ControlComputer: System started\n";
    }

    void ControlComputer::stop()
    {
        if (!running_)
            return;

        running_ = false;

        // Stop threads
        for (auto &aircraft : aircraft_)
        {
            aircraft->stop();
        }
        violation_detector_->stop();

        if (message_thread_.joinable())
        {
            message_thread_.join();
        }

        if (logging_thread_.joinable())
        {
            logging_thread_.join();
        }

        std::cout << "ControlComputer: System stopped\n";
    }

    void ControlComputer::handleMessages()
    {
        while (running_)
        {
            comm::Message message;
            if (channel_->receiveMessage(message, 1000))
            { // Timeout of 1000ms
                switch (message.type)
                {
                case comm::MessageType::COMMAND:
                {
                    auto &command = message.payload.command_data;
                    auto it = std::find_if(aircraft_.begin(), aircraft_.end(),
                                           [&command](const std::shared_ptr<Aircraft> &aircraft)
                                           {
                                               return aircraft->getState().callsign == command.target_id;
                                           });
                    if (it != aircraft_.end())
                    {
                        // Handle specific commands
                        if (command.command == "CHANGE_SPEED")
                        {
                            double new_speed = std::stod(command.params[0]);
                            (*it)->updateSpeed(new_speed);
                        }
                        else if (command.command == "CHANGE_HEADING")
                        {
                            double new_heading = std::stod(command.params[0]);
                            (*it)->updateHeading(new_heading);
                        }
                        else if (command.command == "CHANGE_ALTITUDE")
                        {
                            double new_altitude = std::stod(command.params[0]);
                            (*it)->updateAltitude(new_altitude);
                        }
                    }
                    break;
                }
                case comm::MessageType::ALERT:
                    std::cout << "ALERT: " << message.payload.alert_data.description << std::endl;
                    break;

                default:
                    break;
                }
            }
        }
    }

    void ControlComputer::periodicLoggingTask()
    {
        while (running_)
        {
            logData();
            std::this_thread::sleep_for(std::chrono::milliseconds(constants::HISTORY_LOGGING_INTERVAL));
        }
    }

    void ControlComputer::logData()
    {
        std::ofstream log_file("airspace_history.log", std::ios::app);

        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);

        log_file << "=========================\n";
        log_file << "Airspace State at: " << std::ctime(&time);
        for (const auto &aircraft : aircraft_)
        {
            auto state = aircraft->getState();
            log_file << "Aircraft: " << state.callsign
                     << " Position: (" << state.position.x << ", " << state.position.y << ", " << state.position.z << ")\n";
        }
        log_file << "=========================\n";
    }

} // namespace atc