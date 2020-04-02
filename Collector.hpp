#include <iostream>
#include <thread>
#include <vector>
#include <queue>
#include <math.h>
#include <fstream>
#include <sstream>

class Collector{
private:
    std::thread* thCollector;
    Safe_Queue* outputQueue;
    int* EOSCounter = new int(0);
    size_t* eos = new size_t(-1);
    int maxWorker;
    std::vector<size_t*> out;//TODO cambiare in size_t
    std::atomic<size_t> collectorTime;
public:
    Collector(Safe_Queue* _outputQueue, int _maxworker){
        outputQueue = _outputQueue;
        maxWorker = _maxworker;
    }

    void main(){
        void* output = 0;
        while (*EOSCounter < maxWorker){//quando ricevo maxworker EOS mi fermo
            outputQueue->safe_pop(&output);//estraggo il valore
            auto start_time = std::chrono::high_resolution_clock::now();
            if((*(int*)output) == *eos){
                *EOSCounter += 1;
            }else{
                out.push_back((size_t*)output);//aggiungo il valore al vettore di output
            }
            auto end_time = std::chrono::high_resolution_clock::now();
            size_t act_service_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
            collectorTime = act_service_time;
        }
        collectorTime = *eos;
    }

    //restituisce il ts del collector
    size_t getActualTime(){
        return collectorTime;
    }

    //start del thread
    void startCollector() {
        this->thCollector = new std::thread(&Collector::main, this);
    }

    //join del thread
    void joinCollector(){
        thCollector->join();
    }
};
