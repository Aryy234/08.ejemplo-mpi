#include <iostream>
#include <fmt/core.h>
#include <mpi.h>
#include <vector>
#include <cmath>
#include <algorithm>

#define MATRIX_DIM 25
#define ESTRATEGIA 3 // 1: Ceil, 2: Padding (Rellenar), 3: Truncamiento

// ---------------------------------------------------------------------------
// Funciones de impresión (sin cambios)
// ---------------------------------------------------------------------------

void imprimir_matriz(const std::vector<double> &matriz, int filas, int columnas, int rank)
{
    fmt::print("\n--- Matriz local en RANK_{} ({}x{}) ---\n", rank, filas, columnas);
    for (int i = 0; i < filas; i++)
    {
        for (int j = 0; j < columnas; j++)
            fmt::print("{:>5.1f} ", matriz[i * columnas + j]);
        fmt::print("\n");
    }
    fmt::print("-------------------------------------------\n");
}

void imprimir_vector(const std::vector<double> &vec, int elementos, int rank)
{
    fmt::print("\n--- Vector local en RANK_{} ({} elementos) ---\n", rank, elementos);
    for (int i = 0; i < elementos; i++)
        fmt::print("{:>5.1f} ", vec[i]);
    fmt::print("\n-------------------------------------------\n");
}

void imprimir_resultado_final(const std::vector<double> &vec, int elementos, int rank)
{
    fmt::print("\n========== RESULTADO FINAL (RANK {}) ==========\n", rank);
    for (int i = 0; i < elementos; i++)
    {
        if (i % 5 == 0)
            fmt::print("\n[{:2d}]: ", i);
        fmt::print("{:8.1f} ", vec[i]);
    }
    fmt::print("\n================================================\n");
}

// ---------------------------------------------------------------------------
// Multiplicación local: cada proceso trabaja solo con sus filas
// ---------------------------------------------------------------------------

void multiplicar_matriz_vector(const std::vector<double> &matriz,
                               const std::vector<double> &vec,
                               std::vector<double> &resultado,
                               int filas, int columnas)
{
    for (int i = 0; i < filas; i++)
    {
        double sum = 0.0;
        for (int j = 0; j < columnas; j++)
            sum += matriz[i * columnas + j] * vec[j];
        resultado[i] = sum;
    }
}

