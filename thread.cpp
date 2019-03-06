#include<iostream>
#include<fstream>
#include<thread>
#include<queue>
#include<mutex>
#include<condition_variable>
#include<vector>
#include<unistd.h>

#define EOS -1

using namespace std;


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

int main(int argc, char* argv[]){
    int m = atoi(argv[1]);
    

    safeQueues.resize(0);
    for(int i = 0; i < 4; i++)
        safeQueues.push_back(new SafeQueue());

    std::vector<std::thread*> threads;
    threads.resize(0);
    threads.push_back(new thread(streamInt, m));
    threads.push_back(new thread(streamIncrease));
    threads.push_back(new thread(streamSquare));
    threads.push_back(new thread(streamDecrease));
    threads.push_back(new thread(printAll));

    //for(int i = 0; i < 5; i++)
    //    threads.at(i)->sleep_for(std::chrono::milliseconds(1000));

    for(int i = 0; i < 5; i++){
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(i, &cpuset);
        pthread_setaffinity_np(threads.at(i)->native_handle(), sizeof(cpu_set_t), &cpuset);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        std::cout << "Thread #" << i << ": on CPU " << sched_getcpu() << "\n";
    }

    for(int i = 0; i < 5; i++)
        threads.at(i)->join();

    return 0;
}

//EOF e dies on read