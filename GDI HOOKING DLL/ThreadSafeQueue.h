// ThreadSafeQueue.h
#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include "RenderRequest.h" // Make sure the RenderRequest definition is available

class ThreadSafeQueue {
private:
    std::queue<RenderRequest> queue;
    mutable std::mutex mutex;
    std::condition_variable cv;
    std::atomic<bool> running{ true };

public:
    void push(RenderRequest request) {
        std::lock_guard<std::mutex> lock(mutex);
        queue.push(std::move(request));
        cv.notify_one();
    }

    bool tryPop(RenderRequest& request) {
        std::lock_guard<std::mutex> lock(mutex);
        if (queue.empty()) return false;
        request = std::move(queue.front());
        queue.pop();
        return true;
    }

    bool waitPop(RenderRequest& request, std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex);
        if (!cv.wait_for(lock, timeout, [this] { return !queue.empty() || !running; })) {
            return false;
        }
        if (!running && queue.empty()) return false;
        request = std::move(queue.front());
        queue.pop();
        return true;
    }

    void stop() {
        running = false;
        cv.notify_all();
    }
};
