#ifndef GOBAN_INPUTTHREAD_H
#define GOBAN_INPUTTHREAD_H

#include <mutex>
#include <deque>
#include <thread>
#include <iostream>
#include <condition_variable>
#include <string>
#include <chrono>
#include <spdlog/spdlog.h>

//based on https://gist.github.com/vmrob/ff20420a20c59b5a98a1

template <class C, class T>
class InputThread {
public:

    explicit InputThread(T &fin) : io(0) {
        io = new std::thread([&]() {
            std::string tmp;
            bool good = true;
            while (good) {
                good = static_cast<bool>(std::getline(fin, tmp));
                std::lock_guard<std::mutex> lock{mutex};
                lines.push_back(std::move(tmp));
                cv.notify_one();
            }
            lines.push_back("__EOF__");
            cv.notify_one();
        });
    }

    ~InputThread() {
        if(io) {
            io->join();
            delete io;
        }
        if(consumer) {
            consumer->join();
            delete consumer;
        }
    }

    void bind(C &callback) {
        consumer = new std::thread([&]() {
            bool good = true;
            while (good) {
                {
                    std::unique_lock<std::mutex> lock{mutex};
                    if (cv.wait_for(lock, std::chrono::seconds(0), [&] { return !lines.empty(); })) {
                        std::swap(lines, toProcess);
                    }
                }
                if (!toProcess.empty()) {
                    for (auto &&line : toProcess) {
                        if(line != "__EOF__") {
                            callback(line);
                        }
                        else {
                            good = false;
                            break;
                        }
                    }
                    toProcess.clear();
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });
    }

private:
    std::condition_variable cv;
    std::mutex mutex;
    std::deque<std::string> lines;

    std::thread* io;
    std::thread* consumer{};

    std::deque<std::string> toProcess;

};

#endif
