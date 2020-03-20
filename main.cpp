#include <iostream>
#include <thread>
#include <vector>
#include <queue>

#include "./buffers/safe_queue.h"

#define EOS ((void*)-1)

class Emitter{//TODO dovrà essere un singleton
private:
    int* maxWorker;
    std::thread* thEmitter;
    //std::vector<int> workerJobs; //Non so se serve, lista dove ad ogni worker avrei associato se attivo, non attivo, occupato //forse va gestito con il manager
    Safe_Queue* inputSequence; //sequenza di interi, sarebbe lo stream iniziale

    std::vector<int*>* vectorWorkerIdRequest;//unidiretional from worker to emitter. Vettore con tutte le richieste fatte dai worker
    std::vector<Safe_Queue*>* jobsRequest; //unidiretional from emitter to worker. Assegnazione dei jobs ai worker
public:
    //Emitter(int _maxWorker, SafeQueue<int>* _inputSequence) :  maxWorker(_maxWorker), inputSequence(_inputSequence) {}
    //Emitter(int* _maxWorker, Safe_Queue* _inputSequence) : maxWorker(_maxWorker), inputSequence(_inputSequence) {vectorWorkerIdRequest(_maxWorker, 1)}
    Emitter(int* _maxWorker, Safe_Queue* _inputSequence, std::vector<Safe_Queue*>* _jobsRequest, std::vector<int*>* _vectorWorkerIdRequest) {
        maxWorker = _maxWorker;
        inputSequence = _inputSequence;
        jobsRequest = _jobsRequest;
        vectorWorkerIdRequest = _vectorWorkerIdRequest;
        //this->vectorWorkerIdRequest.resize(*_maxWorker);
        //this->jobsRequest.resize(*_maxWorker);
        //std::vector<int> vectorWorkerIdRequest((int)*maxWorker, 0); //ho messo 0 e non EOS per differenziarlo dal caso finale in cui viene inserito EOS e termina tutto??//TODO pensare a questa cosa

    }

    void nextItem(){
        void* next = 0;
        int counter = 0;
        int workerRequest;

        while((void*)inputSequence->safe_pop(&next) != EOS){
            //vectorWorkerIdRequest[3] = 2;
            //Invece che utilizzare una safequeue ho utilizzato un vettore. Il while scorre finchè non trova un valore diverso da 0
            //vectorWorkerIdRequest->at(1) = 2;
            while(*vectorWorkerIdRequest->at(counter) == -2){//TODO qui il problema è che non viene incrementato il contatore nel momento in cui trova il primo valore != -2
                std::cout<<"VEDIAMO QUANTE VOLTE GIRA STO COUNTER. ATTUALMENTE IL VALORE DI COUNTER è:"+std::to_string(counter)<<std::endl;
                if(counter == ((int)*maxWorker)) counter = 0;
                counter += 1;
                //std::cout<<"qui si"<<std::endl;//dovrà stampare "Nessuna richiesta dal worker"
            }
            workerRequest = *vectorWorkerIdRequest->at(counter);
            this->jobsRequest->at(workerRequest)->safe_push(next); //assegno al worker
            std::cout<<"eseguito il push di: " + std::to_string(*(int*)next)+" alla lista "+std::to_string(workerRequest)<<std::endl;
            counter +=1;
            if(counter == ((int)*maxWorker)) counter = 0;
            //std::cout<<*(int*)next;
            //std::cout<<" alla lista ";
            //std::cout<<workerRequest<<std::endl;

        }
        for(int i = 0; i < jobsRequest->size(); i++) {
            this->jobsRequest->at(i)->safe_push(EOS);
        }
    }

