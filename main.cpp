#include <iostream>
#include <thread>
#include <vector>
#include <queue>
#include "./buffers/safe_queue.h"
#include <math.h>

#define EOS ((void*)-1)

class Emitter{
private:
    int* eos = new int(-1);
    int* maxWorker;
    std::thread* thEmitter;
    Safe_Queue* inputSequence; //sequenza di interi, sarebbe lo stream iniziale
    Safe_Queue* emitterTime;
    std::vector<int*>* vectorWorkerIdRequest;//unidiretional from worker to emitter. Vettore con tutte le richieste fatte dai worker
    std::vector<Safe_Queue*>* jobsRequest; //unidiretional from emitter to worker. Assegnazione dei jobs ai worker
    int* defaultWorkerVector = new int(-2);
public:
    Emitter(int* _maxWorker, Safe_Queue* _inputSequence, std::vector<Safe_Queue*>* _jobsRequest, std::vector<int*>* _vectorWorkerIdRequest, Safe_Queue* _emitterTime) {
        maxWorker = _maxWorker;
        inputSequence = _inputSequence;
        jobsRequest = _jobsRequest;
        vectorWorkerIdRequest = _vectorWorkerIdRequest;
        emitterTime = _emitterTime;
    }

    void nextItem(){
        void* next = 0;
        int counter = 0;
        int workerRequest;
        inputSequence->safe_pop(&next);
        while(*(int*)next != *eos){
            auto start_time = std::chrono::high_resolution_clock::now();

            //Invece che utilizzare una safequeue ho utilizzato un vettore. Il while scorre finchè non trova un valore diverso da -2
            while(*vectorWorkerIdRequest->at(counter) == -2){
                if(counter == ((int)*maxWorker)-1){
                    counter = 0;
                }else{
                    counter += 1;
                }
            }
            workerRequest = *vectorWorkerIdRequest->at(counter);
            this->jobsRequest->at(workerRequest)->safe_push(next); //assegno al worker
            std::cout<<"eseguito il push di: " + std::to_string(*(int*)next)+" alla lista "+std::to_string(workerRequest)<<std::endl;

            vectorWorkerIdRequest->at(counter) = defaultWorkerVector;
            counter +=1;
            if(counter == ((int)*maxWorker)) counter = 0;
            inputSequence->safe_pop(&next);


            auto end_time = std::chrono::high_resolution_clock::now();
            size_t act_service_time_worker = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
            size_t* stw = new size_t(act_service_time_worker);//TODO cotinua ad essere orribile anche così
            emitterTime->safe_push(stw);
        }
        for(int i = 0; i < jobsRequest->size(); i++) {
            this->jobsRequest->at(i)->safe_push(eos);
        }
    }

    void startEmitter() {
        this->thEmitter = new std::thread(&Emitter::nextItem, this);
    }

    void joinEmitter(){
        thEmitter->join();
    }
};

class Worker{
private:
    int* workerId; //assegnato dal controller
    Safe_Queue* outputQueue;
    Safe_Queue* jobsRequest;//viene creata dal controller
    Safe_Queue* serviceTime;
    std::thread* thWorker;
    std::vector<int*>* vectorWorkerIdRequest;
    int active_worker;
    int* eos = new int(-1);
    std::mutex* status_mutex;
    std::condition_variable* status_condition;

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

public:

    Worker(int _workerId, Safe_Queue* _jobsRequest, std::vector<int*>* _vectorWorkerIdRequest, Safe_Queue* _outputQueue, Safe_Queue* _serviceTime){
        workerId = new int(_workerId);
        jobsRequest = _jobsRequest;
        vectorWorkerIdRequest = _vectorWorkerIdRequest;
        outputQueue = _outputQueue;
        serviceTime = _serviceTime;
        status_mutex = new std::mutex();
        status_condition = new std::condition_variable();
        active_worker = 0; //di default il worker non è attivo, viene attivato quando viene chiamata la funzione startworker

    }

