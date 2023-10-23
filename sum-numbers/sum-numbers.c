#include "sum-numbers.h"

/*Per eseguire il programma

mpiexec -np <numero-processori> sum-numbers -s <1,2,3> -n <intero> [-d] [file.txt]
    -s -> indica il tipo di strategia
    -n -> il numero di elementi da sommare
    -d -> opzionale, i valori da sommare sono tutti 1
    file.txt -> opzionale, file da cui recuperare i valori da sommare
*/

int main(int argc, char **argv) {
    //-------------------------- Variabili di controllo --------------------------
    MPI_Status mpiStatus;
    Utils utilsForProcessor;

    //-------------------------- Utils -------------------------------------------
    Input inputType = NONE; //Di default non è assegnato
    int i;

    //Tempo di inizio, tempo di fine, tempo impiegato da un processore, tempo risultante
    double t0, t1, timeForOneProcessor, finalTime;
    
    //Numero di elementi totali 
    int nElements;
    
    //Array per memorizzare i numeri in tutto
    int *numbers;

    //---------------------------- Inizializzazione ------------------------------

    //Inizializza l'ambiente MPI
    MPI_Init(&argc, &argv);
    //Prende l'id dei processori e lo assegna
    MPI_Comm_rank(MPI_COMM_WORLD, &utilsForProcessor.processorID);
    //Prende il numero di processori 
    MPI_Comm_size(MPI_COMM_WORLD, &utilsForProcessor.nProcessors);

    utilsForProcessor.sum = 0; //Inizializzazione somma

    printf("[PID: %d/%d] Processor initialization\n", utilsForProcessor.processorID, utilsForProcessor.nProcessors);

    //------------------------ Configurazione del problema -----------------------

    //Main processor agisce come se fosse un "controller"
    if (utilsForProcessor.processorID == 0) {

        //Parsing da linea di comando
        if(interpretCommandLine(argc, argv, &utilsForProcessor.strategyType, &inputType, &nElements) != 0) {
            MPI_Abort(MPI_COMM_WORLD, -1);
        }

        //Allocazione dell'array che conterrà i numeri
        numbers =(int*) malloc(sizeof(int) * nElements); 
        

    //------------------------ Recupero input ------------------------------------

        //Controlla se l'input deve essere generato o recuperato
        if(inputType == RANDOM) {
            generateRandomNumbers(numbers, nElements);
        } else if (inputType == FROM_FILE) {
            //Nel caso dovesse essere recuperato da file allora l'ultimo argomento arg[optind] è il nome del file
            retrieveInput(numbers, nElements, argv[optind]);
        } else if (inputType == DEBUG) {
            generate1(numbers, nElements);
        }
    }

    //Main processor manda il numero totale degli elementi per permettere a tutti
    //i processori di calcolarsi il numero di elementi da memorizzare localmente
    //Potremmo mandare la grandezza calcolata da P0 ma poi ci sarebbero molte comunicazioni
    MPI_Bcast(&nElements, 1, MPI_INT, 0, MPI_COMM_WORLD);
    //Main processor manda anche il tipo di strategia da adottare
    MPI_Bcast(&utilsForProcessor.strategyType, 1, MPI_INT, 0, MPI_COMM_WORLD);
    /*****************************************************************************************************************
    *Si sarebbe potuto ottimizzare (evitando di fare 2 comunicazioni separate) mandando una struct che conteneva i   *
    *due valori o un array ma per                                                                                    *
    *rendere il tutto più chiaro e leggibile, si è preferito mandare due messaggi. Anche perchè la valutazione dell' *
    *efficienza dell'algoritmo e svolta dopo la distribuzione dei dati                                               *
    ******************************************************************************************************************/     
    
    //Controlla se la strategia è attuabile e, nel caso lo fosse, calcola le variabili per attuarla
    //N.B: le variabili che richiedono calcoli come il logaritmo vengono calcolate qui per misurare il tempo effetivo dell'
    //algoritmo successivamente
    checkStrategy(&utilsForProcessor);
    if (utilsForProcessor.processorID == 0) {
        printf("---------- PROBLEM AND RESOLUTION CONFIGURATION ----------\n-Input type: %s\n-Number of elements: %d\n-Strategy type: %s\n\n", inputTypeToString(inputType), nElements, strategyToString(utilsForProcessor.strategyType));
    }

    //-------------------------- Distribuzione dati --------------------------

    //Main processor distribuisce dati ai vari processori
    distributeData(&utilsForProcessor, nElements, numbers);

    //------------------------- Inizializzazione timer -----------------------

    MPI_Barrier(MPI_COMM_WORLD);
    t0 = MPI_Wtime(); //Tempo iniziale settato

    //-------------------------- Somma locale --------------------------------

    //Somma locale per ogni processo
    for (i = 0; i < utilsForProcessor.nElementsInOneprocessor; i++) {
        utilsForProcessor.sum = utilsForProcessor.sum + utilsForProcessor.numbersInOneprocessor[i];
    }
    
    //printf("[PID %d] Local sum: %d\n", utilsForProcessor.processorID, utilsForProcessor.sum);
    
    //-------------------------- Applicazione strategia ----------------------

    //Prima strategia in cui tutti i processori mandano le loro somme parziali al main processor
    if (utilsForProcessor.strategyType == FIRST_STRATEGY) {
        firstStrategy(&utilsForProcessor);
    }

    if (utilsForProcessor.strategyType == SECOND_STRATEGY) {
        secondStrategy(&utilsForProcessor);
    }

    if (utilsForProcessor.strategyType == THIRD_STRATEGY) {
        thirdStrategy(&utilsForProcessor);
    }

    //-------------------------- Calcolo del tempo impiegato -----------------

    //Calcolo del tempo finale di ogni processore
    t1 = MPI_Wtime();
    timeForOneProcessor = t1 - t0;
    MPI_Barrier(MPI_COMM_WORLD);

    //Viene salvato il tempo del processore che ha impiegato più tempo,
    //infatti la funzione trova il massimo tra i tempi locali
    MPI_Reduce(&timeForOneProcessor, &finalTime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    // //Per testing 
    // if (utilsForProcessor.processorID == 0) {
    //     printf("%-25s%-20s%-13s%-10s\n", "Number of elements", "Strategy", "Sum", "Time");
    //     printf("%-25d%-20d%-13d%-10.5f\n\n",nElements, utilsForProcessor.strategyType, utilsForProcessor.sum, finalTime);
    // }
    
    //Nel caso non sia la terza strategia allora solo il main processor ha memorizzata la somma totale
    if (utilsForProcessor.strategyType != THIRD_STRATEGY) {
        if (utilsForProcessor.processorID == 0) {
            printf("\n[PID 0] *** RESULT *** Total sum value is %d\n\n", utilsForProcessor.sum);
        }
    } else {
        //Nell terza strategia ogni processore memorizza la somma totale
        printf("\n[PID %d] *** RESULT *** Total sum value is %d\n\n", utilsForProcessor.processorID, utilsForProcessor.sum);
    }
    
    MPI_Finalize();
}




/************************ CORE ***************************/

//Interpreta gli argomenti passati da riga di comando
int interpretCommandLine(int argc, char* argv[], Strategy* strategyType, Input* inputType, int* nElements) {
    int opt, tmp;
    //Per controllare se queste opzioni *necessarie* sono state passate a riga di comando
    int checkS = -1, checkN = -1;
    
    //Lettura delle opzioni e degli argomenti passati a riga di comando
    while ((opt = getopt(argc, argv, ":s:n:i:d")) != -1) {
        switch (opt) {
            //Opzione per la strategia
            case 's':
                checkS = 0;
                tmp = atoi(optarg);
                //Check se l'argomento è un numero ed è compreso tra 1 e 3
                if (isdigit(*optarg) != 0 && tmp > 0 && tmp <= 3) {
                    *strategyType = tmp; 
                } else {
                    printError(0, "Strategy number incorrect. Choose between:\n1 -> First strategy\n2 -> Second strategy\n3 -> Third strategy\nCheck the documentation to understand every strategy or options -h.\n");
                    return -1;
                }
                break;
            //Opzione per il numero di elementi
            case 'n':
                checkN = 0;
                //Check se il numero di elementi da sommare è maggiore o uguale di 2
                if ((tmp = atoi(optarg)) >= 2) {
                    *nElements = tmp;
                    //Se il numero degli elementi è minore o uguale di 20 allora gli input vengono presi da file altrimenti vengono generati
                    //se la strategia non è stata ancora assegnata
                    if(*inputType == NONE) {
                        if(tmp <= 20) {
                            *inputType = FROM_FILE;
                        } else {
                            *inputType = RANDOM;
                        }
                    } //else la strategia è gia stata assegnata ed è DEBUG
                    
                } else {
                    printError(0, "Elements number incorrect. Elements should be almost 2.\n");
                    return -1;
                }
                break;

            case 'd':
                *inputType = DEBUG;
                break; 
                
            case 'h':
                /**************TODO*****************/
                break;
            //Opzione non definita
            case '?':
                printError(0, "One option is unknown. The options are:\n\t-n <number of elements>\n\t-s <strategy type>\n");
                return -1;
        }
    }

    //Check se le opzioni obbligatorie sono state passate
    if (checkN != 0 || checkS != 0) {
        printError(0, "Options missed. Insert:\n\t-n <number of elements>\n\t-s <strategy type>\n");
        return -1;
    }

    return 0;
}

void checkStrategy(Utils* utilsForProcessor) {
    int i;
    
    //Nel caso la strategia fosse la seconda o la terza il numero di processori deve essere potenza di 2
    if(utilsForProcessor->strategyType == SECOND_STRATEGY || utilsForProcessor->strategyType == THIRD_STRATEGY) {
        //Casting implicito poichè logNProcessores è intero mentre log2() restituisce double
        utilsForProcessor->logNProcessors = log2(utilsForProcessor->nProcessors);
        //Nel caso il numero di processori non sia potenza di 2 si applica la prima strategia
        //Controllo se troncando il double rimane uguale (quindi potenza di 2) oppure no
        if (log2(utilsForProcessor->nProcessors) != utilsForProcessor->logNProcessors) {
            utilsForProcessor->strategyType = FIRST_STRATEGY;

            //Stampa unica (main processor) del tipo di input, numero di elementi e strategia
            if (utilsForProcessor->processorID == 0) {
                printf("\n\n*** WARNING ***\nStrategy selected is incompatible with the processors number so the first strategy is set by default\n");
            }
        } else {
            //Memorizza le potenze di 2 in un array in modo che non inficino nel calcolo del tempo dell'algoritmo
            utilsForProcessor->powersOf2 = malloc(sizeof(int) * (utilsForProcessor->logNProcessors + 1));
            for (i = 0; i <= utilsForProcessor->logNProcessors; i++) {
                //Le potenze di 2 vengono calcolate tramite lo shift a sinistra
                utilsForProcessor->powersOf2[i] = (1 << i);
            }
        }
    }
}


void distributeData(Utils* utilsForProcessor, int nElements, int numbers[]) {
    int rest, i, tag;
    MPI_Status mpiStatus;
    
    //------ Distribuzione dei dati ----------
    utilsForProcessor->nElementsInOneprocessor = nElements / utilsForProcessor->nProcessors; //Ogni processore calcola quanti elementi deve memorizzare
    rest = nElements % utilsForProcessor->nProcessors;

    //Controlla se ci sono elementi in più tramite il modulo e
    //incrementa la grandezza dell'array dei primi processori nel caso in cui ci siano elementi in più 
    if(utilsForProcessor->processorID < rest){utilsForProcessor->nElementsInOneprocessor++;}
    
    //Alloca per ogni processore l'array con la giusta grandezza
    utilsForProcessor->numbersInOneprocessor =(int*) malloc(sizeof(int) * utilsForProcessor->nElementsInOneprocessor);
    
    //Main processor manda l'input agli altri processi
    if (utilsForProcessor->processorID == 0) {
        utilsForProcessor->numbersInOneprocessor = numbers;

        //Memorizza quanti elementi deve contenere ogni processo
        int tmpNElements = utilsForProcessor->nElementsInOneprocessor;
        int startIndex = 0;

        //Manda i numeri a tutti i processori con ID i
        for (i = 1; i < utilsForProcessor->nProcessors; i++)
        {
            //Incremento l'indice da cui devo iniziare a mandare i numeri al prossimo processore
            startIndex = startIndex + tmpNElements;
            tag = 10+i;

            /*se i==rest allora il main processor ha terminato la comunciazione
            con i processi che hanno un elemento in più quindi deve correggere la 
            grandezza dell'array per il resto dei processi*/
            tmpNElements = (i==rest) ? tmpNElements-1 : tmpNElements;
            MPI_Send(&numbers[startIndex], tmpNElements, MPI_INT, i, tag, MPI_COMM_WORLD);
        }
    } else { //Altri processi ricevono il puntatore all'array in cui sono memorizzati i numeri
        tag = 10 + utilsForProcessor->processorID;
        MPI_Recv(utilsForProcessor->numbersInOneprocessor, utilsForProcessor->nElementsInOneprocessor, MPI_INT, 0, tag, MPI_COMM_WORLD, &mpiStatus);
        //A questo punto ogni processore ha il suo array locale
    }
}

//Ogni processore manda la sua somma al main processor
void firstStrategy(Utils* utilsForProcessor) {
    int i, tag;
    MPI_Status mpiStatus;
    
    if(utilsForProcessor->processorID != 0) {
        tag = 20 + utilsForProcessor->processorID;
        //I processori mandano la loro somma parziale al main processor
        MPI_Send(&utilsForProcessor->sum, 1, MPI_INT, 0, tag, MPI_COMM_WORLD);
    } else {
        int partialSum;
        for (i = 1; i < utilsForProcessor->nProcessors; i++) {
            tag = 20 + i;
            //Main processor riceve una a una le somme parziali degli altri prcessori
            MPI_Recv(&partialSum, 1, MPI_INT, i, tag, MPI_COMM_WORLD, &mpiStatus);
            //Somma totale calcolata aggiungendo alla somma locale del main processor le altre somme parziali
            utilsForProcessor->sum += partialSum;
        }
    }
} 


void secondStrategy(Utils* utilsForProcessor) {
    int i, tag;
    MPI_Status mpiStatus;
    
    //Incremento dei passi temporali
    for (i = 0; i < utilsForProcessor->logNProcessors; i++) {
        //A ogni passo temporale si controlla quali processi partecipano alla comunicazione
        if (utilsForProcessor->processorID % utilsForProcessor->powersOf2[i] == 0) {
            //Controllo per quali processi devono mandare la somma
            if(utilsForProcessor->processorID % utilsForProcessor->powersOf2[i+1] != 0) {
                tag = 30 + utilsForProcessor->processorID;
                MPI_Send(&utilsForProcessor->sum, 1, MPI_INT, utilsForProcessor->processorID - utilsForProcessor->powersOf2[i], tag, MPI_COMM_WORLD);
            } else {
                int partialSum;
                tag = 30 + utilsForProcessor->processorID + utilsForProcessor->powersOf2[i];
                MPI_Recv(&partialSum, 1, MPI_INT, utilsForProcessor->processorID + utilsForProcessor->powersOf2[i], tag, MPI_COMM_WORLD, &mpiStatus);
                utilsForProcessor->sum += partialSum;
            }
        }
    }
}


void thirdStrategy(Utils* utilsForProcessor) {
    int i, tag, partialSum;
    MPI_Status mpiStatus;

    //Incremento dei passi temporali
    for (i = 0; i < utilsForProcessor->logNProcessors; i++) {
        if ((utilsForProcessor->processorID % utilsForProcessor->powersOf2[i+1]) < utilsForProcessor->powersOf2[i]) {
            tag = 30 + utilsForProcessor->processorID;
            
            MPI_Send(&utilsForProcessor->sum, 1, MPI_INT, utilsForProcessor->processorID + utilsForProcessor->powersOf2[i], tag, MPI_COMM_WORLD);
            //printf("[PID %d] Sent to %d\n ", utilsForProcessor->processorID, utilsForProcessor->processorID + utilsForProcessor->powersOf2[i]);
            
            MPI_Recv(&partialSum, 1, MPI_INT, utilsForProcessor->processorID + utilsForProcessor->powersOf2[i], MPI_ANY_TAG, MPI_COMM_WORLD, &mpiStatus);
            //printf("[PID %d] Received from %d the partial sum %d\n ", utilsForProcessor->processorID, utilsForProcessor->processorID + utilsForProcessor->powersOf2[i], partialSum);
            
            utilsForProcessor->sum += partialSum;
        } else {
            tag = 30 + utilsForProcessor->processorID;

            MPI_Send(&utilsForProcessor->sum, 1, MPI_INT, utilsForProcessor->processorID - utilsForProcessor->powersOf2[i], tag, MPI_COMM_WORLD);
            //printf("[PID %d] Sent to %d\n ", utilsForProcessor->processorID, utilsForProcessor->processorID - utilsForProcessor->powersOf2[i]);
        
            MPI_Recv(&partialSum, 1, MPI_INT, utilsForProcessor->processorID - utilsForProcessor->powersOf2[i], MPI_ANY_TAG, MPI_COMM_WORLD, &mpiStatus);
            //printf("[PID %d] Received from %d the partial sum %d\n ", utilsForProcessor->processorID, utilsForProcessor->processorID - utilsForProcessor->powersOf2[i],partialSum);
            
            utilsForProcessor->sum += partialSum;
        }        
    }
}

/*************************INPUT******************************/
//Genera numeri casuali e li memorizza in numbers
void generateRandomNumbers(int* numbers, int nElements) {
    int i;

    srand(time(0));
    printf("[PID 0] Generated numbers:\n");
    for (i = 0; i < nElements; i++) {
        numbers[i] = rand()%100;
        printf("\tElements %d value is %d\n", i, numbers[i]);
    } 
}

//Recupera input da file
void retrieveInput(int* numbers, int nElements, char* file) {
    int i;
    FILE* myFile = fopen("/homes/DMA/PDC/2020/SCRFNC00B/sum-numbers/input.txt", "r");

    if (myFile == NULL) {
        printError(0, "Can't read file. File name should be input.txt\n");
        MPI_Abort(MPI_COMM_WORLD, -1);
    }
    
    //Legge i numeri sul file e li memorizza in numbers
    for (i = 0; i < nElements; i++) { fscanf(myFile, "%d", &numbers[i]); }
    fclose(myFile);
}

//Memorizza tutti 1
void generate1(int* numbers, int nElements) {
    int i;

    for (i = 0; i < nElements; i++) { numbers[i] = 1; }
}

/************************UTILS***************************/

//Stampa nello standard error l'errore trovato con eventuali indicazioni
void printError(int processorID, char* errorType) {
    fprintf(stderr, "\n[PID %d] **** ERROR ****\n%s\n", processorID, errorType);
}

//Ritorna la stringa che descrive in che modo viene recuperato l'input
char* inputTypeToString(Input inputType) {
    char* inputString;
    switch (inputType)
    {
        case RANDOM:
            inputString = "Randomically generated";
            break;

        case FROM_FILE:
            inputString = "Read from command line";
            break;

        case DEBUG:
            inputString = "Every number has value 1";
            break;
    }
    return inputString;
}

//Ritorna la stringa della strategia adottata
char* strategyToString(Strategy strategyType) {
    char* strategyString;
    switch (strategyType)
    {
        case FIRST_STRATEGY:
            strategyString = "First strategy";
            break;

        case SECOND_STRATEGY:
            strategyString = "Second strategy";
            break;

        case THIRD_STRATEGY:
            strategyString = "Third strategy";
            break;
    }
    return strategyString;
}

