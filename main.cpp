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
            std::cout<<(*((int*) next))<<std::endl;
            std::cout<<(*((int*) next))<<std::endl;
            std::cout<<"emitter"<<std::endl;
            //vectorWorkerIdRequest[3] = 2;
            //Invece che utilizzare una safequeue ho utilizzato un vettore. Il while scorre finchè non trova un valore diverso da 0
            //vectorWorkerIdRequest->at(1) = 2;
            while(vectorWorkerIdRequest->at(counter) == 0){
                if(counter == ((int)*maxWorker) - 1) counter = 0;
                counter += 1;
                //std::cout<<"qui si"<<std::endl;//dovrà stampare "Nessuna richiesta dal worker"
            }
            workerRequest = *vectorWorkerIdRequest->at(counter);//TODO a me NON sembra che qui sia necessario usare i puntatori
            std::cout<<"qui non dovrei arrivarci mai"<<std::endl;
            this->jobsRequest->at(workerRequest)->safe_push(next); //assegno al worker//TODO decommentare
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
    void* workerId; //assegnato dal controller
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

    Worker(int* _workerId, Safe_Queue* _jobsRequest, std::vector<int*>* _vectorWorkerIdRequest){
        workerId = _workerId;
        jobsRequest = _jobsRequest;
        vectorWorkerIdRequest = _vectorWorkerIdRequest;
        std::cout<<"worker"<<std::endl;
    }

    void main(){
        std::cout<<"Nel main ci entri?"<<std::endl;
        while(true){
            std::cout<<"MAIN DEL worker"<<std::endl;
            vectorWorkerIdRequest->at(*(int*)workerId) = (int*)workerId;
            //inputWorkerQueue->safe_push(&workerId);
            void* val = 0;
            jobsRequest->safe_pop(&val);
            //se val è EOS Stampo nell'output stream eos ed esco dal while
            if(val == EOS){
                outputQueue->safe_push(EOS);
                break;
            }
            auto res = testOdd(&val);//qui devo chiamare una void non deve restituirmi valore
            outputQueue->safe_push(res);
        }
    }

    void startWorker() {
        std::cout<<"startworker??"<<std::endl;
        this->thWorker = new std::thread(&Worker::main, this);
        this->joinWorker();
    }

    void joinWorker(){
        thWorker->join();
    }
};


class Controller{
private:
    std::thread* thController;
    int* maxWorker;
    int* initWorker;
    Safe_Queue* initSequence; //sequenza di void mandata dall'utente
    std::vector<Safe_Queue*>* jobsRequest = new std::vector<Safe_Queue*>();
    std::vector<Worker*>* workerVector = new std::vector<Worker*>();
    std::vector<int*>* vectorWorkerIdRequest = new std::vector<int*>();
public:
    Controller(int* _maxWorker, int* _initWorker, Safe_Queue* _queue){
        maxWorker = _maxWorker;
        initWorker = _initWorker;
        initSequence = _queue;

        this->jobsRequest->resize(*maxWorker);
        this->vectorWorkerIdRequest->resize(*_maxWorker);
        this->workerVector->resize(*_maxWorker);
        std::cout<<_maxWorker<<std::endl;
        std::cout<<*_maxWorker<<std::endl;
        for (int i = 0; i < *_maxWorker; ++i) {
            Safe_Queue* q = new Safe_Queue(1);
            jobsRequest->at(i) = q;
            std::cout<<"safequeue creata all'indirizzo"<< std::endl;
            std::cout<< jobsRequest->at(i) << std::endl;
        }
    }

    void start(){
        Emitter emitter(maxWorker, initSequence, jobsRequest, vectorWorkerIdRequest);
        //std::vector<Worker> workerVector[*numWorker];
        for (int* i = new int(0); *i < *maxWorker; *i+=1) {
            std::cout<<"dehdeh"<<std::endl;
            std::cout<<jobsRequest<<std::endl;
            Worker* worker = new Worker(i, jobsRequest->at(*i), vectorWorkerIdRequest);
            std::cout<<"qui?"<<std::endl;
            std::cout<<"Voglio indirizzi della classe"<<std::endl;
            std::cout<<"Voglio indirizzi della classe"<<std::endl;
            std::cout<<"Voglio indirizzi della classe"<<std::endl;
            std::cout<<"Voglio indirizzi della classe"<<std::endl;
            std::cout<<"Voglio indirizzi della classe"<<std::endl;
            std::cout<<worker<<std::endl;
            this->workerVector->push_back(worker);
            std::cout<<"controcheck"<<std::endl;
            std::cout<<"controcheck"<<std::endl;
            std::cout<<"controcheck"<<std::endl;
            std::cout<<"controcheck"<<std::endl;
            std::cout<<workerVector->at(*i)<<std::endl;
            std::cout<<workerVector->size()<<std::endl;
            std::cout<<"deh"<<std::endl;
            //this->workerVector.at(*i) = worker;
            //this->workerVector.at(*i).startWorker();
        }

        for (int j = 0; j < workerVector->size(); ++j) {
            std::cout<<workerVector->at(j)<<std::endl;
        }

        emitter.startEmitter();
        for (int i = 0; i < *maxWorker; ++i) {
            //std::cout<<"Questo è il for dei worker"<<std::endl;
            //workerVector->at(i)->startWorker();
            //std::cout<< workerVector->at(i)<<std::endl;
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
    int* maxWorker= new int(2);
    int* initWorker= new int(2);
    Safe_Queue* testSequence;
    testSequence = new Safe_Queue(10);
    for(int i = 0; i < 10; i++){
        int* val = new int(rand() % 100 + 1);
        testSequence->safe_push(val);
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
