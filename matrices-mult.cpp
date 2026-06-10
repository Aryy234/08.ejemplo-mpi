#include <iostream>
#include <fmt/core.h>
#include <mpi.h>
#include <vector>
#include <cmath> // Necesario para std::ceil
#include <algorithm>

#define MATRIX_DIM 25
#define ESTRATEGIA 3 // 1: Ceil, 2: Padding (Rellenar), 3: Truncamiento

// Función para imprimir la matriz aplanada (formato filas x columnas)
void imprimir_matriz(const std::vector<double> &matriz, int filas, int columnas, int rank)
{
    fmt::print("\n--- Matriz local en RANK_{} (imprimiendo {}x{}) ---\n", rank, filas, columnas);
    for (int i = 0; i < filas; i++)
    {
        for (int j = 0; j < columnas; j++)
        {
            int index = i * columnas + j;
            fmt::print("{:>5.1f} ", matriz[index]);
        }
        fmt::print("\n");
    }
    fmt::print("-------------------------------------------------\n");
}

void imprimir_vector(const std::vector<double> &vector, int elementos, int rank)
{
    fmt::print("\n--- Vector local en RANK_{} (imprimiendo {} elementos) ---\n", rank, elementos);
    for (int i = 0; i < elementos; i++)
    {
        fmt::print("{:>5.1f} ", vector[i]);
    }
    fmt::print("\n");
    fmt::print("-------------------------------------------------\n");
}

void imprimir_resultado_final(const std::vector<double> &vector, int elementos, int rank)
{
    fmt::print("\n========== RESULTADO FINAL UNIDO (RANK {}) ==========\n", rank);
    for (int i = 0; i < elementos; i++)
    {
        if (i % 5 == 0)
            fmt::print("\n[{:2d}]: ", i);
        fmt::print("{:8.1f} ", vector[i]);
    }
    fmt::print("\n====================================================\n");
}

