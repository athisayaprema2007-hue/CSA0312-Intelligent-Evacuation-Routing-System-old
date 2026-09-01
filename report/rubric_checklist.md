# Rubric Compliance Checklist

Maps every rubric area and every required deliverable of the CSA0312
Slot B assignment to the exact source file, test, or report section that
satisfies it. Report sections refer to
`report/CSA0312_Final_Assignment_Report.md`.

## A. Rubric areas

### 1. Problem Understanding & ADT Selection — 15 marks (CO2)

| Excellent descriptor | Evidence |
|---|---|
| Clearly analyzes disaster-routing requirements | Report §1 (problem understanding), §2 (all 12 requirements enumerated), §3 (assumptions incl. the safety-cost assumption and bidirectionality) |
| Selects appropriate ADTs with strong justification | Report §4: adjacency-matrix vs HashMap-adjacency-list memory/time argument (~269 MB vs a few MB), requirement→ADT table |

### 2. Graph, HashMap & HashSet Design — 20 marks (CO2/CO4)

| Excellent descriptor | Evidence |
|---|---|
| Efficient graph representation | `src/graph.h` / `src/graph.c`: HashMap-based adjacency list, O(V+E) storage, version counter; design notes in the header comment |
| Correct HashMap integration | `src/hashmap.c`; genuinely used for graph storage/vertex lookup: `Graph.vertices` in `graph_create`, `graph_get_vertex`, and every relaxation in `dijkstra_core` |
| Correct HashSet integration | `src/hashset.c`; genuinely used inside Dijkstra: `processed` set in `dijkstra_core` (`src/dijkstra.c`), verified by functional test 15 |
| Robust edge handling | duplicate roads, self-loops, negative weights, invalid vertices all rejected (`graph.c`); tests 6–9 |

### 3. Min-Heap & Priority Queue Integration — 20 marks (CO4)

| Excellent descriptor | Evidence |
|---|---|
| Correct insert | `minheap_insert` + `minheap_sift_up` (`src/minheap.c`) |
| Correct extract-min | `minheap_extract_min` (`src/minheap.c`) |
| Correct heapify | `minheap_heapify` (iterative sift-down, `src/minheap.c`) |
| Correct decrease-key | `minheap_decrease_key` with O(1) `pos[]` reverse index (`src/minheap.c`) |
| Efficient integration | Dijkstra uses insert/decrease-key/extract-min directly (`dijkstra_core` in `src/dijkstra.c`) — no array scan anywhere; report §7, pseudocode §12.6 |

### 4. Dijkstra & Route Reconstruction — 20 marks (CO5)

| Excellent descriptor | Evidence |
|---|---|
| Correct shortest route | tests 1, 2, 11 (hand-verified graph); report §9 |
| Dynamic weights | `graph_update_road_weight`; test 3 (update changes the best route) |
| Blocked roads | `active` flag skipped in `dijkstra_core`; tests 4 (block) and 5 (reopen) |
| Route reconstruction | `reconstruct_route` in `src/dijkstra.c`; tests 1–5 verify full vertex sequences; pseudocode §12.8 |
| Unreachable handling | `Route.reachable == 0`; tests 10, 16 |

### 5. Performance, Testing & Complexity Analysis — 15 marks (CO4/CO5)

| Excellent descriptor | Evidence |
|---|---|
| Comprehensive testing | 16 functional tests with expected/actual/PASS-FAIL in `results/test_results.txt` (16/16 PASS); report §13 |
| Scalability / performance analysis | `results/benchmark_results.csv` (8,200 vertices, 21,000 logical roads, min/avg/max uncached and cached timings); report §14–§16 (200 ms evaluation) |
| Failure analysis | `results/road_failure_analysis.txt` (30% deterministic road failure); report §17 |
| Complexity analysis | report §19 (full table), §18 (Dijkstra vs Floyd–Warshall with exact complexities) |

### 6. Reflection & SDG Relevance — 10 marks (CO2/CO5)

| Excellent descriptor | Evidence |
|---|---|
| Strong technical reflection | Report §21 (design lessons: heap position index, version-stamp invalidation, failure-path testing) |
| Clear SDG 9, 11, 13 connection | Report §22 (each SDG tied to a concrete system capability, incl. targets 11.5/11.b) |

## B. Required deliverables

| Deliverable | Location |
|---|---|
| Pseudocode (all 11 required topics) | Report §12.1–§12.11 |
| Complete modular C implementation | `src/` (8 modules: graph, hashmap, hashset, minheap, dijkstra, route_cache, main) + `tests/test_runner.c` |
| Test cases and results | `tests/test_runner.c`; `results/test_results.txt` (expected/actual/PASS-FAIL, 16/16) |
| Complexity analysis | Report §19; summary table in `README.md` |
| Dijkstra vs Floyd–Warshall comparison | Report §18 |
| Performance analysis | Report §14–§17; `results/benchmark_results.csv`; `results/road_failure_analysis.txt` |
| GitHub repository | Published at https://github.com/athisayaprema2007-hue/CSA0312-Intelligent-Evacuation-Routing-System (full history, `.gitignore`); the URL is also recorded in the report |
| Reflection + SDG relevance | Report §21–§22 |

## C. Required system behaviours (from the problem statement)

| Constraint | Where demonstrated |
|---|---|
| 8,000+ vertices, 20,000+ weighted edges | benchmark city: 8,200 / 21,000 logical (`main.c: build_city`; CSV meta rows) |
| Frequently changing weights | `graph_update_road_weight`; demo step 5; test 3 |
| Blocked roads removed/disabled | `graph_set_road_status`; demo steps 6–7; tests 4–5 |
| Multiple source-to-destination queries | demo step 8; test 11; 10 benchmark queries |
| Repeated queries efficient | route cache; tests 12–13; cached timings in CSV |
| Moderate memory | O(V+E) adjacency list; report §4, §19 |
| No repeated processing of intersections | HashSet in `dijkstra_core`; test 15 |
| Minimum travel cost prioritized | Dijkstra with min-heap; tests 1–5 |
| Unreachable centers identified | tests 10, 16; demo step 9 |
| Implemented in C | entire `src/`, standard C (valid C99 and C11) |
| Route caching with invalidation | `src/route_cache.c`; tests 12–14; graph `version` mechanism |
| 200 ms evaluation | report §16 (met on the stated machine: worst 7.000 ms) |
| 30% road failure behaviour | report §17; `results/road_failure_analysis.txt` |
