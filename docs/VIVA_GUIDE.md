# Viva Guide

Use this as a short, honest explanation of the project during a demonstration or viva.

## Opening answer

This project models a disaster-prone city as a weighted graph. Each intersection is a vertex and each road is an edge with a safety-adjusted cost. A HashMap-based adjacency list stores the sparse network. Dijkstra's algorithm, driven by a binary Min-Heap, finds the least-cost active evacuation route. A HashSet finalizes vertices once, and a version-stamped route cache makes repeated queries fast while forcing safe recomputation after a road changes.

## Key facts to quote

- **16/16** functional tests passed.
- Benchmark city: **8,200 intersections** and **21,000 logical bidirectional roads**.
- The evaluated uncached routes were well below the **200 ms** requirement on the recorded machine.
- The failure experiment blocked exactly **6,300 roads (30%)**.
- After that failure, **8,026 of 8,200** intersections remained reachable from the reference source.

## Five likely questions

1. **Why an adjacency list instead of a matrix?** The graph is sparse, so an adjacency list stores only real roads in O(V+E) memory instead of O(V^2).
2. **Why Dijkstra?** All weights are non-negative, so it returns the minimum-cost route. With the binary heap it runs in O((V+E) log V).
3. **What happens when a road is blocked?** Its active flag becomes 0, the graph version increments, stale cache entries are rejected, and a new route search ignores the road.
4. **How is the displayed route reconstructed?** Dijkstra stores predecessor IDs in `prev[]`; the destination is traced backward and reversed into source-to-destination order.
5. **What if no route exists?** The result is marked unreachable with `reachable = 0` and `total_cost = -1`; the program does not make up a route.

## Live demonstration

On Windows, run:

```bat
build.bat
bin\evacsim.exe
```

Use the interactive planner for routes, weight updates, blocking/reopening roads, and cache statistics. For the scripted modes:

```bat
bin\evacsim.exe demo
bin\test_runner.exe results\test_results.txt
bin\evacsim.exe bench results\benchmark_results.csv
bin\evacsim.exe fail results\road_failure_analysis.txt
```

## Honest scope

This is a course prototype using a deterministic synthetic city. It demonstrates the data-structure and routing design; it is not a deployed GPS or emergency-service system.
