//https://stackoverflow.com/questions/71598718/timer-with-stdthread 
#pragma once
#include <thread>
#include <chrono>
#include <functional>
#include <cstdio>
#include <atomic>

namespace Grainflow{
class Timer {
public:
    ~Timer() {
        if (mRunning) {
            stop();
        }
    }
    typedef std::chrono::milliseconds Interval;
    typedef std::function<void(int* w)> Timeout;

    void start(const Interval &interval, const Timeout &timeout, int* ptr) {
        mRunning = true;

        mThread = std::thread([this, ptr, interval, timeout] {
            while (mRunning) {
                std::this_thread::sleep_for(interval);

                timeout(ptr);
            }
        });
    }
    void stop() {
        mRunning = false;
        mThread.join();
    }

private:
    std::thread mThread{};
    std::atomic_bool mRunning{};
};
}