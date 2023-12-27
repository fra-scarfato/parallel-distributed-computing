#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <getopt.h>
#include <ctype.h>

void interpretCommandLine(int argc, char* argv[], int dimsMatrix[], int* dimVector, int* nThreads) {
    int opt, tmp;
    //Per controllare se queste opzioni necessarie sono state passate a riga di comando
    int checkT = -1, checkV = -1, checkR = -1, checkC = -1;
    
    //Lettura delle opzioni e degli argomenti passati a riga di comando
    while ((opt = getopt(argc, argv, ":t:r:c:v:")) != -1) {
        switch (opt) {
            //Opzione per il numero di threads
            case 't':
                checkT = 0;
                if((tmp = atoi(optarg)) == 0){
                    fprintf(stderr, "Threads number incorrect.\n");
                    exit(-1);
                };
                *nThreads = tmp;
                break;
            //Opzione per il numero di righe della matrice
            case 'r':
                checkR = 0;
                if((tmp = atoi(optarg)) == 0){
                    fprintf(stderr, "Rows number incorrect.\n");
                    exit(-1);
                };
                dimsMatrix[0] = tmp;
                break;
            //Opzione per il numero di colonne della matrice
            case 'c':
                checkC = 0;
                if((tmp = atoi(optarg)) == 0){
                    fprintf(stderr, "Columns number incorrect.\n");
                    exit(-1);
                };
                dimsMatrix[1] = tmp;
                break;
            //Opzione per il numero di elementi del vettore
            case 'v':
                checkV = 0;
                if ((tmp = atoi(optarg)) == 0) {
                    fprintf(stderr, "Elements number incorrect.\n");
                    exit(-1);
                }
                *dimVector = tmp;    
                break;
            case '?':
                fprintf(stderr, "One option is unknown. The options are:\n\t-r <rows number>\n\t-c <columns number>\n\t-v <vector size>\n\t-t <number of threads>\n");
                exit(-1);
        }
    }
    //Controlla se tutte le opzioni sono state date a riga di comando
    if (checkR != 0 || checkC != 0 || checkV != 0 || checkT != 0) {
        fprintf(stderr, "Options missed. Insert:\n\t-r <rows number>\n\t-c <columns number>\n\t-v <vector size>\n\t-t <number of threads>\n");
        exit(-1);
    }
    /*Se il numero di colonne della matrice e la grandezza del vettore
      non coincidono allora non è possibile eseguire il prodotto */
    if (dimsMatrix[1] != (*dimVector)){
        fprintf(stderr, "Columns number is different than vector size. Both numbers should be equal.\n");
        exit(-1);
    }
    
}

void mallocMatrix(int ***matrix, int r, int c) {
    int i;

    int *cell = (int *)malloc(r*c*sizeof(int));
    if (!cell){
        exit(-1);
    };

    (*matrix) = (int **)malloc(r*sizeof(int*));
    if (!(*matrix)) {
       free(cell);
       exit(-1);
    }

    for (i=0; i<r; i++){
       (*matrix)[i] = &(cell[i*c]);
    }
}

void generateRandomNumbers(int **matrix, int *vector, int dimsMatrix[], int dimVector){
    int i, j;

    srand(time(0));
    //printf("\n--- matrix ---\n");
    for (i=0; i<dimsMatrix[0]; i++){
        for (j=0; j<dimsMatrix[1]; j++){
            matrix[i][j] = rand()%10;
            //printf("|%d|", matrix[i][j]);
        }
        //printf("\n");
    }
    printf("\n--- vector ---\n");
    for (i = 0; i<dimVector; i++){
        vector[i] = rand()%10;
        //printf("|%d|\n", vector[i]); 
    }
    
}

int* computeMatrixPerVector(int **matrix, int *vector, int dimsMatrix[]){
    int i,j;
    int *result;

    //il numero di righe è uguale alla dimensione del vettore risultante
    result = (int *)malloc(dimsMatrix[0]*sizeof(int));

    #pragma omp parallel for default(none) shared(dimsMatrix, matrix, vector, result) private(i,j)
    for (i = 0; i<dimsMatrix[0]; i++){
        for (j = 0; j<dimsMatrix[1]; j++){
            result[i] += matrix[i][j]*vector[j];
        }
        
    }

    //printf("\n--- result ---\n");
    /*for (i = 0; i<dimsMatrix[0]; i++){
        printf("|%d|\n", result[i]);
    }*/
    
    return result;
}

int freeMatrix(int ***array) {
    free(&((*array)[0][0]));
    free(*array);

    return 0;
}

int main(int argc, char **argv){
    int **mainMatrix, *vector, *result; 
    /*L'indice 0 rappresenta il numero di righe
      mentre l'indice 1 rappresenta il numero di colonne*/
    int dimsMatrix[] = {0,0}, dimVector, nThreads;
    struct timeval time;
    double t0, t1;

    interpretCommandLine(argc, argv, dimsMatrix, &dimVector, &nThreads);
    
    //Setting del numero di threads
    omp_set_num_threads(nThreads);
    
    //Allocazione della matrice e del vettore
    mallocMatrix(&mainMatrix, dimsMatrix[0], dimsMatrix[1]);
    vector = (int *)malloc(dimVector*sizeof(int));
    //Inizializzazione matrice e vettore con numeri casuali
    generateRandomNumbers(mainMatrix, vector, dimsMatrix, dimVector);
    
    //Calcolo tempo iniziale
    gettimeofday(&time, NULL);
    t0 = time.tv_sec + (time.tv_usec/1000000.0);

    result = computeMatrixPerVector(mainMatrix, vector, dimsMatrix);

    //Calcolo tempo finale
    gettimeofday(&time, NULL);
    t1 = time.tv_sec + (time.tv_usec/1000000.0);
    printf("%f\n", t1-t0);
    //printf("Thread: %d, Time: %f", nThreads, t1-t0);

    freeMatrix(&mainMatrix);
    free(vector);
    free(result);
}
