#include <iostream>
#include <thread>
#include <vector>
#include <queue>
#include <math.h>
#include <fstream>
#include <sstream>

//#define EOS ((void*)-1)


class Worker{
private:
    int* workerId; //assegnato dal controller

    std::function<size_t(size_t)> funct;

    Safe_Queue* outputQueue;
    Safe_Queue* jobsRequest;
    std::thread* thWorker;
    Safe_Queue* workerIdRequest;
    int active_worker;
    int* eos = new int(-1);
    std::mutex* status_mutex;
    std::condition_variable* status_condition;
    std::atomic<size_t> actualTime;

    std::mutex* actual_worker_mutex;
    int* actualWorker;

public:

    Worker(int _workerId, Safe_Queue* _jobsRequest, Safe_Queue* _workerIdRequest, Safe_Queue* _outputQueue, std::function<size_t(size_t)> _funct, int* _actualWorker, std::mutex* _actualWorkerMutex){
        workerId = new int(_workerId);
        jobsRequest = _jobsRequest;
        workerIdRequest = _workerIdRequest;
        outputQueue = _outputQueue;
        status_mutex = new std::mutex();
        status_condition = new std::condition_variable();
        active_worker = 0; //di default il worker non è attivo, viene attivato quando viene chiamata la funzione startworker
        funct = _funct;
        actualWorker = _actualWorker;
        actual_worker_mutex = _actualWorkerMutex;
    }

    void main(){

        while(checkWorkerStatus() == 1){//Se il worker è attivo
            //In teoria ad inizio while la coda è vuota, perchè ho fatto il pop precedentemente. Se è piena significa che qualcuno ci ha pushato eos
            if(jobsRequest->safe_empty() == 1){//controlla se la coda è vuota. Di default la coda è vuota, in quanto è il worker che deve richiedere un task all'emitter
                workerIdRequest->safe_push((void*)workerId);//richiedo un task all'emitter
                void* val = 0;
                jobsRequest->safe_pop(&val);//prendo il task assegnatomi dall'emitter
                auto start_time = std::chrono::high_resolution_clock::now();
                if(*(int*)val == -1){//se il valore è eos
                    outputQueue->safe_push(eos);
                    long* eos = new long(-1);//TODO orribile, ma non va con EOS, da vedere
                    disactivate();//se riceve eos disattiva il thread
                    break;
                }
                long* res = new long(funct(*(size_t*)val));//eseguo il task//TODO non so se ha senso castare qua  e il new long mi pare una porcata.
                ssize_t &r = (*((ssize_t*) res));
                auto end_time = std::chrono::high_resolution_clock::now();
                size_t act_service_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count()+1;//calcolo il service time del thread
                actualTime = act_service_time;
                outputQueue->safe_push(&r);//mando al collector il risultato del task
            }else{//in questo caso il worker ha un elemento in coda, questo succede solo quando l'emitter riceve eos e manda eos a tutti i worker, indipendentemente che fossero attivi o no
                void* val = 0;
                jobsRequest->safe_pop(&val); //Estrae il valore
                if(*(int*)val == -1){
                    outputQueue->safe_push(eos);
                    size_t* eos = new size_t(-1);//TODO orribile, ma non va con EOS, da vedere
                    disactivate();//disattiva il thread
                    actualTime = *eos;
                    break;
                }

                outputQueue->safe_push(eos);
            }

        }
    }

    //setta lo stato del worker su attivo (1) e inrementa il numero di worker attivi
    void activate(){
        //0:non_attivo
        //1: attivo
        std::unique_lock<std::mutex> lock(*status_mutex);
        active_worker = 1;
        increaseActualWorker();
        notify();
        std::cout<<"worker "<<*workerId<<" attivato"<<std::endl;
    }

    //Attiva il worker senza incrementare il numero di worker attivi, viene chiamato dall'emitter sui worker inattivi quando riceve EOS
    void wake(){
        //0:non_attivo
        //1: attivo
        std::unique_lock<std::mutex> lock(*status_mutex);
        active_worker = 1;
        notify();
    }

    void notify(){
        status_condition->notify_one();
    }

    //setta lo stato del worker su inattivo (0) e decrementa il numero di worker attivi
    void disactivate(){
        std::unique_lock<std::mutex> lock(*status_mutex);
        active_worker = 0;
        decreaseActualWorker();
        status_condition->notify_one();
        std::cout<<"worker "<<*workerId<<" disattivato"<<std::endl;
    }

    //Quando un worker viene attivato viene incrementato il numero di worker attivi
    void increaseActualWorker(){
        actual_worker_mutex->lock();
        *actualWorker += 1;
        actual_worker_mutex->unlock();
    }

    //Quando un worker viene disattivato viene decrementato il numero di worker attivi
    void decreaseActualWorker(){
        actual_worker_mutex->lock();
        *actualWorker -= 1;
        actual_worker_mutex->unlock();
    }

    //restituisce il ts del worker
    size_t getActualTime(){
        return actualTime;
    }

    //controlla se il worker è attivo o no
    int checkWorkerStatus(){
        std::unique_lock<std::mutex> lock(*status_mutex);
        if(active_worker == 0){//nel caso in cui non è attivo aspetta la notifica
            status_condition->wait(lock);
        }
        return active_worker;
    }

    //start del thread
    void startWorker() {
        std::cout<<"Worker "+std::to_string(*workerId)+" Runnato"<<std::endl;
        this->thWorker = new std::thread(&Worker::main, this);
    }

    //join del thread
    void joinWorker(){
        thWorker->join();
    }
};