#include <iostream>
#include <mpi.h>
#include <fmt/core.h>

int main(int argc, char* argv[]) {
    MPI_Init(&argc, &argv);

    int nprocs;
    int rank;

    // -- rank y nprocs
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // --version
    int version, subversion;
    MPI_Get_version(&version, &subversion);

    if(rank == 0){
        fmt::print("MPI version: {}.{}\n", version, subversion);
        fmt::print("Number of processes: {}\n", nprocs);
    }

    fmt::print("RANK {} de {} procesos\n", rank, nprocs);


    MPI_Finalize();

    return 0;
}