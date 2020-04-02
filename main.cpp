#include <iostream>
#include <thread>
#include <vector>
#include <queue>
#include "./buffers/safe_queue.h"
#include <math.h>
#include <fstream>
#include <sstream>

#define EOS ((void*)-1)

class Worker{
private://TODO aggiungere atomic con service time e funzione per leggerlo dal controller
    int* workerId; //assegnato dal controller
    Safe_Queue* outputQueue;
    Safe_Queue* jobsRequest;//viene creata dal controller
    Safe_Queue* serviceTime;
    std::thread* thWorker;
    //std::vector<int*>* vectorWorkerIdRequest;
    Safe_Queue* workerIdRequest;
    int active_worker;
    int* eos = new int(-1);
    std::mutex* status_mutex;
    std::condition_variable* status_condition;
    std::atomic<size_t> actualTime;

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
           // std::cout<<act<<" nbvfjbjfbjfbjf "<<time<<std::endl;
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

public:

    Worker(int _workerId, Safe_Queue* _jobsRequest, Safe_Queue* _workerIdRequest, Safe_Queue* _outputQueue, Safe_Queue* _serviceTime){
        workerId = new int(_workerId);
        jobsRequest = _jobsRequest;
        workerIdRequest = _workerIdRequest;
        outputQueue = _outputQueue;
        serviceTime = _serviceTime;
        status_mutex = new std::mutex();
        status_condition = new std::condition_variable();
        active_worker = 0; //di default il worker non è attivo, viene attivato quando viene chiamata la funzione startworker

    }

    void main(){

        while(checkWorkerStatus() == 1){
            //std::cout<<"Sono entrato, la coda è: "<<jobsRequest->safe_empty()<<std::endl;
            //In teoria ad inizio while la coda è vuota, perchè ho fatto il pop precedentemente. Se è piena significa che qualcuno ci ha pushato eos
            if(jobsRequest->safe_empty() == 1){
                //vectorWorkerIdRequest->at((*workerId)) = ((int*)workerId);
                workerIdRequest->safe_push((void*)workerId);
                //std::cout<<"DOVREI AVER FATTO IL PUSH ALL'EMITTER DI: "<<*workerId<<std::endl;
                //std::cout<<"push da parte del worker "<<*workerId<<" eseguito"<<std::endl;//TODO decommentar
                void* val = 0;
                jobsRequest->safe_pop(&val);
                std::cout<<"sono nel worker e questo è il valore che arriva: "<<*(size_t*)val<<std::endl;//OK
                //std::cout<<"SONO IL WORKER ho eseguito il pop di: "<<*(int*)val<<std::endl;
                auto start_time = std::chrono::high_resolution_clock::now();
                //std::cout<<"Il valore assegnato è: "+std::to_string(*(int*)val)<<std::endl;//TODO decommentar
                if(*(int*)val == -1){
                    //std::cout<<"il valore è EOS!!!!!!"<<std::endl;//se val è EOS Stampo nell'output stream eos ed esco dal while
                    outputQueue->safe_push(eos);
                    long* eos = new long(-1);//TODO orribile, ma non va con EOS, da vedere
                    //serviceTime->safe_push(eos);
                    disactivate();
                    break;
                }
                //std::cout<<"questo calcolo viene eseguito dal thread numero: "+std::to_string(*workerId)<<std::endl;//TODO decommentare
                long* res = new long(busy_wait(*(size_t*)val));//TODO non so se ha senso castare qua  e il new long mi pare una porcata.
                ssize_t &r = (*((ssize_t*) res));
                auto end_time = std::chrono::high_resolution_clock::now();
                size_t act_service_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count()+1;
                actualTime = act_service_time;
                std::cout<<"Tempo stampato dal worker"<<actualTime<<std::endl;
                outputQueue->safe_push(&r);
            }else{
                void* val = 0;
                jobsRequest->safe_pop(&val);
                std::cout<<"Il valore dovrebbe essere menouno, proviamo: "<<*(int*)val<<std::endl;
                //auto start_time = std::chrono::high_resolution_clock::now();
                //std::cout<<"Il valore assegnato è: "+std::to_string(*(int*)val)<<std::endl;//TODO decommentar
                if(*(int*)val == -1){
                    //std::cout<<"il valore è EOS!!!!!!"<<std::endl;//se val è EOS Stampo nell'output stream eos ed esco dal while
                    outputQueue->safe_push(eos);
                    size_t* eos = new size_t(-1);//TODO orribile, ma non va con EOS, da vedere
                    //serviceTime->safe_push(eos);
                    disactivate();
                    actualTime = *eos;
                    break;
                }

                outputQueue->safe_push(eos);
            }

        }
    }

