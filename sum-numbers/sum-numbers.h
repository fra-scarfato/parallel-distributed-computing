#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <getopt.h>
#include <ctype.h>
#include <time.h>

//Tipo di strategia
typedef enum{
    FIRST_STRATEGY = 1,
    SECOND_STRATEGY,
    THIRD_STRATEGY
} Strategy;

//Da dove recuperare l'input
typedef enum{
    NONE,
    FROM_FILE,
    RANDOM,
    DEBUG
} Input;

//Utility per un processo
typedef struct{
    int processorID;
    int nProcessors;
    Strategy strategyType;
    int sum;
    //Numero di elementi per ogni processore
    int nElementsInOneprocessor;
    //Array per memorizzare i numeri in ogni processo
    int* numbersInOneprocessor;
    //Memorizza il logaritmo in base 2 del numero di processori
    int logNProcessors;
    //Memorizza le potenze di 2
    int* powersOf2;
} Utils;

/******** CORE* *******/
/*Funzione di gestione*/
int interpretCommandLine(int, char*[], Strategy*, Input*, int*);
void checkStrategy(Utils*);
void distributeData(Utils*, int, int[]);
void firstStrategy(Utils*);
void secondStrategy(Utils*);
void thirdStrategy(Utils*);

/******* INPUT ******/
/*Funzioni per generare/recuperare input*/
void generateRandomNumbers(int*, int);
void generate1(int*, int);
void retrieveInput(int*, int, char*);

/****** UTILS *****/
void printError(int, char*);
char* inputTypeToString(Input); 
char* strategyToString(Strategy);