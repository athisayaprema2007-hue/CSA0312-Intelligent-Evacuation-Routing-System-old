# Intelligent Emergency Evacuation Routing System

CSA0312 – Data Structures – Slot B final assignment.
A modular C prototype that routes evacuees through a city road network of
more than 8,000 intersections and 20,000 weighted roads, under disaster
conditions where road costs change and roads become blocked. It is a
course prototype evaluated on synthetic data, not a deployed system.

## Purpose

During floods, earthquakes, fires or industrial accidents, travel costs
on city roads change continuously and some roads become impassable. The
system answers "what is the minimum-cost open route from disaster
location S to evacuation center D right now?" quickly, repeatedly, and
for many (S, D) pairs, while detecting unreachable evacuation centers.

## Features

- HashMap-based adjacency list for a sparse network (no adjacency matrix)
- Bidirectional roads with non-negative, safety-adjusted travel costs
- Dynamic road-weight updates and road blocking/reopening
- Min-heap (insert, extract-min, heapify, decrease-key) driving Dijkstra
- HashSet of processed intersections – no intersection is processed twice,
  so cyclic networks are handled safely
- Route reconstruction with full path and total travel cost
- Route cache with graph-version invalidation for repeated queries
- Validation: invalid vertices, duplicate roads, self-loops, negative
  weights, unreachable destinations
- Deterministic synthetic city (fixed seed) for benchmarking:
  8,200 intersections, 21,000 logical roads
- 30% road-failure experiment with reachability/cost/runtime analysis
- All dynamic memory is freed (create/free pairs in every module)

## Data structures (all in `src/`)

| Structure | File | Role |
|---|---|---|
| `Vertex` | graph.h | one intersection (ID, dense index, adjacency list) |
| `Edge` | graph.h | one directed half of a road (dest, weight, active flag) |
| `Graph` | graph.h | HashMap of vertices + version counter |
| `HashMapEntry`, `HashMap` | hashmap.h | intersection-ID → Vertex lookup |
| `HashSetEntry`, `HashSet` | hashset.h | processed-intersection set in Dijkstra |
| `HeapNode`, `MinHeap` | minheap.h | priority queue for Dijkstra |
| `Route` | dijkstra.h | reconstructed path + total cost |
| `RouteCacheEntry`, `RouteCache` | route_cache.h | version-stamped route cache |

## Building

Preferred (any modern GCC/Clang, Linux/macOS/MinGW with make):

```bash
make
```

Windows without make:

```bash
./build.bat
```

The code is standard C (valid C99 and C11). The Makefile uses
`gcc -std=c11 -O2 -Wall -Wextra -Wpedantic`; `build.bat` uses
`-std=c99 -O2 -Wall -Wextra -pedantic` so it also works on the old
GCC 3.4.x shipped with Dev-C++ (which predates `-std=c11`).
Note: on some modern Windows systems the GCC 3.4.2 `collect2` link
wrapper crashes; the object files still compile and can be linked by
invoking `ld` directly (documented in the report, section 15).

## Interactive Console Interface

```bash
bin/evacsim
```

Running `bin\evacsim.exe` with **no argument** opens a polished
interactive console dashboard (colour-coded on Windows via
`SetConsoleTextAttribute`, ANSI colours elsewhere, plain text when
output is redirected). Menu functions:

1. **Interactive Evacuation Route Planner** – a small demonstration
   city built through the normal graph API: view intersections and
   roads, find routes between any two IDs, update road weights, block
   and reopen roads, inspect route-cache statistics, and reset the
   city. All input is validated; invalid text, unknown IDs, negative
   weights, missing roads and unreachable destinations produce clear
   messages instead of crashes.
2. **Guided System Demonstration** – the same automated walkthrough as
   `bin/evacsim demo`.
3. **Full-Scale Performance Benchmark** – the same run as
   `bin/evacsim bench results/benchmark_results.csv`.
4. **30% Road Failure Simulation** – the same run as
   `bin/evacsim fail results/road_failure_analysis.txt`.
5. **System Architecture and Features** – module and complexity
   overview.

The dashboard is presentation only (`src/console_ui.c`); the graph,
hashing, heap, Dijkstra, cache and test logic are untouched, and the
command-line modes below remain available for automated evaluation.
This is a console-based C data-structures prototype, not a deployed
navigation application.

## Running

```bash
bin/evacsim demo
```
Narrated small-city walkthrough: adding intersections and roads,
updating a weight, blocking/reopening a road, cache miss/hit/
invalidation, multiple queries, unreachable and invalid queries.

```bash
bin/test_runner results/test_results.txt
```
Runs the 16 functional tests and writes expected/actual/PASS-FAIL
for each into `results/test_results.txt` (exit code 0 only if all pass).

```bash
bin/evacsim bench results/benchmark_results.csv
```
Generates the deterministic 8,200-vertex / 21,000-road city
(seed 20260831), times 10 routing queries uncached and cached
(min/avg/max over 5 passes), and writes the CSV. Graph generation and
file writing are excluded from all timed regions.

```bash
bin/evacsim fail results/road_failure_analysis.txt
```
Rebuilds the same city, records baseline routes/timings, blocks exactly
30% of logical roads (deterministic Fisher–Yates selection), reruns the
same queries and writes the comparison.

## Complexity

| Operation | Time | Space |
|---|---|---|
| Vertex lookup (HashMap) | O(1) average | O(V) |
| Add road / update / block | O(1) avg + O(deg) edge scan | O(1) |
| Min-heap insert / extract-min / decrease-key | O(log V) | O(V) |
| Dijkstra (binary heap + adjacency list) | O((V + E) log V) | O(V) |
| Route reconstruction | O(L) for an L-hop route | O(L) |
| Cache lookup / insert | O(1) average | O(routes cached) |
| Graph storage | – | O(V + E) |

Floyd–Warshall (O(V³) time, O(V²) space) is analysed in the report and
shown to be unsuitable at this scale (~8,200 vertices).

## Limitations

- Single-threaded prototype; no real map data, GPS input or live feeds
- Costs are single scalar "safety-adjusted" values, not separate
  distance/safety objectives
- Cache invalidation is global (any change invalidates all cached
  routes), which is correct but coarse
- clock() timing has ~1 ms resolution; fast operations are timed in
  batches and averaged
- Tested on synthetic data only; no real disaster validation is claimed

See `report/CSA0312_Final_Assignment_Report.md` for the full report and
`report/rubric_checklist.md` for the rubric mapping.
