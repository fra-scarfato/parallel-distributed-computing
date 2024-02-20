#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <getopt.h>
#include <ctype.h>
#include <math.h>
#include <string.h>

struct coords{
    int world;
    int gridRank;
    int gridCoords[2];
    int row;
    int column;
};

int SHOW_OUTPUT = -1;

void interpretCommandLine(int, char* [], int*, int*);
void createGrid(MPI_Comm *, MPI_Comm *, MPI_Comm *, int);
void mallocMatrix(int ***, int, int);
void generateRandomNumbers(int **, int, int);
void printMatrix(int **, int, int);
void computeMatrixProduct(int *, int *, int*, int);
MPI_Datatype distributeData(int **, int **, int **, int **, struct coords, int, int, int, int, int *, int *);
void BMR(int **, int **, int **, struct coords, MPI_Comm, MPI_Comm, MPI_Comm, int, int);
void freeMatrix(int ***);

int main(int argc, char** argv){
    int **a, **b, **resultMatrix, **subBlockA, **subBlockB, **resultSubBlock;
    int dim, nProc, i, dimSub;
    double t0, t1, localTime, finalTime;

    MPI_Comm grid, rowC, columnC;
    struct coords coord;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &coord.world);
    MPI_Comm_size(MPI_COMM_WORLD, &nProc);

    if (coord.world == 0) interpretCommandLine(argc, argv, &dim, &nProc);
    MPI_Bcast(&dim, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&SHOW_OUTPUT, 1, MPI_INT, 0, MPI_COMM_WORLD);
    

    /*------- CONTROLLO ERRORI SU PARAMETRI -------*/
    double sqrtP;
    sqrtP = sqrt(nProc);
    
    /* Se il numero di processori non è una radice perfetta allora non è 
    possibile creare una griglia quadrata*/
    if ((int)sqrtP != sqrtP){
        perror("Impossible to create pxp grid\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
        exit(-1);
    }

    int gridSize = (int)sqrtP;
    /*Se la dimensione della matrice non è multiplo del numero di processori */
    if (dim%gridSize != 0){
        perror("Matrix dimension isn't multiple of processors number\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
        exit(-1);
    }
    

    /*------ SETUP ------*/
    dimSub = dim/gridSize;
    /* Allocazione sottoblocchi locali */
    mallocMatrix(&subBlockA, dimSub, dimSub);
    mallocMatrix(&subBlockB, dimSub, dimSub);
    /* Allocazione matrice risultante globale */
    mallocMatrix(&resultMatrix, dim, dim);
    /* Allocazione matrice risultante locale */
    mallocMatrix(&resultSubBlock, dimSub, dimSub);

    memset(*resultSubBlock, 0, dimSub*dimSub*sizeof(int));

    if (coord.world == 0){
        srand(time(0));
        mallocMatrix(&a, dim, dim);
        mallocMatrix(&b, dim, dim);
        generateRandomNumbers(a, dim, dim);
        generateRandomNumbers(b, dim, dim);
        if (SHOW_OUTPUT == 0) {
            printf("---- Matrix A ----\n");
            printMatrix(a, dim, dim);
            printf("---- Matrix B ----\n");
            printMatrix(b, dim, dim);
        }
        
        
    }

    /*------ DISTRIBUZIONE SOTTO MATRICI -------*/
    int countDataToSend[nProc];
    int offset[nProc];
    MPI_Datatype block;

    block = distributeData(a, b, subBlockA, subBlockB, coord, dim, dimSub, gridSize, nProc, countDataToSend, offset);

    /*------- CREAZIONE GRIGLIA -------*/
    createGrid(&grid, &rowC, &columnC, gridSize);
    /* Assegnazione delle coordinate ai processori nella griglia */
    MPI_Cart_coords(grid, coord.world, 2, coord.gridCoords);

    /*------ INIZIALIZZAZIONE TIMER -------*/
    MPI_Barrier(MPI_COMM_WORLD);
    t0 = MPI_Wtime();

    /*-------- BMR ---------*/
    BMR(subBlockA, subBlockB, resultSubBlock, coord, rowC, columnC, grid, dimSub, gridSize);

    //Calcolo del tempo finale di ogni processore
    t1 = MPI_Wtime();
    localTime = t1 - t0;
    MPI_Barrier(MPI_COMM_WORLD);

    //Viene salvato il tempo del processore che ha impiegato più tempo,
    //infatti la funzione trova il massimo tra i tempi locali
    MPI_Reduce(&localTime, &finalTime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    
    /* Stampa risultato e ricostruzione*/
    if(SHOW_OUTPUT == 0){
        for (i=0; i<nProc; i++) {
            if (coord.world == i) {
                printf("Local block of result on rank %d is:\n", coord.world);
                printMatrix(resultSubBlock, dimSub, dimSub);
            }
            MPI_Barrier(MPI_COMM_WORLD);
        }
    }

    int *ptrToResult = &(resultMatrix[0][0]);
    /* Ricostruzione della matrice risultante globale dalle matrici risultanti locali */
    MPI_Gatherv(&(resultSubBlock[0][0]), dimSub*dimSub, MPI_INT, ptrToResult, countDataToSend, offset, block, 0, MPI_COMM_WORLD);

    if (coord.world == 0) {
        if(SHOW_OUTPUT == 0) {
            printf("---- RESULT ----\n");
            printMatrix(resultMatrix, dim, dim);
        }
        printf("%.5f\n", finalTime);
    }

    /*--------- DEALLOCAZIONE ---------*/
    if (coord.world == 0) {
        freeMatrix(&a);
        freeMatrix(&b);
    }
    freeMatrix(&resultMatrix);
    freeMatrix(&resultSubBlock);
    freeMatrix(&subBlockA);
    freeMatrix(&subBlockB);
    
    MPI_Finalize();
    return 0;
}

MPI_Datatype distributeData(int **a, int **b, int **subBlockA, int **subBlockB, struct coords coord, int dim, int dimSub, int gridSize, int nProc, int *countDataToSend, int *offset) {
    int dimsMatrix[2] = {dim, dim};
    int dimsSubmatrix[2] = {dimSub, dimSub};
    int start[2] = {0,0};
    int i, j;
    int* ptrToMatrixA = NULL;
    int* ptrToMatrixB = NULL;
    MPI_Datatype type, block;
    
    /* Creazione del tipo block per distribuire blocchi di matrice*/
    MPI_Type_create_subarray(2, dimsMatrix, dimsSubmatrix, start, MPI_ORDER_C, MPI_INT, &type);
    MPI_Type_create_resized(type, 0, dimSub*sizeof(int), &block);
    MPI_Type_commit(&block);
    

    /* Definizione degli offset */
    if (coord.world == 0) {
        /* ptr da cui partire per mandare le sotto matrici*/
        ptrToMatrixA = &(a[0][0]);
        ptrToMatrixB = &(b[0][0]);

        /* Setting del numero di blocchi da mandare per ogni processore */
        for (i=0; i<nProc; i++) countDataToSend[i] = 1;
        
        /* Calcolo degli offset in base alla dim del sotto blocco */
        int disp = 0;
        for (i=0; i<gridSize; i++) {
            for (j=0; j<gridSize; j++) {
                offset[i*gridSize+j] = disp;
                disp += 1;
            }
            disp += (dimSub-1)*gridSize;
        }
    }
    
    MPI_Scatterv(ptrToMatrixA, countDataToSend, offset, block, &(subBlockA[0][0]), dimSub*dimSub, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Scatterv(ptrToMatrixB, countDataToSend, offset, block, &(subBlockB[0][0]), dimSub*dimSub, MPI_INT, 0, MPI_COMM_WORLD);
    
    for(i=0; i<nProc && SHOW_OUTPUT == 0; i++) {
        if(coord.world == i) {
            printf("Local block of A on rank %d is:\n", coord.world);
            printMatrix(subBlockA, dimSub, dimSub);
            printf("Local block of B on rank %d is:\n", coord.world);
            printMatrix(subBlockB, dimSub, dimSub);
            MPI_Barrier(MPI_COMM_WORLD);
        }
    }

    return block;
}

void BMR(int **subBlockA, int **subBlockB, int **resultSubBlock, struct coords coord, MPI_Comm rowC, MPI_Comm columnC, MPI_Comm grid, int dimSub, int gridSize) {
    /* Processori sulla diagonale principale*/
    int **temp;
    int senderCoords[2] = {coord.gridCoords[0], coord.gridCoords[0]};
    /* Identifica l'id dei processi che devono fare broadcast */
    int senderGridRank, sender, i;
    /* Processori comunicanti sul communicator di colonna */
    int previousProcessorCoords[2] = {coord.gridCoords[0]-1, coord.gridCoords[1]};
    int nextProcessorCoords[2] = {coord.gridCoords[0]+1, coord.gridCoords[1]}; 
    int previousProcessorRank, nextProcessorRank;
    MPI_Request request;
    MPI_Status status;

    /* Viene salvato per ogni processore un rank associato alla sua coordinata */
    MPI_Cart_rank(grid, coord.gridCoords, &coord.gridRank);
    MPI_Comm_rank(columnC, &coord.column);
    /* Calcolo dell'id per i processori sulla diagonale principale */
    MPI_Cart_rank(grid, senderCoords, &senderGridRank);
    /* Id nel communicator di riga dei processi che mandano */
    MPI_Cart_rank(rowC, &senderGridRank, &sender);
    /* Calcolo dell'id dei processori che comunicano sul communicator di colonna */
    MPI_Cart_rank(columnC, previousProcessorCoords, &previousProcessorRank);
    MPI_Cart_rank(columnC, nextProcessorCoords, &nextProcessorRank);

    /* Allocazione e inizializzazione matrice di appoggio */
    mallocMatrix(&temp, dimSub, dimSub);
    memset(*temp, 0, dimSub*dimSub*sizeof(int));

    /* Processori sulla diagonale principale */
    if(coord.gridCoords[0] == coord.gridCoords[1]) {
        /* Matrice di appoggio per non sovrascrivere il sottoblocco di A locale */
        memcpy(*temp, *subBlockA, dimSub*dimSub*sizeof(int));
    }

    /* BROADCAST */
    /* Processori sulla diagonale principale fanno un broadcast sul communicator di riga */
    MPI_Bcast(&(temp[0][0]), dimSub*dimSub, MPI_INT, sender, rowC);

    /* MULTIPLY */
    computeMatrixProduct(*temp, *subBlockB, *resultSubBlock, dimSub);

    for (i=1; i<gridSize; i++) {
        /* Diagonale successiva */
        senderCoords[1] += 1;
        MPI_Cart_rank(grid, senderCoords, &senderGridRank);
        MPI_Cart_rank(rowC, &senderGridRank, &sender);

        /* Controllo se è il processore che deve fare broadcast */
        if(senderGridRank == coord.gridRank) {
            /* Matrice di appoggio per non sovrascrivere il sottoblocco di A locale */
            memcpy(*temp, *subBlockA, dimSub*dimSub*sizeof(int));
        }

        /* BROADCAST */
        /* Processori sulla diagonale successiva fanno un broadcast sul communicator di riga */
        MPI_Bcast(&(temp[0][0]), dimSub*dimSub, MPI_INT, sender, rowC);

        /* ROLLING */
        MPI_Isend(*subBlockB, dimSub*dimSub, MPI_INT, previousProcessorRank, 0, columnC, &request); //Send non bloccante
        MPI_Recv(&(subBlockB[0][0]), dimSub*dimSub, MPI_INT, nextProcessorRank, MPI_ANY_TAG, columnC, &status);

        /* MULTIPLY */
        computeMatrixProduct(*temp, *subBlockB, *resultSubBlock, dimSub);
    }
    freeMatrix(&temp);
}

void interpretCommandLine(int argc, char* argv[], int* dims, int* nProc) {
    int opt, tmp;
    //Per controllare se queste opzioni necessarie sono state passate a riga di comando
    int checkD = -1;
    
    //Lettura delle opzioni e degli argomenti passati a riga di comando
    while ((opt = getopt(argc, argv, ":d:s")) != -1) {
        switch (opt) {
            //Opzione per la dimensione delle matrici
            case 'd':
                checkD = 0;
                tmp = atoi(optarg);
                *dims = tmp;
                break;
            case 's':
                SHOW_OUTPUT = 0;
                break;
            case '?':
                fprintf(stderr, "One option is unknown. The options are:\n\t-d <dim matrix>\n\t-s to show output\n");
                exit(-1);
        }
        
    }
    //Controlla se tutte le opzioni sono state date a riga di comando
    if (checkD != 0) {
        fprintf(stderr, "Options missed. Insert:\n\t-d <matrix dimension>\n");
        exit(-1);
    }
    
}

void createGrid(MPI_Comm *grid, MPI_Comm *rowC, MPI_Comm *columnC, int gridSize) {
    int periods[] = {1,1}, dimsGrid[] = {gridSize, gridSize}, reorder = 0;
    
    /* Creazione griglia*/
    MPI_Cart_create(MPI_COMM_WORLD, 2, dimsGrid, periods, reorder, grid);

    int remain1[] = {0,1};
    MPI_Cart_sub(*grid, remain1, rowC);

    int remain2[] = {1,0};
    MPI_Cart_sub(*grid, remain2, columnC);
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

void generateRandomNumbers(int **matrix, int r, int c){
    int i, j;

    for (i=0; i<r; i++){
        for (j=0; j<c; j++){
            matrix[i][j] = rand()%10;
        }
    }
}

void printMatrix(int **matrix, int r, int c){
    int i, j;

    for (i=0; i<r; i++){
        for (j=0; j<c; j++){
            printf("|%d|", matrix[i][j]);
        }
        printf("\n");
    }
}

void computeMatrixProduct(int *A, int *B, int *C, int dim) {
    int i, j, z;

    for(i = 0; i < dim; i++){
       for(j = 0; j < dim; j++){
           for(z = 0; z < dim; z++){
               C[i*dim+j] += A[i*dim+z] * B[dim*z+j];
           }
       }
   }
    
}

void freeMatrix(int ***array) {
    free(&((*array)[0][0]));
    free(*array);
}






