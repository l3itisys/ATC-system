#ifndef ATC_PERIODIC_TASK_H
#define ATC_PERIODIC_TASK_H

#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
extern "C" {
    #include <unistd.h>
    #include <time.h>
}

namespace atc {

class PeriodicTask {
public:
    explicit PeriodicTask(std::chrono::milliseconds period, int priority)
        : period_(period)
        , running_(false)
        , best_execution_time_(0)
        , worst_execution_time_(0) {}

    virtual ~PeriodicTask() {
        stop();
    }

    void start() {
        if (!running_) {
            running_ = true;
            thread_ = std::thread(&PeriodicTask::run, this);
        }
    }

    void stop() {
        running_ = false;
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    // Get execution time statistics
    int64_t getBestExecutionTime() const { return best_execution_time_; }
    int64_t getWorstExecutionTime() const { return worst_execution_time_; }

protected:
    virtual void execute() = 0;

    // Protected method for execution stats that derived classes can use
    void updateExecutionStats(int64_t duration) {
        if (duration < best_execution_time_ || best_execution_time_ == 0) {
            best_execution_time_ = duration;
        }
        if (duration > worst_execution_time_) {
            worst_execution_time_ = duration;
        }
    }

private:
    void run() {
        while (running_) {
            auto start = std::chrono::steady_clock::now();

            execute();

            auto end = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                end - start).count();

            // Update execution statistics internally
            updateExecutionStats(elapsed);

            // Calculate remaining time in period
            auto sleep_time = period_.count() * 1000 - elapsed;  // Convert ms to μs

            if (sleep_time > 0) {
                usleep(sleep_time);
            }
        }
    }

    std::chrono::milliseconds period_;
    std::atomic<bool> running_;
    std::thread thread_;
    std::atomic<int64_t> best_execution_time_;
    std::atomic<int64_t> worst_execution_time_;
    mutable std::mutex mutex_;
};

} // namespace atc

#endif // ATC_PERIODIC_TASK_H
