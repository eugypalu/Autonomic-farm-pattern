#include <iostream>
#include <thread>
#include <vector>
#include <queue>
#include <math.h>
#include <fstream>
#include <sstream>

#define EOS ((void*)-1)

class Controller{
private:

    std::ofstream to_save;
    std::thread* thController;

    const std::function<size_t(size_t)> funct;

    //Parametri passati dall'utente
    int maxWorker;
    int initWorker;
    int movingAverageParam;
    size_t expectedServiceTime;
    size_t upperBoundServiceTime;
    size_t lowerBoundServiceTime;
    std::vector<size_t> initSequence;

    //Actual worker tiene il conteggio dei worker attualmente attivi
    std::mutex* actual_worker_mutex;//il mutex è necessario per condividere actual worker con collector e worker
    int* actualWorker = new int(0);

    Safe_Queue* outputSequence;//Coda unidirezionale dal worker verso il collector

    std::queue<size_t>* nw_series;//Coda necessaria per tener conto della media variabile tra i worker
    size_t pos = 0, acc = 0;

    std::vector<Safe_Queue*>* jobsRequest = new std::vector<Safe_Queue*>();//Coda unidirezionale dall'emitter al worker in cui l'emitter inserisce i task
    std::vector<Worker*>* workerVector = new std::vector<Worker*>();//Vettore contenente maxworker worker
    Safe_Queue* workerIdRequest;//Coda in cui ogni worker inserisce il proprio id quando richiede un nuovo task

public:
    Controller(int _maxWorker, int _initWorker, std::vector<size_t> _queue, int _movingAverageParam, size_t _expectedTS, size_t _perc, std::function<size_t(size_t)> _funct):funct(_funct){
        maxWorker = _maxWorker;
        initWorker = _initWorker;
        initSequence = _queue;
        movingAverageParam = _movingAverageParam;
        expectedServiceTime = _expectedTS;

        actual_worker_mutex = new std::mutex();

        //upperbound e lowerbound vengono calcolati incrementando e decrementando l'expected service time di una percentuale definita dall'utente
        upperBoundServiceTime = _expectedTS + _expectedTS * _perc /100;
        lowerBoundServiceTime = _expectedTS - _expectedTS * _perc /100;

        this->nw_series =  new std::queue<size_t>(); //coda necessaria per l'aggiustamento del degree durante l'esecuzione della farm

        this->jobsRequest->resize(maxWorker);//Il vettore di code contenente i task assegnati a ciascun worker ha dimensione =  al numero massimo di worker
        this->workerVector->resize(_maxWorker); //vettore di worker di dimensione maxworker
        this->outputSequence = new Safe_Queue(_queue.size());//coda unidirezzionale verso il collector//TODO mettere val a mano????
        this->workerIdRequest = new Safe_Queue(maxWorker); //coda di dimensione maxworker
        std::ostringstream file_name_stream;

        this->to_save.open("./data/bl.csv");
        if(this->to_save.is_open())
            this->to_save << expectedServiceTime << "\n" << upperBoundServiceTime << "\n" << lowerBoundServiceTime << "\n" << "Degree,Service_Time,Time\n";

        //le code appartenenti ai worker, in cui vengono inseriti i nuovi task, avranno dimensione 1. In questo modo il worker richiede il task lo processa lo manda in output e lo richiede di nuovo
        for (int i = 0; i < _maxWorker; ++i) {
            Safe_Queue* q = new Safe_Queue(1);
            jobsRequest->at(i) = q;
        }
    }

