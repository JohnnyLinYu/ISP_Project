#include "../include/thread_pool.h"
#include <iostream>
#include <mutex>
TaskManager::TaskManager(): _finished(false)
{
    th = std::thread(&TaskManager::exec, this);
}
TaskManager::~TaskManager()
{
    if(th.joinable())
    {
        {
            std::unique_lock<std::mutex> lock(mtx);
            _finished = true;
            cv.notify_one();
        }
        th.join();
    }
}
void TaskManager::exec()
{
    while(true)
    {
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [this](){return !(_calls.size() == 0) || _finished;});
            while(!_calls.empty())
            {
                auto func = std::move(_calls.front());
                _calls.pop_front();

                lock.unlock();
                func();
                lock.lock();
            }
            if(_finished)
            {
                break;
            }
        }
        
    }
}
void TaskManager::callLater(const std::function<void()> &func)
{
    {
        std::lock_guard<std::mutex> lock(mtx);
        _calls.push_back(func);
        cv.notify_one();
    }
}

void TaskManager::stop()
{
    {
        std::lock_guard<std::mutex> lock(mtx);
        _calls.clear();
        cv.notify_one();
    }
}

