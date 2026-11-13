#include <mutex>
#include <deque>
#include <thread>
#include <functional>
#include <condition_variable>

class TaskManager
{
public:
    TaskManager();
    ~TaskManager();
    void callLater(const std::function<void()>& func); // enque worker
    void stop();
private:
    std::deque<std::function<void()>> _calls;
    std::condition_variable cv;
    std::mutex mtx;
    std::thread th;
    bool _finished;
private:
    void exec();
};