#include <iostream>
#include <thread>
#include <vector>
#include <queue>
#include "./buffers/safe_queue.h"
//#include "./buffersDouble/safe_double_queue.h"

#define EOS ((void*)-1)

class Emitter{
private:
    int* eos = new int(-1);
    int* maxWorker;
    std::thread* thEmitter;
    Safe_Queue* inputSequence; //sequenza di interi, sarebbe lo stream iniziale
    std::vector<int*>* vectorWorkerIdRequest;//unidiretional from worker to emitter. Vettore con tutte le richieste fatte dai worker
    std::vector<Safe_Queue*>* jobsRequest; //unidiretional from emitter to worker. Assegnazione dei jobs ai worker
    int* defaultWorkerVector = new int(-2);
public:
    Emitter(int* _maxWorker, Safe_Queue* _inputSequence, std::vector<Safe_Queue*>* _jobsRequest, std::vector<int*>* _vectorWorkerIdRequest) {
        maxWorker = _maxWorker;
        inputSequence = _inputSequence;
        jobsRequest = _jobsRequest;
        vectorWorkerIdRequest = _vectorWorkerIdRequest;
        //std::vector<int> vectorWorkerIdRequest((int)*maxWorker, 0); //ho messo 0 e non EOS per differenziarlo dal caso finale in cui viene inserito EOS e termina tutto??//TODO pensare a questa cosa
    }

    void nextItem(){
        void* next = 0;
        int counter = 0;
        int workerRequest;
        inputSequence->safe_pop(&next);
        while(*(int*)next != *eos){

            //Invece che utilizzare una safequeue ho utilizzato un vettore. Il while scorre finchè non trova un valore diverso da 0
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
    //Safe_Double_Queue* serviceTime;
    Safe_Queue* serviceTime;
    std::thread* thWorker;
    std::vector<int*>* vectorWorkerIdRequest;
    int active_worker;
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
        //this->activate();
        while(checkWorkerStatus() != 0){
            if(checkWorkerStatus() == 2){
                break;
            }
            vectorWorkerIdRequest->at((*workerId)) = ((int*)workerId);
            std::cout<<"push da parte del worker "<<*workerId<<" eseguito"<<std::endl;
            //inputWorkerQueue->safe_push(&workerId);
            void* val = 0;
            jobsRequest->safe_pop(&val);
            auto start_time = std::chrono::high_resolution_clock::now();
            std::cout<<"Il valore assegnato è: "+std::to_string(*(int*)val)<<std::endl;
            if(*(int*)val == -1){
                std::cout<<"il valore è EOS!!!!!!"<<std::endl;//se val è EOS Stampo nell'output stream eos ed esco dal while
                outputQueue->safe_push(EOS);
                int* eos = new int(-1);//TODO orribile, ma non va con EOS, da vedere
                serviceTime->safe_push(eos);
                disactivate();
                break;
            }
            std::cout<<"questo calcolo viene eseguito dal thread numero: "+std::to_string(*workerId)<<std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));
            void* res = testOdd(&val);//qui devo chiamare una void non deve restituirmi valore
            auto end_time = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> act_service_time = (end_time - start_time);
            std::cout<<"il service time è: "<< act_service_time.count()<<std::endl;
            //double* st = new double(act_service_time.count());//TODO orribile
            //size_t st = (size_t)(act_service_time * 1000);
            size_t* st = new size_t(act_service_time.count() * 1000);//TODO cotinua ad essere orribile anche così
            serviceTime->safe_push(st);
            outputQueue->safe_push(res);
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
    int* actualWorker;//TODO dovrà esserci una lock qui dato che il numero potrà cambiare
public:
    Collector(Safe_Queue* _outputQueue, int* _actualWorker){
        outputQueue = _outputQueue;
        actualWorker = _actualWorker;
    }

    void main(){
        while (true){
            if(EOSCounter <= actualWorker){
                void* output;
                outputQueue->safe_pop(&output);
                if(output == EOS){
                    EOSCounter += 1;
                }
                std::cout<<"L'output è: "+std::to_string(*(int*)output);
            }
        }
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
    //Safe_Queue* serviceTime;

    //Safe_Double_Queue* serviceTime;
    Safe_Queue* serviceTime;
    //std::list<double>* serviceTime;
    //std::mutex* serviceTime_mutex;
    //std::condition_variable* serviceTime_condition;

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

        //serviceTime_mutex = new std::mutex();
        //serviceTime_condition = new std::condition_variable();

        this->jobsRequest->resize(*maxWorker);
        this->vectorWorkerIdRequest->resize(*_maxWorker, new int(-2));
        this->workerVector->resize(*_maxWorker);
        this->outputSequence = new Safe_Queue(_queue->safe_get_size());
        //this->serviceTime = new Safe_Double_Queue(2*(*_movingAverageParam));
        this->serviceTime = new Safe_Queue(2*(*_movingAverageParam));
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
        Emitter emitter(maxWorker, initSequence, jobsRequest, vectorWorkerIdRequest);
        for (int i = 0; i < *maxWorker; i+=1) {
            this->workerVector->at(workerId) = new Worker(workerId, jobsRequest->at(workerId), vectorWorkerIdRequest, outputSequence, serviceTime);
            workerId += 1;
        }

        emitter.startEmitter();
        for (int i = 0; i < *maxWorker; ++i) {
            workerVector->at(i)->startWorker();
        }
        for (int i = 0; i < *initWorker; ++i) {
            workerVector->at(i)->activate();
        }

        void* actualTime = 0;
        int averageCounter = 0;
        serviceTime->safe_pop(&actualTime);
        while (*EOSCounter < (*actualWorker)-1){
            //std::cout<<*(int*)actualTime<<std::endl;
            if(*(int*)actualTime == -1){
                EOSCounter +=1;
            }else{
                if(averageCounter < *movingAverageParam) {
                    serviceTime->safe_pop(&actualTime);
                    *averageTime += *(size_t*)actualTime;
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

        for (int i = 0; i < *maxWorker; ++i) {
            std::cout<<"STO FACENDO IL JOIN DEI WORKER"<<std::endl;
            workerVector->at(i)->joinWorker();
        }
        emitter.joinEmitter();
    }

    void checkTime(){
        if(*averageTime >= *expectedServiceTime && *averageTime <= *upperBoundServiceTime){
            //std::cout<<"qui tutto bene, non devono partire altri thread"<<std::endl;
            std::cout<<"Expected service time "<<*expectedServiceTime<<" tempo medio "<<*averageTime<<" qui tutto bene, non devono partire altri thread"<<std::endl;
        }else if(*averageTime < *expectedServiceTime){
            std::cout<<"Expected service time "<<*expectedServiceTime<<" tempo medio "<<*averageTime<<" qui va disattivato un nuovo thread"<<std::endl;
            std::cout<<"sto disattivando il thread "<<*actualWorker<<std::endl;
            workerVector->at(*actualWorker)->disactivate();
            if(*actualWorker > 0){
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
    int* initWorker= new int(3);
    int* movingAverageParam = new int(3);
    size_t* expectedServiceTime = new size_t(1 * 1000);
    size_t* upperBoundServiceTime = new size_t(3*1000);
    //std::cout<<*expectedServiceTime<<"  "<<*upperBoundServiceTime<<"  "<<std::endl;
    Safe_Queue* testSequence;
    testSequence = new Safe_Queue(10 + *maxWorker);
    for(int i = 0; i < 10; i++){
        int* val = new int(rand() % 100 + 1);
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
