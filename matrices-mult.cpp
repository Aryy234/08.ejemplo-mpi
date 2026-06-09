#include <iostream>
#include <mpi.h>
#include <fmt/core.h>
#include <vector>
#include <cmath>

#define MATRIX_DIM 25

void imprimir_matriz(const std::vector<double>& matrix, int rows, int cols) {
    for(int i = 0; i < rows; i++){
        for(int j = 0; j < cols; j++){
            fmt::print("{:.2f} ", matrix[i * cols + j]);
        }
        fmt::print("\n");
    }
}

int main(int argc, char* argv[]) {
    MPI_Init(&argc, &argv);

    int nprocs;
    int rank;

    // -- rank y nprocs
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);



    if(rank == 0){

        std::vector<double> A(MATRIX_DIM * MATRIX_DIM);
        std::vector<double> b(MATRIX_DIM * MATRIX_DIM);
        std::vector<double> x(MATRIX_DIM * MATRIX_DIM);

        for(int i = 0; i < MATRIX_DIM; i++){
            for(int j = 0; j < MATRIX_DIM; j++){
                int index = i * MATRIX_DIM + j;
                A[index] = i;
            }
                
        }

        for(int i = 0; i < MATRIX_DIM; i++){
            b[i] = i;
        }


        //numero de filas para cada RANK (proceso)
        int rows_per_rank = std::ceil (MATRIX_DIM * 1.0 / nprocs);
        int padding = rows_per_rank * nprocs - MATRIX_DIM;

        fmt::print("MATRIX_DIM: {}\n", MATRIX_DIM);
        fmt::print("Rows per rank: {}, padding: {}\n", rows_per_rank, padding);

        for(int i = 1; i < nprocs; i++){
            int filas = rows_per_rank;

            if(i == nprocs - 1){
                filas = rows_per_rank - padding;
            }

            // enviar dimension
            std::vector<int> data = {MATRIX_DIM, filas};
            MPI_Send(
                data.data(), // buffer
                (int)data.size(), // count
                MPI_INT, // datatype
                i, // RANK destino
                0, // tag
                MPI_COMM_WORLD // grupo de comunicacion
            );

            double* A_data = A.data();
            MPI_Send(
                &A_data[i*rows_per_rank*MATRIX_DIM], // buffer
                filas * MATRIX_DIM, // count
                MPI_DOUBLE, // datatype
                i, // RANK destino
                0, // tag
                MPI_COMM_WORLD // grupo de comunicacion
            );
        }

        fmt::print("RANK {} de {} X procesos\n", rank, nprocs);
        
        int matrix_dim = MATRIX_DIM;
        int rows = (nprocs == 1) ? (rows_per_rank - padding) : rows_per_rank;
        fmt::print("RANK {}: matrix_dim {}, rows {}\n", rank, matrix_dim, rows);
    }
    else {
        std::vector<int> data(2);
        MPI_Recv(
            data.data(), // buffer
            (int)data.size(), // count
            MPI_INT, // datatype
            0, // RANK origen
            0, // tag
            MPI_COMM_WORLD, // grupo de comunicacion
            MPI_STATUS_IGNORE // status
        );

        int matrix_dim = data[0];
        int rows = data[1];

        std::vector<double> A_local(rows * matrix_dim);
        MPI_Recv(
            A_local.data(), // buffer
            rows * matrix_dim, // count
            MPI_DOUBLE, // datatype
            0, // RANK origen
            0, // tag
            MPI_COMM_WORLD, // grupo de comunicacion
            MPI_STATUS_IGNORE // status
        );

         if(rank ==3){
            imprimir_matriz(A_local, rows, matrix_dim);
         }
         
    }

    MPI_Finalize();

    return 0;
}