// ---------------------------------------------------------------------------
// MAIN
// ---------------------------------------------------------------------------

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);

    int nprocs, rank;
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // =========================================================================
    // 1. Todos los procesos calculan cuántas filas les corresponden
    //    (evita tener que enviar esta información por mensajes)
    // =========================================================================
    std::vector<int> rows_per_proc(nprocs);
    int rows_per_rank_base = 0;

    if (ESTRATEGIA == 1) // Ceil
    {
        rows_per_rank_base = (int)std::ceil(MATRIX_DIM * 1.0 / nprocs);
        for (int i = 0; i < nprocs; i++)
        {
            int from = std::min(i * rows_per_rank_base, MATRIX_DIM);
            int to = std::min((i + 1) * rows_per_rank_base, MATRIX_DIM);
            rows_per_proc[i] = to - from;
        }
    }
    else if (ESTRATEGIA == 2) // Padding
    {
        rows_per_rank_base = (int)std::ceil(MATRIX_DIM * 1.0 / nprocs);
        for (int i = 0; i < nprocs; i++)
            rows_per_proc[i] = rows_per_rank_base;
    }
    else // ESTRATEGIA == 3: Truncamiento
    {
        rows_per_rank_base = MATRIX_DIM / nprocs;
        int remainder = MATRIX_DIM % nprocs;
        for (int i = 0; i < nprocs; i++)
            rows_per_proc[i] = (i == nprocs - 1) ? rows_per_rank_base + remainder
                                                  : rows_per_rank_base;
    }

    int my_rows = rows_per_proc[rank]; // filas locales de este proceso

    // Calcular sendcounts y displacements para Scatterv y Gatherv
    std::vector<int> scatter_counts(nprocs), scatter_displs(nprocs);
    std::vector<int> gather_counts(nprocs),  gather_displs(nprocs);
    int total_filas_virtuales = 0;

    for (int i = 0; i < nprocs; i++)
    {
        // Scatterv trabaja en términos de doubles (filas * columnas)
        scatter_counts[i] = rows_per_proc[i] * MATRIX_DIM;
        scatter_displs[i] = total_filas_virtuales * MATRIX_DIM;

        // Gatherv trabaja en términos de doubles (solo el resultado: 1 double por fila)
        gather_counts[i] = rows_per_proc[i];
        gather_displs[i] = total_filas_virtuales;

        total_filas_virtuales += rows_per_proc[i];
    }

    // =========================================================================
    // 2. Solo Rank 0 inicializa la matriz A global y el vector resultado final
    //    Todos los procesos declaran b (Rank 0 lo rellena, los demás lo recibirán)
    // =========================================================================
    std::vector<double> A_global; // solo Rank 0 lo usa completo
    std::vector<double> x_global; // solo Rank 0 recibe el resultado final
    std::vector<double> b(MATRIX_DIM, 1.0); // todos lo declaran; Rank 0 ya tiene el valor

    if (rank == 0)
    {
        A_global.resize(total_filas_virtuales * MATRIX_DIM, 0.0);
        x_global.resize(total_filas_virtuales, 0.0);

        // Inicializar A: cada fila i tiene el valor i en todas sus columnas
        for (int i = 0; i < MATRIX_DIM; i++)
            for (int j = 0; j < MATRIX_DIM; j++)
                A_global[i * MATRIX_DIM + j] = i;
        // En Estrategia 2 las filas extra (padding) quedan en 0.0 por el resize

        fmt::print("MATRIX_DIM: {}, nprocs: {}, rows_per_rank_base: {}, ESTRATEGIA: {}\n",
                   MATRIX_DIM, nprocs, rows_per_rank_base, ESTRATEGIA);
    }

    // =========================================================================
    // COMUNICACIÓN COLECTIVA 1 — MPI_Bcast
    // Rank 0 envía el vector b completo a TODOS los procesos.
    // Cada proceso necesita b entero para poder calcular su parte de A*b.
    // =========================================================================
    MPI_Bcast(b.data(), MATRIX_DIM, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    // =========================================================================
    // COMUNICACIÓN COLECTIVA 2 — MPI_Scatterv
    // Rank 0 reparte porciones distintas de filas de A a cada proceso.
    // Se usa Scatterv (variante variable) porque Ceil y Truncamiento
    // pueden dar tamaños distintos en el último proceso.
    // =========================================================================
    std::vector<double> A_local(my_rows * MATRIX_DIM);

    MPI_Scatterv(
        A_global.data(),    scatter_counts.data(), scatter_displs.data(), MPI_DOUBLE, // Rank 0 envía
        A_local.data(),     my_rows * MATRIX_DIM,                         MPI_DOUBLE, // cada proceso recibe
        0, MPI_COMM_WORLD
    );

    fmt::print("RANK_{}: calculando {} x {}\n", rank, my_rows, MATRIX_DIM);
    imprimir_matriz(A_local, my_rows, MATRIX_DIM, rank);
    imprimir_vector(b, MATRIX_DIM, rank);

    // =========================================================================
    // CÓMPUTO LOCAL — sin comunicación
    // Cada proceso multiplica su porción de A por el vector b completo.
    // =========================================================================
    std::vector<double> x_local(my_rows);
    multiplicar_matriz_vector(A_local, b, x_local, my_rows, MATRIX_DIM);

    // =========================================================================
    // COMUNICACIÓN COLECTIVA 3 — MPI_Gatherv
    // Cada proceso envía su porción de x_local al Rank 0,
    // que ensambla el vector resultado completo x_global.
    // =========================================================================
    MPI_Gatherv(
        x_local.data(),  my_rows,                                          MPI_DOUBLE, // cada proceso envía
        x_global.data(), gather_counts.data(), gather_displs.data(),       MPI_DOUBLE, // Rank 0 recibe
        0, MPI_COMM_WORLD
    );

    // =========================================================================
    // Solo Rank 0 imprime el resultado final (hasta MATRIX_DIM, ignora padding)
    // =========================================================================
    if (rank == 0)
    {
        imprimir_resultado_final(x_global, MATRIX_DIM, rank);
        imprimir_vector(x_global, MATRIX_DIM, rank);
    }

    MPI_Finalize();
    return 0;
}