#include "Queue.hh"
#include <thread>
#include <atomic>
#include <chrono>
#include <iostream>
#include <assert.h>

using namespace comm;

Queue<int> q;
const int workernum = 10;
const int producernum = 5;
const int msgNum = 1000000;
constexpr uint64_t getTotal(int sum) {
    uint64_t r = 0;
    for (int i = 0; i < sum; i++) {
        r += i;
    }
    return r;
}
const uint64_t total = getTotal(msgNum);
std::atomic<uint64_t> sendCounter{0};
std::atomic<uint64_t> receiveCounter{0};

class CountTime {
public:
    CountTime(const std::string& str) : description(str) {
        t1 = std::chrono::high_resolution_clock::now();
    }
    CountTime(std::string&& str) : description(std::move(str)) {
        t1 = std::chrono::high_resolution_clock::now();
    }

    ~CountTime() {
        t2 = std::chrono::high_resolution_clock::now();
        auto d = std::chrono::duration_cast<std::chrono::milliseconds>(t2-t1);
        std::cout << description << ", use " << d.count() << "ms\n";
    }
private:
    std::string description;
    std::chrono::high_resolution_clock::time_point t1{}, t2{};
};

void producer_thread(int id) {
    std::string description = std::string("producer_thread, total = ").append(std::to_string(id));
    CountTime t(description);
    for (int i = 0; i < msgNum; i++) {
        q.push(i);
        sendCounter.fetch_add(i);
    }
}

void worker_thread(int id) {
    CountTime t(std::string("worker_thread ").append(std::to_string(id)));
    uint64_t sum = total * producernum;
    while (receiveCounter.load() < sum) {
        receiveCounter.fetch_add(q.pop());
    }
}

int main() {
    std::thread p[producernum];
    for (int i = 0; i < producernum; i++) {
        p[i] = std::thread(producer_thread, i);
    }

    std::thread t[workernum];
    for (int i = 0; i < workernum; i++) {
        t[i] = std::thread(worker_thread, i);
    }

    for (int i = 0; i < producernum; i++) {
        p[i].join();
    }
    q.cancel();

    for (int i = 0; i < workernum; i++) {
        t[i].join();
    }
    std::cout << "total = " << total << "\n";
    std::cout << "sendCouner = " << sendCounter.load() << ", receiveCouner = " << receiveCounter << "\n";
    assert(sendCounter.load() == receiveCounter.load());
    assert(sendCounter.load() == (total * producernum));
}