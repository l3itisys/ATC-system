#ifndef ATC_PERIODIC_TASK_H
#define ATC_PERIODIC_TASK_H

#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <sys/neutrino.h>

namespace atc {

class PeriodicTask {
public:
    explicit PeriodicTask(std::chrono::milliseconds period, int priority)
        : period_(period)
        , running_(false) {
        // Set thread priority using QNX scheduler
        struct sched_param param;
        param.sched_priority = priority;
        pthread_setschedparam(pthread_self(), SCHED_RR, &param);
    }

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

    // Period management
    void setPeriod(std::chrono::milliseconds new_period) {
        std::lock_guard<std::mutex> lock(mutex_);
        period_ = new_period;
    }

    std::chrono::milliseconds getPeriod() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return period_;
    }

protected:
    virtual void execute() = 0;

private:
    void run() {
        while (running_) {
            auto start = std::chrono::steady_clock::now();

            // Execute periodic task and measure time
            auto exec_start = std::chrono::steady_clock::now();
            execute();
            auto exec_end = std::chrono::steady_clock::now();

            // Update execution time statistics
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                exec_end - exec_start).count();
            updateExecutionStats(duration);

            // Get current period
            std::chrono::milliseconds current_period;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                current_period = period_;
            }

            // Sleep for remaining time in period
            auto end = std::chrono::steady_clock::now();
            auto next_period = start + current_period;
            if (next_period > end) {
                std::this_thread::sleep_until(next_period);
            }
        }
    }

    void updateExecutionStats(int64_t duration) {
        if (duration < best_execution_time_ || best_execution_time_ == 0) {
            best_execution_time_ = duration;
        }
        if (duration > worst_execution_time_) {
            worst_execution_time_ = duration;
        }
    }

    std::chrono::milliseconds period_;
    std::atomic<bool> running_;
    std::thread thread_;
    std::atomic<int64_t> best_execution_time_{0};
    std::atomic<int64_t> worst_execution_time_{0};
    mutable std::mutex mutex_;
};

}

#endif // ATC_PERIODIC_TASK_H
