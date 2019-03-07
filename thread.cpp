#include<iostream>
#include<fstream>
#include<thread>
#include<queue>
#include<mutex>
#include<condition_variable>
#include<vector>
#include<unistd.h>

#define EOS -1

class SafeQueue{
private:
    std::queue<int> queue;
    std::mutex d_mutex;
    std::condition_variable d_condition;

public:

    void safePush(int v){
        std::unique_lock<std::mutex> lock(d_mutex);
        this->queue.push(v);
        this->d_condition.notify_one();
    }

    int safePop(){
        std::unique_lock<std::mutex> lock(d_mutex);
        this->d_condition.wait(lock, [=]{ return !this->queue.empty(); });
        int popped = this->queue.front();
        this->queue.pop();
        return popped;
    }

    bool safeEmpty(){
        std::unique_lock<std::mutex> lock(d_mutex);
        this->d_condition.wait(lock, [=]{ return !this->queue.empty(); });
        return this->queue.empty();
    }

    int safeFront(){
        std::unique_lock<std::mutex> lock(d_mutex);
        this->d_condition.wait(lock, [=]{ return !this->queue.empty(); });
        return this->queue.front();
    }
};

std::vector<SafeQueue*> safeQueues;

void streamInt(int m){
    for(int i = 0; i < m; i++){
        //safeQueues.at(0)->safePush(i);
        safeQueues.at(0)->safePush(rand() % 10);
    }
    safeQueues.at(0)->safePush(EOS); //-1 EOF
}

void streamIncrease(){
    int v;
    while( (v = safeQueues.at(0)->safePop()) != EOS){
        safeQueues.at(1)->safePush(++v);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    safeQueues.at(1)->safePush(EOS);
}

void streamSquare(){
    int v;
    while( (v = safeQueues.at(1)->safePop()) != EOS){
        safeQueues.at(2)->safePush(v*v);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    safeQueues.at(2)->safePush(EOS);
}

void streamDecrease(){
    int v;
    while((v = safeQueues.at(2)->safePop()) != EOS){
        safeQueues.at(3)->safePush(--v);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    safeQueues.at(3)->safePush(EOS);
}

void printAll(){
    int v;
    while ((v = safeQueues.at(3)->safePop()) != EOS)
        std::cout << v << " ";
    std::cout << std::endl;
}

typedef void (*FunctionWithoutParam) ();

int main(int argc, const char** argv) {
    constexpr unsigned num_threads = 5;
    //const int m = 5;
    int m = atoi(argv[1]);
    std::vector<std::function<void()> > funcs;

    funcs.push_back(streamIncrease);
    funcs.push_back(streamSquare);
    funcs.push_back(streamDecrease);
    funcs.push_back(streamDecrease);

    FunctionWithoutParam functions[] =
            {
                    streamIncrease,
                    streamSquare,
                    streamDecrease,
                    printAll
            };
    // A mutex ensures orderly access to std::cout from multiple threads.
    std::mutex iomutex;
    std::vector<std::thread> threads(num_threads);
    safeQueues.resize(0);
    for(int i = 0; i < 4; i++)
        safeQueues.push_back(new SafeQueue());



    for (unsigned i = 0; i < num_threads; ++i) {
        threads[i] = std::thread([&iomutex, i, &m, &functions] {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            while (1) {
                {
                    // Use a lexical scope and lock_guard to safely lock the mutex only
                    // for the duration of std::cout usage.
                    std::lock_guard<std::mutex> iolock(iomutex);
                    std::cout << "Thread #" << i << ": on CPU " << sched_getcpu() << "\n";
                }
                if( i == 0){
                    streamInt(m);
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                }
                else{
                    functions[i - 1]();
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(900));
            }
        });

        // Create a cpu_set_t object representing a set of CPUs. Clear it and mark
        // only CPU i as set.
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(i, &cpuset);
        int rc = pthread_setaffinity_np(threads[i].native_handle(),
                                        sizeof(cpu_set_t), &cpuset);
        if (rc != 0) {
            std::cerr << "Error calling pthread_setaffinity_np: " << rc << "\n";
        }
    }

    for (auto& t : threads) {
        t.join();
    }
    return 0;
}
