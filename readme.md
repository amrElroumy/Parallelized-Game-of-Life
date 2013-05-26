# Parallelized Conway's Game of Life

Implemented using MPI and OMP.

## Rules

- Any live cell with fewer that two live neighbours dies, as if caused by under-population.
- Any live cell with two or three live neighbours makes it to the next generation.
- Any live cell with more than three live neighbours dies, as if by overcrowding.
- Any dead cell with exactly three live neighbours becomes a live cell, as if by reproduction.
