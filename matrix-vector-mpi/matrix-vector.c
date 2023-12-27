#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

void createGrid(MPI_Comm *, MPI_Comm *, MPI_Comm *, int []);
void mallocMatrix(int ***, int, int);

int main(int argc, char **argv) {
    int nProc, coords[] = {0,0}, dimsGrid[] = {0,0}, dimsMatrix[] = {0,0}, dimsSubMatrix[] = {0,0};
    int rest[2];
    int startOffset[] = {0,0};
    /**
     *- 0 is id in MPI_COMM_WORLD
     *- 1 is id in columns communicator
     *- 2 is id in rows communicator
    */
    int idProc[3];  
    int **mainMatrix, **localMatrix;
    MPI_Comm grid, rowC, columnC;
    MPI_Datatype newType, newTypeResized;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &idProc[0]);
    MPI_Comm_size(MPI_COMM_WORLD, &nProc);

    //rows number of matrix
    dimsMatrix[0] = atoi(argv[1]);
    //columns numbers of matrix
    dimsMatrix[1] = atoi(argv[2]);
    //rows number of grid
    dimsGrid[0] = atoi(argv[3]);
    
    //check if processors number is divisible with rows number of grid
    if (nProc%dimsGrid[0]!=0) {
         MPI_Finalize();
         perror("processors number must be divisible with rows number of grid\n");
         exit(-1);
    }
    //columns number of grid
    dimsGrid[1] = nProc/dimsGrid[0];
    
    if (idProc[0] == 0){
        printf("n. proc: %d\n", nProc);
        printf("matrix rows: %d, columns: %d\n", dimsMatrix[0], dimsMatrix[1]);
        printf("grid rows: %d, columns: %d\n", dimsGrid[0], dimsGrid[1]);

        //allocate matrix
        mallocMatrix(&mainMatrix, dimsMatrix[0], dimsMatrix[1]);
        for (int i=0; i<dimsMatrix[0]; i++) {
            for (int j=0; j<dimsMatrix[1]; j++) {
                mainMatrix[i][j] = (3*i+j)%10;
                printf("%d|", mainMatrix[i][j]);
            }
            printf("\n");
        }
    }
    
    //create grid processors
    createGrid(&grid, &rowC, &columnC, dimsGrid);
    //assigns coords to processor
    MPI_Cart_coords(grid , idProc[0], 2, coords);
    
    printf("id: %d, coords: (%d,%d)\n", idProc[0], coords[0], coords[1]);

    //computing submatrix dims
    dimsSubMatrix[0] = dimsMatrix[0]/dimsGrid[0];
    dimsSubMatrix[1] = dimsMatrix[1]/dimsGrid[1];
    /*computing rest of the elements left in row (index 0)
    and rest of the elements left in column*/
    rest[0] = dimsMatrix[0]%dimsGrid[0];
    rest[1] = dimsMatrix[1]%dimsGrid[1];
    //assign id in columns communicator (communication in one row)
    MPI_Comm_rank(columnC, &idProc[1]);
    if(idProc[1] < rest[0]){
        //increse dim of first processors if there is rest on one row
        dimsSubMatrix[0]++;
    }
    //assign id in rows communicator (communication in one column)
    MPI_Comm_rank(rowC, &idProc[2]);
    if (idProc[2] < rest[1]){
        //increse dim of first processors if there is rest on one column
        dimsSubMatrix[1]++;
    }

    printf("id: %d, local rows of submatrix: %d\n", idProc[0], dimsSubMatrix[0]);
    printf("id: %d, local columns of submatrix: %d\n", idProc[0], dimsSubMatrix[1]);
    MPI_Barrier(grid);

    if (idProc[0] == 0){
        int nElemRow = dimsSubMatrix[0];
        int nElemColumn = dimsSubMatrix[1];
        for (int i = 0; i < dimsMatrix[0]; i++){
            for (int j = 0; j < dimsMatrix[1]; j++){
                
            }  
        }
    }
    
    
    
    
    mallocMatrix(&localMatrix, dimsSubMatrix[0], dimsSubMatrix[1]);
    
    MPI_Finalize();

}

void createGrid(MPI_Comm *grid, MPI_Comm *rowC, MPI_Comm *columnC, int dims[]) {
    int periods[] = {0,0}, reorder = 0;
    
    MPI_Cart_create(MPI_COMM_WORLD, 2, dims, periods, reorder, grid);

    int remain[] = {0,1};
    MPI_Cart_sub(*grid, remain, rowC);

    remain[0] = 1;
    remain[1] = 0;
    MPI_Cart_sub(*grid, remain, columnC);
}

void mallocMatrix(int ***matrix, int n, int m) {

    /* allocate the n*m contiguous items */
    int *cell = (int *)malloc(n*m*sizeof(int));
    if (!cell){
        exit(-1);
    };

    /* allocate the row pointers into the memory */
    (*matrix) = (int **)malloc(n*sizeof(int*));
    if (!(*matrix)) {
       free(cell);
       exit(-1);
    }

    /* set up the pointers into the contiguous memory */
    for (int i=0; i<n; i++){
       (*matrix)[i] = &(cell[i*m]);
    }
}