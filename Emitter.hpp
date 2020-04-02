#include <iostream>
#include <thread>
#include <vector>
#include <queue>
#include <math.h>
#include <fstream>
#include <sstream>

#define EOS ((void*)-1)

class Emitter{
private:
    size_t* eos = new size_t(-1);
    int maxWorker;
    std::thread* thEmitter; //thread
    std::vector<size_t> inputSequence;//sequenza di input definita dall'utente
    Safe_Queue* workerIdRequest;//coda in cui i worker inseriscono i propri id per richiedere un nuovo task
    std::vector<Safe_Queue*>* jobsRequest; //unidiretional from emitter to worker. Assegnazione dei jobs ai worker
    std::vector<Worker*>* workerVector = new std::vector<Worker*>();//vettore di worker

    std::mutex* actual_worker_mutex;
    int* actualWorker;

    std::atomic<size_t> emitterTime;//variabile atomica che tiene traccia del ts
public:
    Emitter(int _maxWorker, std::vector<size_t> _inputSequence, std::vector<Safe_Queue*>* _jobsRequest, Safe_Queue* _workerIdRequest, std::vector<Worker*>* _workerVector, int* _actualWorker, std::mutex* _actualWorkerMutex) {
        maxWorker = _maxWorker;
        inputSequence = _inputSequence;
        jobsRequest = _jobsRequest;
        workerIdRequest = _workerIdRequest;
        workerVector = _workerVector;
        actualWorker = _actualWorker;
        actual_worker_mutex = _actualWorkerMutex;
    }

    void nextItem(){
        size_t next = 0;
        void* workerRequest = 0;
        int counter = 0;
        next = inputSequence[counter];//prende il task dalla sequenza iniziale
        while(next != *eos){//finchè non trovo eos
            counter += 1;
            workerIdRequest->safe_pop(&workerRequest);//estraggo l'id del worker che richiede il task
            auto start_time = std::chrono::high_resolution_clock::now();
            this->jobsRequest->at(*(int*)workerRequest)->safe_push(new size_t(next)); //assegno al worker il task //TODO il new size_t non mi fa impazzire qui, però mi sembra corretto
            next = inputSequence[counter]; //estraggo l'elemento successivo
            auto end_time = std::chrono::high_resolution_clock::now();
            emitterTime = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
        }
        //avendo estratto EOS mando un eos a ciascun worker
        for(int i = 0; i < jobsRequest->size(); i++) {
            emitterTime = *eos;
            this->jobsRequest->at(i)->safe_push(eos);
        }

        //attivo tutti i worker inattivi, in modo da fargli processare l'eos mandato
        for(int i = getActualWorker(); i < maxWorker; i++) {
            workerVector->at(i)->wake();
        }
    }

    //restituisce il ts dell'emitter
    size_t getActualTime(){
        return emitterTime;
    }

    //restituisce il numero di worker attivi
    int getActualWorker(){
        actual_worker_mutex->lock();
        int acWork = *actualWorker;
        actual_worker_mutex->unlock();
        return acWork;
    }

    //start del thread
    void startEmitter() {
        this->thEmitter = new std::thread(&Emitter::nextItem, this);
    }

    //join del thread
    void joinEmitter(){
        thEmitter->join();
    }
};