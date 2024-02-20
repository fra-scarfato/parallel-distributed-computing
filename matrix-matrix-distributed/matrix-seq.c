#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

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

int main(int argc, char** argv) {
    int **a, **b;
    int dim=1000;
    int **r, i, j, k;
    struct timeval time1;
    double t0, t1;

    mallocMatrix(&a, dim, dim);
    mallocMatrix(&b, dim, dim);
    mallocMatrix(&r, dim, dim);

    srand(time(0));
    generateRandomNumbers(a, dim, dim);
    generateRandomNumbers(b, dim, dim);

    //Calcolo tempo iniziale
    gettimeofday(&time1, NULL);
    t0 = time1.tv_sec + (time1.tv_usec/1000000.0);

    for (i = 0; i < dim; i++) {
        for (j = 0; j < dim; j++) {
            int sum = 0;
            for (k = 0; k < dim; k++) {
                sum += a[i][k] * b[k][j];
            }
            r[i][j] = sum;
        }
    }

    //Calcolo tempo finale
    gettimeofday(&time1, NULL);
    t1 = time1.tv_sec + (time1.tv_usec/1000000.0);
    printf("%f\n", t1-t0);


}