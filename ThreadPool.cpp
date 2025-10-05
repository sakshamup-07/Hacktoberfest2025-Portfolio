#include <iostream>
#include <vector>
#include <thread>
#include <queue>
#include <functional>
#include <condition_variable>
#include <future>
using namespace std;

class ThreadPool {
    vector<thread> workers;
    queue<function<void()>> tasks;
    mutex m;
    condition_variable cv;
    bool stop = false;
public:
    ThreadPool(size_t n) {
        for(size_t i=0;i<n;i++) {
            workers.emplace_back([this]{
                for(;;) {
                    function<void()> task;
                    {
                        unique_lock<mutex> lock(m);
                        cv.wait(lock, [this]{return stop || !tasks.empty();});
                        if (stop && tasks.empty()) return;
                        task = move(tasks.front());
                        tasks.pop();
                    }
                    task();
                }
            });
        }
    }
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) -> future<decltype(f(args...))> {
        using return_type = decltype(f(args...));
        auto task = make_shared<packaged_task<return_type()>>(
            bind(forward<F>(f), forward<Args>(args)...)
        );
        future<return_type> res = task->get_future();
        {
            unique_lock<mutex> lock(m);
            tasks.emplace([task](){ (*task)(); });
        }
        cv.notify_one();
        return res;
    }
    ~ThreadPool() {
        {
            unique_lock<mutex> lock(m);
            stop = true;
        }
        cv.notify_all();
        for(auto &w: workers) w.join();
    }
};

int main() {
    ThreadPool pool(4);
    auto f1 = pool.enqueue([](int x){return x*x;}, 7);
    auto f2 = pool.enqueue([](string s){return s+" world";}, "hello");
    cout << "7*7 = " << f1.get() << endl;
    cout << f2.get() << endl;
}