    void main(){
        while(checkWorkerStatus() != 0){
            if(checkWorkerStatus() == 2){
                break;
            }
            vectorWorkerIdRequest->at((*workerId)) = ((int*)workerId);
            std::cout<<"push da parte del worker "<<*workerId<<" eseguito"<<std::endl;
            void* val = 0;
            jobsRequest->safe_pop(&val);
            auto start_time = std::chrono::high_resolution_clock::now();
            std::cout<<"Il valore assegnato è: "+std::to_string(*(int*)val)<<std::endl;
            if(*(int*)val == -1){
                std::cout<<"il valore è EOS!!!!!!"<<std::endl;//se val è EOS Stampo nell'output stream eos ed esco dal while
                outputQueue->safe_push(eos);
                long* eos = new long(-1);//TODO orribile, ma non va con EOS, da vedere
                serviceTime->safe_push(eos);
                disactivate();
                break;
            }
            std::cout<<"questo calcolo viene eseguito dal thread numero: "+std::to_string(*workerId)<<std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));
            long* res = new long(isPrime((size_t)val));//TODO non so se ha senso castare qua  e il new long mi pare una porcata.
            ssize_t &r = (*((ssize_t*) res));
            auto end_time = std::chrono::high_resolution_clock::now();
            size_t act_service_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
            std::cout<<"il service time è: "<< act_service_time<<std::endl;
            size_t* st = new size_t(act_service_time);//TODO cotinua ad essere orribile anche così
            serviceTime->safe_push(st);
            outputQueue->safe_push(&r);
        }
    }

    void activate(){
        //0:non_attivo
        //1: attivo
        //2: termina
        std::cout<<"worker "<<*workerId<<" attivato"<<std::endl;
        std::unique_lock<std::mutex> lock(*status_mutex);
        active_worker = 1;
        status_condition->notify_one();
    }

    void disactivate(){
        std::unique_lock<std::mutex> lock(*status_mutex);
        active_worker = 0;
        status_condition->notify_one();
    }

    void disactivateAndTerminate(){
        std::unique_lock<std::mutex> lock(*status_mutex);
        active_worker = 2;
        status_condition->notify_one();
    }

    int checkWorkerStatus(){
        std::unique_lock<std::mutex> lock(*status_mutex);
        if(active_worker == 0){
            status_condition->wait(lock);
        }
        return active_worker;
    }

    void startWorker() {
        std::cout<<"Worker "+std::to_string(*workerId)+" Runnato"<<std::endl;
        this->thWorker = new std::thread(&Worker::main, this);
    }

    void joinWorker(){
        thWorker->join();
    }
};

class Collector{
private:
    std::thread* thCollector;
    Safe_Queue* outputQueue;
    int* EOSCounter = new int(0);
    int* eos = new int(-1);
    std::vector<long*> out;
    std::mutex* actual_worker_mutex;
    int* actualWorker;//TODO dovrà esserci una lock qui dato che il numero potrà cambiare
public:
    Collector(Safe_Queue* _outputQueue, int* _actualWorker, std::mutex* _actualWorkerMutex){
        outputQueue = _outputQueue;
        actualWorker = _actualWorker;
        actual_worker_mutex = _actualWorkerMutex;
    }

    void main(){
        void* output = 0;
        //outputQueue->safe_pop(&output);
        //while (*EOSCounter < *actualWorker){
        while (*EOSCounter < getActualWorker()){
            outputQueue->safe_pop(&output);
            std::cout<<"L'output è: "<<(*(int*)output)<<std::endl;
            if((*(int*)output) == *eos){
                *EOSCounter += 1;
            }else{
                out.push_back((long*)output);
            }
        }
    }

    int getActualWorker(){//TODO mi pare una brutta cosa reimplementare la stessa cosa anche qui, sarebbe stato meglio fare un qualcosa di condiviso
        actual_worker_mutex->lock();
        int acWork = *actualWorker;
        actual_worker_mutex->unlock();
        return acWork;
    }

    void startCollector() {
        this->thCollector = new std::thread(&Collector::main, this);
    }

    void joinCollector(){
        thCollector->join();
    }
};

