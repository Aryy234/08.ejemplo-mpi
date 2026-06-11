para ejecutar el proyecto primero

crear los ejecutable:


ir a la ruta: c:\oneAPI
y ahi colocar: .\setvars.bat

luego cambiar a: C:\tools\CLASES 2026-2026\prog-paralela\08.ejemplo-mpi\build\Release
y ahi colocar: mpiexec -n 8  matrices-mult.exe


Tarea realizar estas implementaciones:

1. Realizar ceil
2. Rellenar el ultimo y cambiar la matriz original
3. Realizar truncamiento

otra forma desde powershell:

1. cd "build\Release"
2. $env:PATH = "C:\oneAPI\mpi\2021.18\bin;" + $env:PATH
3. mpiexec -n 8 matrices-mult.exe


para hacer build del proyecto:
cmake --build "c:\tools\CLASES 2026-2026\prog-paralela\08.ejemplo-mpi\build" --config Release

Tarea 2:
Analizar que Comunicación Colectiva utlizar para el problema de multipliación de matrices