    void startEmitter() {
        this->thEmitter = new std::thread(&Emitter::nextItem, this);
        //this->joinEmitter();
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
    std::thread* thWorker;
    std::vector<int*>* vectorWorkerIdRequest;
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

    Worker(int _workerId, Safe_Queue* _jobsRequest, std::vector<int*>* _vectorWorkerIdRequest, Safe_Queue* _outputQueue){
        workerId = new int(_workerId);
        jobsRequest = _jobsRequest;
        vectorWorkerIdRequest = _vectorWorkerIdRequest;
        outputQueue = _outputQueue;
    }

    void main(){
        while(true){
            vectorWorkerIdRequest->at((*workerId)) = ((int*)workerId);
            //inputWorkerQueue->safe_push(&workerId);
            void* val = 0;
            jobsRequest->safe_pop(&val);
            std::cout<<"Il valore assegnato è: "+std::to_string(*(int*)val)<<std::endl;
            //sstd::cout<<*(int*)val<<std::endl;
            //se val è EOS Stampo nell'output stream eos ed esco dal while
            if(*(int*)val == -1){
                std::cout<<"il valore è EOS!!!!!!"<<std::endl;
                outputQueue->safe_push(EOS);
                break;
            }
            std::cout<<"questo calcolo viene eseguito dal thread numero: "+std::to_string(*workerId)<<std::endl;
            //std::cout<<*workerId<<std::endl;
            void* res = testOdd(&val);//qui devo chiamare una void non deve restituirmi valore
            //outputQueue->safe_push(res);
        }
        std::cout<<"furi dal while "<<std::endl;

    }

    void startWorker() {
        std::cout<<"Worker "+std::to_string(*workerId)+" Runnato"<<std::endl;
        this->thWorker = new std::thread(&Worker::main, this);
        //this->joinWorker();
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
    Safe_Queue* initSequence; //sequenza di void mandata dall'utente
    Safe_Queue* outputSequence;//TODO per ora è una safequeue, poi diventerà un vettore con le lock per ridurre overhead
    std::vector<Safe_Queue*>* jobsRequest = new std::vector<Safe_Queue*>();
    std::vector<Worker*>* workerVector = new std::vector<Worker*>();
    std::vector<int*>* vectorWorkerIdRequest = new std::vector<int*>();
public:
    Controller(int* _maxWorker, int* _initWorker, Safe_Queue* _queue){
        maxWorker = _maxWorker;
        initWorker = _initWorker;
        initSequence = _queue;

        this->jobsRequest->resize(*maxWorker);
        this->vectorWorkerIdRequest->resize(*_maxWorker, new int(-2));
        this->workerVector->resize(*_maxWorker);
        this->outputSequence = new Safe_Queue(_queue->safe_get_size());
        for (int i = 0; i < *_maxWorker; ++i) {
            Safe_Queue* q = new Safe_Queue(1);
            jobsRequest->at(i) = q;
            std::cout<<"safequeue creata all'indirizzo ";
            std::cout<< jobsRequest->at(i) << std::endl;
        }
    }

    void start(){
        int workerId = 0;
        Emitter emitter(maxWorker, initSequence, jobsRequest, vectorWorkerIdRequest);
        //std::vector<Worker> workerVector[*numWorker];
        for (int i = 0; i < *maxWorker; i+=1) {
            //int* workerId = new int(0);
            //Worker* worker = new Worker(workerId, jobsRequest->at(workerId), vectorWorkerIdRequest, outputSequence);
            //this->workerVector->push_back(worker);
            this->workerVector->at(workerId) = new Worker(workerId, jobsRequest->at(workerId), vectorWorkerIdRequest, outputSequence);
            //this->workerVector->at(workerId) = (worker);
            workerId += 1;
        }

        emitter.startEmitter();
        for (int i = 0; i < *maxWorker; ++i) {
            workerVector->at(i)->startWorker();
        }
        for (int i = 0; i < *maxWorker; ++i) {
            workerVector->at(i)->joinWorker();
        }
    }

    void startController() {
        this->thController = new std::thread(&Controller::start, this);
        this->joinController();
    }

    void joinController(){
        thController->join();
    }
};

int main() {
    int* maxWorker= new int(3);
    int* initWorker= new int(3);
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

    Controller ctrl(maxWorker, initWorker, testSequence);
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
