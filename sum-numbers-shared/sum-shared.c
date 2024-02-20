#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <getopt.h>
#include <ctype.h>

//Genera numeri casuali e li memorizza in numbers
void generateRandomNumbers(int* numbers, int nElements) {
    int i;

    srand(time(0));
    printf("[TID %d] Generated numbers:\n", omp_get_thread_num());
    for (i = 0; i < nElements; i++) {
        numbers[i] = rand()%100;
        printf("\tElements %d value is %d\n", i, numbers[i]);
    } 
}

void printError(int threadID, char* errorType) {
    fprintf(stderr, "\n[TID %d] **** ERROR ****\n%s\n", threadID, errorType);
}

//Calcolo della somma parallelamente con memoria condivisa
int computeSum(int* numbers, int n) {
    int sumtot = 0, i;

    //Inizio della fork operation
    #pragma omp parallel
    {
        printf("[TID %d] Thread initialized.\n", omp_get_num_threads());
        //Divide le iterazioni dei for tra i thread 
        //Viene effettuata un'operazione di riduzione e sumtot 
        //contiene il valore finale della computazione
        #pragma omp for reduction(+ : sumtot)
        for (i = 0; i < n; i++) {
            sumtot = sumtot + numbers[i];
        }
    }
    return sumtot;
}

//Interpreta gli argomenti passati da riga di comando
void interpretCommandLine(int argc, char* argv[], int* nElements, int* nThreads) {
    int opt, tmp;
    //Per controllare se queste opzioni *necessarie* sono state passate a riga di comando
    int checkT = -1, checkN = -1;
    
    //Lettura delle opzioni e degli argomenti passati a riga di comando
    while ((opt = getopt(argc, argv, ":t:n:")) != -1) {
        switch (opt) {
            //Opzione per la strategia
            case 't':
                checkT = 0;
                tmp = atoi(optarg);
                //Check se l'argomento è un numero
                if (isdigit(*optarg) != 0) {
                    *nThreads = tmp; 
                } else {
                    printError(0, "Threads number incorrect.\n");
                    exit(-1);
                }
                break;
            //Opzione per il numero di elementi
            case 'n':
                checkN = 0;
                //Check se il numero di elementi da sommare è maggiore o uguale di 2
                if ((tmp = atoi(optarg)) >= 2) {
                    *nElements = tmp;
                } else {
                    printError(0, "Elements number incorrect. Elements should be almost 2.\n");
                    exit(-1);
                }
                break;
            case '?':
                printError(0, "One option is unknown. The options are:\n\t-n <number of elements>\n\t-t <number of threads>\n");
                exit(-1);
        }
    }

    //Check se le opzioni obbligatorie sono state passate
    if (checkN != 0 || checkT != 0) {
        printError(0, "Options missed. Insert:\n\t-n <number of elements>\n\t-t <number of threads>\n");
        exit(-1);
    }
}


int main(int argc, char** argv) {
    int* numbers;
    int n, nThreads, i, sumtot;
    struct timeval time;
    double t0, t1;

    //Parser linea di comando
    interpretCommandLine(argc, argv, &n, &nThreads);
    
    //Setting del numero di threads
    omp_set_num_threads(nThreads);

    numbers = malloc(sizeof(int) * n);
    generateRandomNumbers(numbers, n);

    //Calcolo tempo iniziale
    gettimeofday(&time, NULL);
    t0 = time.tv_sec + (time.tv_usec/1000000.0);
    
    //Calcolo della somma
    sumtot = computeSum(numbers, n);

    //Calcolo tempo finale
    gettimeofday(&time, NULL);
    t1 = time.tv_sec + (time.tv_usec/1000000.0);

    //Stampa della somma dal master thread
    printf("[TID %d] La somma è: %d\n", omp_get_thread_num(), sumtot);
    printf("[TID %d] Il tempo impiegato è %f\n", omp_get_thread_num(), t1 - t0);

}