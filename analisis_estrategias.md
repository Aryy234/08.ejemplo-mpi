# Análisis de Estrategias de Distribución MPI

Este documento analiza los resultados obtenidos al intentar distribuir una matriz de **25x25** entre **8 procesos** utilizando tres estrategias diferentes.

## El Problema Base
Queremos repartir 25 filas entre 8 procesos. Como 25 no es divisible entre 8 de forma exacta, tenemos que tomar decisiones sobre cómo manejar el sobrante o "resto".

---

## Opción 1: Estrategia `Ceil` (División por Techo) - ❌ FALLO
**Concepto:** Calcular el número de filas usando el techo de la división (`ceil(25/8) = 4`). Todos los procesos reciben 4 filas, excepto el último, al cual se le restan las filas sobrantes (padding) para compensar.

**Por qué falló:**
Si repartimos 4 filas a los primeros 7 procesos, hemos consumido `4 * 7 = 28` filas. ¡Pero la matriz solo tiene 25!
El cálculo interno del programa para el proceso 7 fue:
* `padding = (4 filas * 8 procesos) - 25 = 7`
* `filas_para_el_ultimo = 4 - 7 = -3`

**Resultado:**
```text
Abort(135448322) on node 0 (rank 0 in comm 0): Fatal error in internal_Send: Invalid count
internal_Send(55627): MPI_Send(..., count=-75, ...) failed
internal_Send(55562): Negative count, value is -75
terminate called after throwing an instance of 'std::length_error'
```
El programa intentó enviar `-3` filas (que equivalen a `-3 * 25 = -75` elementos) a través de `MPI_Send`. Como no se puede enviar una cantidad negativa de datos, MPI lanzó un error fatal. Además, C++ lanzó un `std::length_error` al intentar crear un vector de tamaño negativo.

---

## Opción 2: Estrategia `Padding` (Rellenar la Matriz) - ✅ ÉXITO
**Concepto:** Ya que la división por techo (`ceil(25/8) = 4`) requiere 32 filas en total para que la repartición sea perfecta (4 filas x 8 procesos), **agrandamos la matriz original** añadiendo filas llenas de ceros (del índice 25 al 31).

**Por qué funcionó:**
Todos los procesos reciben exactamente 4 filas, sin excepciones ni cálculos negativos.
* El `RANK_6` recibió la última fila real (la 24) y 3 filas de ceros.
* El `RANK_7` recibió 4 filas completamente llenas de ceros.

**Resultado:**
Al multiplicar una fila de ceros por el vector, el resultado es cero, lo cual no altera los resultados reales de nuestra matriz de 25x25. Al final, simplemente ignoramos esos ceros extras y nos quedamos con los primeros 25 elementos útiles del vector final.
```text
MATRIX_DIM: 25, nprocs: 8, rows_per_rank: 4, ESTRATEGIA: 2
...
RANK_7: recibido 4 x 25
--- Matriz local en RANK_7 (imprimiendo 4x25) ---
  0.0   0.0   0.0 ...
  0.0   0.0   0.0 ...
```

---

## Opción 3: Estrategia `Truncamiento` (División Entera) - ✅ ÉXITO
**Concepto:** Se realiza una división entera normal (`25 / 8 = 3`). A todos los procesos se les asigna esta cantidad base. Las filas que sobran (el residuo `25 % 8 = 1`) se le suman al último proceso.

**Por qué funcionó:**
No hay necesidad de crear filas artificiales ni de tener valores negativos. 
* Los primeros 7 procesos (`RANK_0` al `RANK_6`) reciben exactamente 3 filas cada uno (`7 * 3 = 21` filas repartidas).
* El último proceso (`RANK_7`) recibe su cuota base (3 filas) más el residuo (1 fila) = **4 filas**. `21 + 4 = 25` filas en total.

**Resultado:**
La salida de consola confirmó exactamente este comportamiento:
```text
MATRIX_DIM: 25, nprocs: 8, rows_per_rank: 3, ESTRATEGIA: 3
...
RANK_6: recibido 3 x 25
LLEGO RANK: 6, rows: 3, matrix_dim: 25
...
RANK_7: recibido 4 x 25
LLEGO RANK: 7, rows: 4, matrix_dim: 25
```
Esta es la estrategia más limpia en términos de uso de memoria, ya que no requiere almacenar ceros innecesarios, aunque el último procesador (o los primeros, dependiendo de la implementación) termina con un poco más de carga de trabajo.
