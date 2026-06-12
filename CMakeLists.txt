cmake_minimum_required(VERSION 3.16)

project(EjemploMPI)

set(CMAKE_CXX_FLAGS)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2 -fopenmp")

find_package(fmt CONFIG REQUIRED)

#-- conf mpi
set(MPI_ROOT "C:/oneAPI/mpi/2021.18")
set(MPI_INCLUDE "${MPI_ROOT}/include")
set(MPI_LIB "${MPI_ROOT}/lib")

include_directories(
    ${MPI_INCLUDE}
)

add_executable(
    ejemplo-mpi
    main.cpp
    )

add_executable(
    matrices-mult
    matrices-mult.cpp
)

add_executable(
    mpi-matrices-mult
    mpi-matrices-mult.cpp
)

target_link_libraries(ejemplo-mpi PRIVATE 
    fmt::fmt
    ${MPI_LIB}/impi.lib
)

target_link_libraries(matrices-mult PRIVATE 
    fmt::fmt
    ${MPI_LIB}/impi.lib
)

target_link_libraries(mpi-matrices-mult PRIVATE 
    fmt::fmt
    ${MPI_LIB}/impi.lib
)
