#ifndef COMM_QUEUE_HH_
#define COMM_QUEUE_H__

#include <deque>
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace comm {

template<typename T>
class Queue {
public:
    using Queue_T = std::deque<T>;
    Queue() = default;
    ~Queue() = default;
    Queue(const Queue& q) = delete;
    Queue& operator=(const Queue& q) = delete;

    template<typename Elem = T>
    void push(Elem&& elem) {
        {
            std::lock_guard<std::mutex> l(mtx_);
            q_.emplace_back(std::forward<Elem>(elem));
        }
        cv_.notify_all();
    }
    
    T pop() {
        std::unique_lock<std::mutex> l(mtx_);
        while (q_.empty()) {
            if (cancel_) {
                return T{};
            }
            cv_.wait(l);
        }
        auto elem = q_.front();
        q_.pop_front();
        return elem;
    }
    void cancel() {
        cancel_ = true;
        cv_.notify_all();
    }
private:
    Queue_T q_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::atomic_bool cancel_ = false;
};


}


#endif