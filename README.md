# Airspace Violation Detection System (QNX RTOS)

A real-time C++ system designed to monitor aircraft trajectories, log flight history, detect airspace violations, and display alerts using inter-process communication on the QNX RTOS. Built for deterministic behavior and high reliability in embedded environments.

---

## ✈️ Project Overview

This project simulates an air traffic control subsystem that:
- Monitors aircraft positions
- Detects restricted airspace violations
- Logs flight histories
- Communicates between multiple system components using QNX message passing (channels/pulses)

---

## 🧠 Components

- `main.cpp`: Entry point and system controller
- `aircraft.cpp`: Simulates aircraft with position updates
- `radar_system.cpp`: Continuously receives aircraft positions and checks for violations
- `violation_detector.cpp`: Detects unauthorized entry into restricted zones
- `display_system.cpp`: Outputs real-time alerts to the console
- `history_logger.cpp`: Logs historical position data to persistent storage
- `logger.cpp`: Centralized logging for system events
- `qnx_channel.cpp`: Manages QNX channel creation and message passing
- `constants.cpp`: Contains system-wide thresholds and configuration values

---

## 🛠️ Build Instructions (QNX)

### 🔧 Requirements
- QNX SDP (Software Development Platform)
- QNX toolchain and cross-compiler
- CMake 3.16+

### 🧪 Build Steps

```bash
mkdir build
cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../qnx-toolchain.cmake ..
make
```

> If targeting QNX hardware, ensure environment variables are set for your specific board and architecture.

---

## 🧪 Run Instructions

Once built, you can deploy the binary to your QNX system or run the simulation in a QNX virtual machine.

The system will simulate multiple aircraft and output logs such as:

```
[ALERT] Aircraft A123 has entered restricted airspace (zone X)!
[INFO] Logging violation to /var/log/history.log
```

---

## 📚 Learning Outcomes

- Real-time C++ system design under QNX RTOS
- Message-passing IPC using QNX channels/pulses
- Modular architecture with simulation, detection, and UI layers
- Logging and persistence in embedded systems
- Deterministic behavior and deadline awareness

---

## 📄 License

MIT License. See `LICENSE` for details.