class Controller{
private:
    std::thread* thController;
    int* maxWorker;
    int* initWorker;
    int* actualWorker = new int(0);
    int* movingAverageParam;
    size_t* averageTime = new size_t(0);
    size_t* expectedServiceTime;
    size_t* upperBoundServiceTime;
    Safe_Queue* initSequence; //sequenza di void mandata dall'utente
    Safe_Queue* outputSequence;//TODO per ora è una safequeue, poi diventerà un vettore con le lock per ridurre overhead????????

    Safe_Queue* serviceTime;//dei worker
    Safe_Queue* emitterTime;

    std::mutex* actual_worker_mutex;
    //std::condition_variable* actual_worker_condition;

    std::vector<Safe_Queue*>* jobsRequest = new std::vector<Safe_Queue*>();
    std::vector<Worker*>* workerVector = new std::vector<Worker*>();
    std::vector<int*>* vectorWorkerIdRequest = new std::vector<int*>();
public:
    Controller(int* _maxWorker, int* _initWorker, Safe_Queue* _queue, int* _movingAverageParam, size_t* _expectedTS, size_t* _upperTS){
        maxWorker = _maxWorker;
        initWorker = _initWorker;
        initSequence = _queue;
        *actualWorker = *initWorker;
        movingAverageParam = _movingAverageParam;
        //serviceTime = new Safe_Queue(2*(*_movingAverageParam));//TODO qui ho scelto 2 volte il valore della media scelto dall'utente, dovrei considerare se la media è minore dei maxworker in modo tale da prendere 2 volte i worker in caso?
        expectedServiceTime = _expectedTS;
        upperBoundServiceTime = _upperTS;

        actual_worker_mutex = new std::mutex();
        //actual_worker_condition = new std::condition_variable();

        this->jobsRequest->resize(*maxWorker);
        this->vectorWorkerIdRequest->resize(*_maxWorker, new int(-2));
        this->workerVector->resize(*_maxWorker);
        this->outputSequence = new Safe_Queue(_queue->safe_get_size());
        //this->serviceTime = new Safe_Double_Queue(2*(*_movingAverageParam));
        this->serviceTime = new Safe_Queue(2*(*_movingAverageParam));//TODO ricontrollare se va bene come dimensione per il dane, come logica torna
        this->emitterTime = new Safe_Queue(2*(*_movingAverageParam));//TODO uguale a sopra
        for (int i = 0; i < *_maxWorker; ++i) {
            Safe_Queue* q = new Safe_Queue(1);
            jobsRequest->at(i) = q;
            std::cout<<"safequeue creata all'indirizzo ";
            std::cout<< jobsRequest->at(i) << std::endl;
        }
    }

    void start(){
        int workerId = 0;
        int* EOSCounter = new int(0);
        Emitter emitter(maxWorker, initSequence, jobsRequest, vectorWorkerIdRequest, emitterTime);
        Collector collector(outputSequence, actualWorker, actual_worker_mutex);
        for (int i = 0; i < *maxWorker; i+=1) {
            this->workerVector->at(workerId) = new Worker(workerId, jobsRequest->at(workerId), vectorWorkerIdRequest, outputSequence, serviceTime);//TODO orribile passargli il valore e poi creare un nuovo puntatore con valore quello passato, però non trovavo una soluzione migliore. (se passo indirizzo è un casino)
            workerId += 1;
        }

        emitter.startEmitter();
        for (int i = 0; i < *maxWorker; ++i) {
            workerVector->at(i)->startWorker();
        }
        for (int i = 0; i < *initWorker; ++i) {
            workerVector->at(i)->activate();
        }
        collector.startCollector();


        void* actualTime = 0;
        void* actualEmitterTime = 0;
        int averageCounter = 0;
        serviceTime->safe_pop(&actualTime);
        while (*EOSCounter < (*actualWorker)-1){
            //std::cout<<*(int*)actualTime<<std::endl;
            if(*(int*)actualTime == -1){
                EOSCounter +=1;
            }else{
                if(averageCounter < *movingAverageParam) {
                    serviceTime->safe_pop(&actualTime);
                    emitterTime->safe_pop(&actualEmitterTime);
                    *averageTime += *(size_t*)actualTime + *(size_t*)actualEmitterTime;
                    averageCounter += 1;
                }else{
                    *averageTime = *averageTime / averageCounter;
                    this->checkTime();
                    averageCounter = 0;
                }
            }
        }

        for (int j = *actualWorker; j < *maxWorker; ++j) {
            std::cout<<"disattivo il worker "<<j<<std::endl;
            workerVector->at(j)->disactivateAndTerminate();
        }

        emitter.joinEmitter();
        for (int i = 0; i < *maxWorker; ++i) {
            std::cout<<"STO FACENDO IL JOIN DEI WORKER"<<std::endl;
            workerVector->at(i)->joinWorker();
        }
        collector.joinCollector();
    }