void multiplicar_matriz_vector(const std::vector<double> &matriz,
                               const std::vector<double> &vector,
                               std::vector<double> &resultado,
                               int filas,
                               int columnas)
{
    for (int i = 0; i < filas; i++)
    {
        double sum = 0.0;
        for (int j = 0; j < columnas; j++)
        {
            int index = i * columnas + j;
            sum += matriz[index] * vector[j];
        }
        resultado[i] = sum;
    }
}

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);

    int nprocs, rank;
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // Vector local para el vector 'b', todos los nodos lo necesitan
    std::vector<double> b_local(MATRIX_DIM);

    if (rank == 0)
    {
        int total_filas_virtuales = MATRIX_DIM;
        int rows_per_rank = 0;
        int padding = 0;

        if (ESTRATEGIA == 1) {
            // 1. Ceil
            rows_per_rank = std::ceil(MATRIX_DIM * 1.0 / nprocs);
            padding = rows_per_rank * nprocs - MATRIX_DIM;
            total_filas_virtuales = MATRIX_DIM;
        } else if (ESTRATEGIA == 2) {
            // 2. Padding (Rellenar matriz)
            rows_per_rank = std::ceil(MATRIX_DIM * 1.0 / nprocs);
            padding = 0;
            total_filas_virtuales = rows_per_rank * nprocs;
        } else if (ESTRATEGIA == 3) {
            // 3. Truncamiento
            rows_per_rank = MATRIX_DIM / nprocs;
            padding = 0;
            total_filas_virtuales = MATRIX_DIM;
        }

        std::vector<double> A(total_filas_virtuales * MATRIX_DIM, 0.0);
        std::vector<double> b(MATRIX_DIM, 1.0);
        std::vector<double> x(total_filas_virtuales, 0.0);

        // Inicializar Matriz A
        for (int i = 0; i < MATRIX_DIM; i++)
        {
            for (int j = 0; j < MATRIX_DIM; j++)
            {
                int index = i * MATRIX_DIM + j;
                A[index] = i; // Rellena con el índice de la fila
            }
        }
        // En ESTRATEGIA 2, las filas extra virtuales quedan inicializadas en 0.0 gracias al constructor del vector

        // Copiar el b local del rank 0
        b_local = b;

        fmt::print("MATRIX_DIM: {}, nprocs: {}, rows_per_rank: {}, ESTRATEGIA: {}\n",
                   MATRIX_DIM, nprocs, rows_per_rank, ESTRATEGIA);

        // Enviar datos a los esclavos (ranks 1 hasta nprocs-1)
        for (int i = 1; i < nprocs; i++)
        {
            int filas = rows_per_rank;
            if (ESTRATEGIA == 1 && i == nprocs - 1)
            {
                filas = rows_per_rank - padding;
            }
            else if (ESTRATEGIA == 3 && i == nprocs - 1)
            {
                filas = rows_per_rank + (MATRIX_DIM % nprocs);
            }

            // Enviar las dimensiones
            std::vector<int> data = {MATRIX_DIM, filas};
            MPI_Send(data.data(), data.size(), MPI_INT, i, 0, MPI_COMM_WORLD);

            // Enviar los datos de la matriz A
            int offset_filas = i * rows_per_rank;
            MPI_Send(&A[offset_filas * MATRIX_DIM], filas * MATRIX_DIM, MPI_DOUBLE, i, 0, MPI_COMM_WORLD);

            // Enviar el vector b a cada proceso esclavo
            MPI_Send(b.data(), MATRIX_DIM, MPI_DOUBLE, i, 0, MPI_COMM_WORLD);
        }

        // El Rank 0 procesa sus propias filas locales de A
        int filas_locales_0 = rows_per_rank;
        std::vector<double> A_local_0(filas_locales_0 * MATRIX_DIM);
        std::copy(A.begin(), A.begin() + (filas_locales_0 * MATRIX_DIM), A_local_0.begin());

        fmt::print("RANK_{}: calculando {} x {}\n", rank, filas_locales_0, MATRIX_DIM);

        // muliplicar matriz por vector en el Rank 0
        std::vector<double> x_local_0(filas_locales_0);
        multiplicar_matriz_vector(A_local_0, b_local, x_local_0, filas_locales_0, MATRIX_DIM);
        std::copy(x_local_0.begin(), x_local_0.end(), x.begin());

        imprimir_matriz(A_local_0, filas_locales_0, MATRIX_DIM, rank);
        imprimir_vector(b_local, MATRIX_DIM, rank);

        // Recibir resultados locales de cada rank esclavo
        for (int i = 1; i < nprocs; i++)
        {
            int filas = rows_per_rank;
            if (ESTRATEGIA == 1 && i == nprocs - 1)
            {
                filas = rows_per_rank - padding;
            }
            else if (ESTRATEGIA == 3 && i == nprocs - 1)
            {
                filas = rows_per_rank + (MATRIX_DIM % nprocs);
            }

            int offset_filas = i * rows_per_rank;
            MPI_Recv(&x[offset_filas], filas, MPI_DOUBLE, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }

        // Imprimir resultado final unido (solo se imprime hasta MATRIX_DIM, ignorando el relleno de ESTRATEGIA 2)
        imprimir_resultado_final(x, MATRIX_DIM, rank);
        imprimir_vector(x, MATRIX_DIM, rank);
    }
    else
    {
        // Bloque para los Ranks esclavos (rank != 0)
        std::vector<int> data_rec(2);
        MPI_Recv(data_rec.data(), 2, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        
        int matrix_dim = data_rec[0];
        int rows = data_rec[1];

        fmt::print("RANK_{}: recibido {} x {}\n", rank, rows, matrix_dim);
        std::vector<double> A_local(rows * matrix_dim);

        // Recibir sub-matriz A
        MPI_Recv(A_local.data(), rows * matrix_dim, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        // Recibir el vector b enviado por el rank 0
        MPI_Recv(b_local.data(), matrix_dim, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        // Imprimir matriz y vector local en cada rank (Correccion del fmt::print)
        fmt::print("LLEGO RANK: {}, rows: {}, matrix_dim: {}\n", rank, rows, matrix_dim);
        imprimir_matriz(A_local, rows, matrix_dim, rank);
        imprimir_vector(b_local, matrix_dim, rank);

        std::vector<double> x_local(rows);
        multiplicar_matriz_vector(A_local, b_local, x_local, rows, matrix_dim);

        MPI_Send(x_local.data(), rows, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD);
    }

    MPI_Finalize();
    return 0;
}