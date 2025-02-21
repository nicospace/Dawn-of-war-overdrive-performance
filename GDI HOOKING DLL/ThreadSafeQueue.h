#pragma once
#include <queue>
#include <mutex>
#include "RenderRequest.h"

class ThreadSafeQueue {
private:
    std::queue<RenderRequest> queue;
    mutable std::mutex mutex;

public:
    ThreadSafeQueue() = default;
    ~ThreadSafeQueue() = default;

    // Prevent copying
    ThreadSafeQueue(const ThreadSafeQueue&) = delete;
    ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;

    void push(RenderRequest&& request) {
        std::lock_guard<std::mutex> lock(mutex);
        queue.push(std::move(request));
    }

    bool try_pop(RenderRequest& request) {
        std::lock_guard<std::mutex> lock(mutex);
        if (queue.empty()) {
            return false;
        }
        request = std::move(queue.front());
        queue.pop();
        return true;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex);
        return queue.empty();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex);
        return queue.size();
    }
};