    void activate(){
        //0:non_attivo
        //1: attivo
        //2: termina
        std::unique_lock<std::mutex> lock(*status_mutex);
        active_worker = 1;
        //status_condition->notify_one();
        notify();
        std::cout<<"worker "<<*workerId<<" attivato"<<std::endl;
    }

    void notify(){
        status_condition->notify_one();
        std::cout<<"Notificato"<<std::endl;
    }

    void disactivate(){
        std::unique_lock<std::mutex> lock(*status_mutex);
        active_worker = 0;
        status_condition->notify_one();
        std::cout<<"worker "<<*workerId<<" disattivato"<<std::endl;
    }

    size_t getActualTime(){
        return actualTime;
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

class Emitter{
private:
    size_t* eos = new size_t(-1);
    int maxWorker;
    std::thread* thEmitter;
    std::vector<size_t> inputSequence;
    //Safe_Queue* inputSequence; //sequenza di interi, sarebbe lo stream iniziale
    //Safe_Queue* emitterTime;
    //std::vector<int*>* vectorWorkerIdRequest;//unidiretional from worker to emitter. Vettore con tutte le richieste fatte dai worker
    Safe_Queue* workerIdRequest;
    std::vector<Safe_Queue*>* jobsRequest; //unidiretional from emitter to worker. Assegnazione dei jobs ai worker
    std::vector<Worker*>* workerVector = new std::vector<Worker*>();

    int* defaultWorkerVector = new int(-2);
    int* EOSCounter = new int(0);
    std::atomic<size_t> emitterTime;
public:
    Emitter(int _maxWorker, std::vector<size_t> _inputSequence, std::vector<Safe_Queue*>* _jobsRequest, Safe_Queue* _workerIdRequest, std::vector<Worker*>* _workerVector) {
        maxWorker = _maxWorker;
        inputSequence = _inputSequence;
        jobsRequest = _jobsRequest;
        //vectorWorkerIdRequest = _vectorWorkerIdRequest;
        workerIdRequest = _workerIdRequest;
        //emitterTime = _emitterTime;
        workerVector = _workerVector;
    }

    void nextItem(){
        size_t next = 0;
        void* workerRequest = 0;
        int counter = 0;
        //inputSequence->safe_pop(&next);
        next = inputSequence[counter];
        while(next != *eos){//FIXME non so che problemi ha ma non tira fuori il -1
            std::cout<<"VALORE ARRIVATO = "<<next<<std::endl;
            counter += 1;
            workerIdRequest->safe_pop(&workerRequest);
            auto start_time = std::chrono::high_resolution_clock::now();
            this->jobsRequest->at(*(int*)workerRequest)->safe_push(new size_t(next)); //assegno al worker
            //std::cout << "eseguito il push di: "<<(*(int *) next)<< " alla lista "<<*(int*)workerRequest<< std::endl; //TODO decommentar
            //inputSequence->safe_pop(&next);
            next = inputSequence[counter];
            auto end_time = std::chrono::high_resolution_clock::now();
            size_t act_service_time_worker = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
            size_t *stw = new size_t(act_service_time_worker);//TODO cotinua ad essere orribile anche così
            //emitterTime->safe_push(stw);
            emitterTime = act_service_time_worker;
        }
        for(int i = 0; i < jobsRequest->size(); i++) {
            emitterTime = *eos;
            this->jobsRequest->at(i)->safe_push(eos);
        }

        for(int i = 0; i < maxWorker; i++) {
            //std::cout<<"attivo tutti"<<std::endl;
            workerVector->at(i)->activate();
            //workerVector->at(i)->notify();
        }
    }

    size_t getActualTime(){
        return emitterTime;
    }

    void startEmitter() {
        this->thEmitter = new std::thread(&Emitter::nextItem, this);
    }

    void joinEmitter(){
        thEmitter->join();
    }
};

class Collector{
private:
    std::thread* thCollector;
    Safe_Queue* outputQueue;
    int* EOSCounter = new int(0);
    size_t* eos = new size_t(-1);
    int maxWorker;
    std::vector<long*> out;//TODO cambiare in size_t
    std::atomic<size_t> collectorTime;
public:
    Collector(Safe_Queue* _outputQueue, int _maxworker){
        outputQueue = _outputQueue;
        maxWorker = _maxworker;
    }

    void main(){
        void* output = 0;
        //outputQueue->safe_pop(&output);
        //while (*EOSCounter < *actualWorker){
        while (*EOSCounter < maxWorker){
            outputQueue->safe_pop(&output);
            auto start_time = std::chrono::high_resolution_clock::now();
            if((*(int*)output) == *eos){
                *EOSCounter += 1;
            }else{
                out.push_back((long*)output);
            }
            auto end_time = std::chrono::high_resolution_clock::now();
            size_t act_service_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
            collectorTime = act_service_time;

        }
        collectorTime = *eos;
    }

    size_t getActualTime(){
        return collectorTime;
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
    std::ofstream to_save;
    std::thread* thController;
    int maxWorker;//TODO passa come valore (tutti i valori passati dall'utente)
    int initWorker;
    int* actualWorker = new int(0); //TODO metti atomic
    int movingAverageParam;
    size_t* averageTime = new size_t(0);
    size_t expectedServiceTime;
    size_t upperBoundServiceTime;
    size_t lowerBoundServiceTime;

    std::vector<size_t> initSequence;
    //Safe_Queue* initSequence; //sequenza di void mandata dall'utente
    Safe_Queue* outputSequence;

    Safe_Queue* serviceTime;//TODO tenere una variabile atomica per ogni worker per controllare se il valore nel vettore lo posso prendere. gestire coda valori con vettore
    Safe_Queue* emitterTime; //TODO tenere in considerazione anche collector
    size_t* eos = new size_t(-1);

    std::queue<size_t>* nw_series;

    size_t pos = 0, acc = 0;

    //std::mutex* actual_worker_mutex;
    //std::condition_variable* actual_worker_condition;

    std::vector<Safe_Queue*>* jobsRequest = new std::vector<Safe_Queue*>();
    std::vector<Worker*>* workerVector = new std::vector<Worker*>();
    //std::vector<int*>* vectorWorkerIdRequest = new std::vector<int*>();//TODO safequeue fare osservazione nella relazione
    Safe_Queue* workerIdRequest;
public:
    Controller(int _maxWorker, int _initWorker, std::vector<size_t> _queue, int _movingAverageParam, size_t _expectedTS, size_t _perc){
        maxWorker = _maxWorker;
        initWorker = _initWorker;
        initSequence = _queue;
        *actualWorker = initWorker;
        movingAverageParam = _movingAverageParam;
        //serviceTime = new Safe_Queue(2*(*_movingAverageParam));//TODO qui ho scelto 2 volte il valore della media scelto dall'utente, dovrei considerare se la media è minore dei maxworker in modo tale da prendere 2 volte i worker in caso?
        expectedServiceTime = _expectedTS;
        upperBoundServiceTime = _expectedTS + _expectedTS * _perc /100;
        lowerBoundServiceTime = _expectedTS - _expectedTS * _perc /100;

        this->nw_series =  new std::queue<size_t>();


        //actual_worker_mutex = new std::mutex();
        //actual_worker_condition = new std::condition_variable();

        this->jobsRequest->resize(maxWorker);
        //this->vectorWorkerIdRequest->resize(*_maxWorker, new int(-2));
        this->workerVector->resize(_maxWorker);
        this->outputSequence = new Safe_Queue(_queue.size());//TODO mettere val a mano
        this->serviceTime = new Safe_Queue(2*(_movingAverageParam));//TODO
        this->emitterTime = new Safe_Queue(2*(_movingAverageParam));//TODO uguale a sopra
        this->workerIdRequest = new Safe_Queue(maxWorker);
        std::ostringstream file_name_stream;

        //this->to_save.open("./data/"+file_name_stream.str());
        this->to_save.open("./data/bl.csv");
        if(this->to_save.is_open())
            this->to_save << expectedServiceTime << "\n" << upperBoundServiceTime << "\n" << lowerBoundServiceTime << "\n" << "Degree,Service_Time,Time\n";

        for (int i = 0; i < _maxWorker; ++i) {
            Safe_Queue* q = new Safe_Queue(1);
            jobsRequest->at(i) = q;
            //std::cout<<"safequeue creata all'indirizzo ";
            //std::cout<< jobsRequest->at(i) << std::endl;
        }
    }

    void start(){
        int workerId = 0;
        int* EOSCounter = new int(0);
        Emitter emitter(maxWorker, initSequence, jobsRequest, workerIdRequest, workerVector);
        Collector collector(outputSequence, maxWorker);
        for (int i = 0; i < maxWorker; i+=1) {
            this->workerVector->at(workerId) = new Worker(workerId, jobsRequest->at(workerId), workerIdRequest, outputSequence, serviceTime);
            workerId += 1;
        }

        emitter.startEmitter();
        for (int i = 0; i < maxWorker; ++i) {
            workerVector->at(i)->startWorker();
        }
        for (int i = 0; i < initWorker; ++i) {
            workerVector->at(i)->activate();
        }
        collector.startCollector();

        //std::cout<<"Gli start li ho fatti tutti"<<std::endl;
        void* actualTime = 0;
        void* actualEmitterTime = 0;
        int averageCounter = 0;
        size_t workersServiceTime = 0;
        size_t emitterServiceTime = 0;
        size_t collectorServiceTime = 0;
        //std::this_thread::sleep_for(std::chrono::milliseconds(10000));//TODO decomentare sleep

        size_t farm_ts = 0, time = 0, rest = 200, Te = 0, Tc = 0, Tw = 0; //rest is the time to be waited to perform next parallelism degree adjustment
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        while (Tc != *eos){
            std::this_thread::sleep_for(std::chrono::milliseconds(rest));
            for (int i = 0; i < *actualWorker; ++i) {
                Tw += workerVector->at(i)->getActualTime();
            }
            //Tw /= (maxWorker*(*actualWorker)); //TODO controllare se questa formula, come tutte le altre, vanno bene
            std::cout<<"TW "<<Tw<<std::endl;
            Tw = Tw / ((*actualWorker)); //TODO controllare se questa formula, come tutte le altre, vanno bene
            //Tw = maximum(workersServiceTime/movingAverageParam, collectorServiceTime/movingAverageParam, workersServiceTime/(2*(*actualWorker)));
            Te = emitter.getActualTime();
            Tc = collector.getActualTime();
            farm_ts = maximum(Tc, Te, Tw/(*actualWorker));
            std::cout<<"actualWorker, farmTs, Time, Tw, Te, Tc "<< *actualWorker << "," << farm_ts << "," << time << ","<< Tw << ","<< Te << ","<< Tc <<std::endl;
            if(this->to_save.is_open()) //save data in ./data
                this->to_save << *actualWorker << "," << farm_ts << "," << time << "\n";
            this->concurrency_throttling(Tw, Te, Tc);
            time+=rest;
            Tw = 0;
        }
        to_save.close();

        std::cout<<"JOIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIn"<<std::endl;
        emitter.joinEmitter();
        for (int i = 0; i < maxWorker; ++i) {
            workerVector->at(i)->joinWorker();
        }
        collector.joinCollector();
    }

    void concurrency_throttling(size_t Tw, size_t Te, size_t Tc){
        if(Tw < Te || Tw < Tc) {
            return; //In the case that Tw is less than Collector Service_Time and Emitter Service_Time, the concurrency_throttling can't do nothing
        }
        size_t nw = *actualWorker;
        if(Tw > upperBoundServiceTime){ //Increase the degree -> Service_Time is getting higher than Ts_goal
            nw = Tw/expectedServiceTime;
            nw = (nw <= maxWorker) ? nw : maxWorker;
        }
        else if(Tw < lowerBoundServiceTime){ //Decrese the degree -> Service_Time is getting lower than Ts_goal
            nw = expectedServiceTime/Tw;
            nw = (nw < *actualWorker) ? nw : *actualWorker -1;//TODO prima era *actualWorker -1, ricontrollare
            //nw--;
        }
        /*else if(Tw > this->ts_goal && Tw <= this->ts_upper_bound){ //Whether the Tw (Worker Service_Time) is close to ts_goal, try to detect bottlenecks, in case update the degree only if doesn't increase too much the performance
            nw = Tw/this->ts_goal;
            nw = (nw <= this->max_nw) ? nw : this->max_nw;
        }*/
        this->update_nw_moving_avg(nw);
        //this->get_nw_moving_avg();
        this->update_thread(this->get_nw_moving_avg());//TODO cambia nomi delle funzioni

        return;
    }

    void update_thread(size_t new_nw){
        //if(new_nw > this->*actualWorker &&  this->maxWorker - new_nw <= this->idle_contexts.size()){
        if(new_nw > *actualWorker &&  new_nw <= maxWorker){
            //size_t to_add = new_nw - this->active_contexts.size();
            for(auto i = *actualWorker; i < new_nw; i++) { //add as many workers as needed to the already active
                workerVector->at(i)->activate();
                *actualWorker = new_nw;
            }
        }
        else if(new_nw < *actualWorker && new_nw > 0){ //remove as many workers as needed to the already active
            //size_t to_remove = this->active_contexts.size() - new_nw;
            for(auto i = *actualWorker - 1; i > new_nw; i--) {
                workerVector->at(i)->disactivate();
                *actualWorker = new_nw;
            }
        }
        std::cout<<"I WORKER SONO DIVENTATI "<<*actualWorker<<std::endl;

        //this->redistribute(); //load balance the workers across all the active_contexts //TODO IO QUESTO NON LO FACCIO, controllare
    }

    void update_nw_moving_avg(size_t new_value){
        this->acc += new_value;
        this->nw_series->push(new_value);
        if(this->pos <= movingAverageParam)
            this->pos++;
        else{
            this->acc -= this->nw_series->front();
            this->nw_series->pop();
        }
        //std::cout<<"Qui i valori di pos e acc sono: "<<pos<<","<<acc<<std::endl;
        return;
    }

    size_t get_nw_moving_avg(){
        return this->acc/this->pos;
    }

    size_t maximum( size_t a, size_t b, size_t c )
    {
        size_t max = ( a < b ) ? b : a;
        return ( ( max < c ) ? c : max );
    }

    void startController() {
        this->thController = new std::thread(&Controller::start, this);
    }

    void joinController(){
        thController->join();
    }
};

int main() {



    int (*function_pointer)(int a , int b);

    int dim = 10000;

    int maxWorker= 8;
    int initWorker = 1;//TODO controllare perchè con 1 ad una certa non va. nemmeno 2-2
    int movingAverageParam = 1;
    size_t expectedServiceTime = 4000;//190350621,2001985 //PROVARE SU MACCHINA XEON 208000
    size_t perc = 1;
    size_t eos = -1;

    //Safe_Queue* testSequence;
    std::vector<size_t> testSequence;
    //testSequence = new Safe_Queue(dim + maxWorker);
    size_t val1 = (size_t)16000;

    for(int i = 0; i < dim/3; i++){
        testSequence.push_back(val1);
    }
    size_t val2 = (size_t)4000;

    for(int i = dim/3+1; i < 2*dim/3; i++){
        testSequence.push_back(val2);
    }
    size_t val3 = (size_t)32000;
    for(int i = 2*dim/3; i < dim; i++){
        testSequence.push_back(val3);
    }
    /*for(int i = 0; i < dim; i++){
        int* val = new int(rand() % 100000000000 + 1);
        testSequence->safe_push(val);
    }*/

    /*for(int i = 0; i < dim; i++){
        void* bl = 0;
        testSequence->safe_pop(&bl);
        std::cout<<*(int*)bl<<std::endl;
    }*/

    testSequence.push_back(eos);
    //std::atomic<int> testATOMIC;
    //std::cout<<testATOMIC<<std::endl;
    /*for(int i = 0; i < 15; i++){
        void* val = 0;
        testSequence->safe_pop(&val);
        std::cout<<(*((int*) val))<<std::endl;
    }*/

    /*for (int j = 0; j < *maxWorker; ++j) {
        testSequence->safe_push(eos);
    }*/

    Controller ctrl(maxWorker, initWorker, testSequence, movingAverageParam, expectedServiceTime, perc);
    ctrl.start();

    return 0;
}