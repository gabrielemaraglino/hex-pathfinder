# Efficient Pathfinding on Hexagonal Grids

This project implements an efficient C program for dynamic pathfinding on hexagonal grids with weighted terrain costs and optional “air routes”.
The implementation leverages BFS and Dijkstra’s algorithm in combination with custom data structures and caching strategies, optimizing both runtime and memory usage under stress-test conditions, and making it robust for scenarios involving frequent queries and dynamic map updates.

## Main Features

- **init** – initialize a hexagonal map with default costs.
- **change_cost** – update terrain exit costs in a given region.
- **toggle_air_route** – add or remove teleport-like air connections between hexagons.
- **travel_cost** – compute the shortest path using Dijkstra’s algorithm.

This project was developed as part of the university course *Algorithms and Theoretical Computer Science* and was awarded the maximum grade (**30/30 cum laude**).