    int getActualWorker(){
        actual_worker_mutex->lock();
        int acWork = *actualWorker;
        actual_worker_mutex->unlock();
        return acWork;
    }

    void checkTime(){
        if(*averageTime >= *expectedServiceTime && *averageTime <= *upperBoundServiceTime){
            //std::cout<<"qui tutto bene, non devono partire altri thread"<<std::endl;
            std::cout<<"Expected service time "<<*expectedServiceTime<<" tempo medio "<<*averageTime<<" qui tutto bene, non devono partire altri thread"<<std::endl;
        }else if(*averageTime < *expectedServiceTime){
            std::cout<<"Expected service time "<<*expectedServiceTime<<" tempo medio "<<*averageTime<<" qui va disattivato un nuovo thread"<<std::endl;
            std::cout<<"sto disattivando il thread "<<*actualWorker<<std::endl;
            workerVector->at(*actualWorker)->disactivate();
            if(*actualWorker > 1){
                *actualWorker -= 1;
                std::cout<<"actual worker "<<*actualWorker<<std::endl;
            }
        }else if(*averageTime > *upperBoundServiceTime){
            //std::cout<<"qui va disattivato un thread"<<std::endl;
            std::cout<<"Expected service time "<<*expectedServiceTime<<" tempo medio "<<*averageTime<<" qui va attivato un thread"<<std::endl;
            std::cout<<"sto attivando il thread "<<*actualWorker<<std::endl;
            if(*actualWorker < *maxWorker){
                workerVector->at(*actualWorker)->activate();
                *actualWorker += 1;
            }

        }

    }

    void startController() {
        this->thController = new std::thread(&Controller::start, this);
    }

    void joinController(){
        thController->join();
    }
};

int main() {
    int* maxWorker= new int(5);
    int* initWorker= new int(2);//TODO controllare perchè con 1 ad una certa non va
    int* movingAverageParam = new int(3);
    size_t* expectedServiceTime = new size_t(1 * 1000);
    size_t* upperBoundServiceTime = new size_t(3*1000);
    //std::cout<<*expectedServiceTime<<"  "<<*upperBoundServiceTime<<"  "<<std::endl;
    Safe_Queue* testSequence;
    testSequence = new Safe_Queue(10 + *maxWorker);
    for(int i = 0; i < 10; i++){
        int* val = new int(rand() % 10000 + 1);
        testSequence->safe_push(val);
    }

    int* eos = new int(-1);
    for (int j = 0; j < *maxWorker; ++j) {
        testSequence->safe_push(eos);
    }

    Controller ctrl(maxWorker, initWorker, testSequence, movingAverageParam, expectedServiceTime, upperBoundServiceTime);
    ctrl.start();
    /*for(int i = 0; i < 10; i++){
        void* val = 0;
        testSequence->safe_pop(&val);
        std::cout<<(*((int*) val))<<std::endl;
    }*/
    /*Emitter emit(numWorker, testSequence);
    std::vector<Worker> workerVector[*numWorker];
    emit.startEmitter();*/
    //emit.nextItem();
    /*for (int i = 0; i < *numWorker; ++i) {
        workerVector->at(i).startWorker();
    }*/

    return 0;
}