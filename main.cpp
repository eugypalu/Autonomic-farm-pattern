#include <iostream>
#include <thread>
#include <vector>
#include <queue>
#include "./buffers/safe_queue.h"
#include "./Worker.hpp"
#include "./Emitter.hpp"
#include "./Collector.hpp"
#include "./Controller.hpp"
#include <math.h>
#include <fstream>
#include <sstream>

#define EOS ((void*)-1)

void* testOdd(void* _value){
    if(_value == 0){
        std::cout << "even" << std::endl;
        return 0;
    }
    else{
        std::cout << "odd" << std::endl;
        return 0;
    }
}

int busy_wait(size_t time){
    size_t act = 0;
    std::chrono::high_resolution_clock::time_point start_time = std::chrono::high_resolution_clock::now();
    while(act <= time){
        std::chrono::high_resolution_clock::time_point end_time = std::chrono::high_resolution_clock::now();
        act = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
        continue;
    }
    return 0;
}

long isPrime(size_t x){
    if(x==2)
        return 1;
    if(x%2==0)
        return 0;
    size_t i = 2, sq = sqrt(x);
    while(i <= sq){
        if(x % i == 0)
            return 0;
        i++;
    }
    return 1;
}

long seq(std::function<size_t(size_t)> funct, std::vector<size_t> initSequence){
    std::chrono::high_resolution_clock::time_point start_time = std::chrono::high_resolution_clock::now();
    for(size_t i = 0; i < initSequence.size(); i++){
        initSequence[i] = funct(initSequence[i]);
    }
    std::chrono::high_resolution_clock::time_point end_time = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
}

long parallel(int maxWorker, int initWorker, std::vector<size_t> testSequence, int movingAverageParam, size_t expectedServiceTime, size_t perc, std::function<size_t(size_t)> funct){
    std::chrono::high_resolution_clock::time_point start_time = std::chrono::high_resolution_clock::now();

    Controller ctrl(maxWorker, initWorker, testSequence, movingAverageParam, expectedServiceTime, perc, busy_wait);
    ctrl.start();

    std::chrono::high_resolution_clock::time_point end_time = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
}


int main(int argc, const char** argv){
    if(argc < 9){
        std::cout << "Missing Arguments.\nExample: " << argv[0] << " n_tasks start_degree max_degree percentage ts_goal sliding_size task_1 task_2 task_3" << std::endl;//SUGGESTED: 10000 1 8 1 4000 1 16000 4000 32000
        return 1;
    }

    size_t totalTask = std::stoul(argv[1]);
    size_t initWorker = std::stoul(argv[2]);
    size_t maxWorker = std::stoul(argv[3]);
    size_t perc = atoi(argv[4]);
    long expectedServiceTime = atoi(argv[5]);
    long movingAverageParam = atoi(argv[6]);
    size_t task_1 = std::stoul(argv[7]);
    size_t task_2 = std::stoul(argv[8]);
    size_t task_3 = std::stoul(argv[9]);

    /*int totalTask = 10000;

    int maxWorker= 8;
    int initWorker = 1;
    int movingAverageParam = 1;
    size_t expectedServiceTime = 4000;//190350621,2001985 //PROVARE SU MACCHINA XEON 208000
    size_t perc = 1;*/
    //size_t eos = -1;

    //size_t task_1 = (size_t)16000;
    //size_t task_2 = (size_t)4000;
    //size_t task_3 = (size_t)32000;


    std::vector<size_t> testSequence;

    for(int i = 0; i < totalTask/3; i++){
        testSequence.push_back(task_1);
    }

    for(int i = totalTask/3+1; i < 2*totalTask/3; i++){
        testSequence.push_back(task_2);
    }
    for(int i = 2*totalTask/3; i < totalTask; i++){
        testSequence.push_back(task_3);
    }

    testSequence.push_back(-1);

    long parallelTime =parallel(maxWorker, initWorker, testSequence, movingAverageParam, expectedServiceTime, perc, busy_wait);
    //long sequentialTime = seq(busy_wait, testSequence);
    //std::cout<<"sequential time "<<sequentialTime<<std::endl;
    std::cout<<"parallel time "<<parallelTime<<std::endl;
    return 0;
}