    void start(){
        int workerId = 0;
        Emitter emitter(maxWorker, initSequence, jobsRequest, workerIdRequest, workerVector, actualWorker, actual_worker_mutex);
        Collector collector(outputSequence, maxWorker);
        for (int i = 0; i < maxWorker; i+=1) {
            this->workerVector->at(workerId) = new Worker(workerId, jobsRequest->at(workerId), workerIdRequest, outputSequence, funct, actualWorker, actual_worker_mutex);
            workerId += 1;
        }

        emitter.startEmitter();
        for (int i = 0; i < maxWorker; ++i) {
            workerVector->at(i)->startWorker();
        }
        for (int i = 0; i < initWorker; ++i) {
            workerVector->at(i)->activate();//vengono attivati solo i worker iniziali definiti dall'utente
        }
        collector.startCollector();

        size_t farm_ts = 0, time = 0, rest = 200, Te = 0, Tc = 0, Tw = 0; //rest è il tempo da aspettare per la prossima modifica del degree
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        while (getActualWorker() > 0){//mi fermo quando il numero di worker attivi diventa 0

            std::this_thread::sleep_for(std::chrono::milliseconds(rest));

            for (int i = 0; i < getActualWorker(); ++i) {
                Tw += workerVector->at(i)->getActualTime();
            }

            int actual = getActualWorker();

            if(actual > 0){//controllo anche qui se il valore è > 0 in quanto i worker possono fermarsi autonomamente quando gli arriva un End Of Stream
                Tw = Tw / (actual);
                Te = emitter.getActualTime();
                Tc = collector.getActualTime();
                farm_ts = maximum(Tc, Te, Tw/(actual)); //calcolo il service time della farm
            }else
                break; //se = 0 esco senza aggiornare il concurrency throttling
            std::cout << "Farm_Ts: " << farm_ts << std::endl;
            std::cout << " Degree >> " << getActualWorker() << std::endl;
            if(this->to_save.is_open()) //save data in ./data            if(this->to_save.is_open())
                this->to_save << actual << "," << farm_ts << "," << time << "\n";

            this->concurrency_throttling(Tw, Te, Tc);

            time+=rest;
            Tw = 0;
        }
        to_save.close();

        emitter.joinEmitter();
        for (int i = 0; i < maxWorker; ++i) {
            workerVector->at(i)->joinWorker();
        }
        collector.joinCollector();
    }


    //In base al service time di worker emitter e collector valuta quanti thread sono necessari per raggiungere il service time aspettato
    void concurrency_throttling(size_t Tw, size_t Te, size_t Tc){
        if(Tw < Te || Tw < Tc) {
            return; //Se il time dei worker è minore di quello di collector ed emitter non si possono fare aggiustamenti sul degree
        }

        size_t nw = getActualWorker();

        if(Tw > upperBoundServiceTime){ //in questo caso va aumentato il degree, il service time è > dell'expected service time
            nw = Tw/expectedServiceTime; //calcolo quanti nuovi thread sono necessari per abbassare il ts
            nw = (nw <= maxWorker) ? nw : maxWorker;
        }
        else if(Tw < lowerBoundServiceTime){ //Abbassare il degree, il service time è < dell'expected service time
            nw = expectedServiceTime/Tw; //calcolo quanti thread vanno fermati per alzare il ts
            nw = (nw < getActualWorker()) ? nw : getActualWorker() -1;//TODO prima era *actualWorker -1, ricontrollare
        }

        this->update_nw_moving_avg(nw);
        this->update_thread(this->get_nw_moving_avg());//TODO cambia nomi delle funzioni

        return;
    }

    //Si occupa dell'effettivo stop/start dei thread, basandosi sul risultato di update_nw_moving_avg
    void update_thread(size_t new_nw){
        if(new_nw > getActualWorker() &&  new_nw <= maxWorker){
            for(auto i = getActualWorker(); i < new_nw; i++) {
                workerVector->at(i)->activate();
            }
        }
        else if(new_nw < getActualWorker() && new_nw > 0){
            for(auto i = getActualWorker() - 1; i > new_nw; i--) {
                workerVector->at(i)->disactivate();
            }
        }
    }

    //Tenendo conto degli ultimi n valori dei thread, permette di stabilire quale dovrà essere il prossimo valore del degree
    void update_nw_moving_avg(size_t new_value){
        this->acc += new_value;
        this->nw_series->push(new_value);
        if(this->pos <= movingAverageParam)//finchè pos è minore del valore impostato dall'utente si continua ad incrementare il valore totale di acc
            this->pos++;
        else{ //Nel caso in cui si raggiunge il valore definito dall'utente si aggiunge il nuovo valore, si decrementa del valore più vecchio e si elimina il vecchio
            this->acc -= this->nw_series->front();
            this->nw_series->pop();
        }
        return;
    }

    size_t get_nw_moving_avg(){
        return this->acc/this->pos;
    }


    size_t maximum( size_t a, size_t b, size_t c ){
        size_t max = ( a < b ) ? b : a;
        return ( ( max < c ) ? c : max );
    }

    //restituisce il numero di worker attualmente attivi
    int getActualWorker(){
        actual_worker_mutex->lock();
        int acWork = *actualWorker;
        actual_worker_mutex->unlock();
        return acWork;
    }

    //start del thread
    void startController() {
        this->thController = new std::thread(&Controller::start, this);
    }

    //join del thread
    void joinController(){
        thController->join();
    }
};