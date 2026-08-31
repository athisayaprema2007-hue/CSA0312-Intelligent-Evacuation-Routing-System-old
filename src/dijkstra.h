/*
 * dijkstra.h
 * Min-heap-optimized Dijkstra shortest-path search with route
 * reconstruction. Blocked roads (active == 0) are skipped, a HashSet
 * records finalized intersections so none is processed twice, and the
 * search stops early once the destination is finalized.
 */
#ifndef DIJKSTRA_H
#define DIJKSTRA_H

#include "graph.h"

/* A reconstructed evacuation route. */
typedef struct Route {
    int src_id;
    int dst_id;
    int *intersection_ids;   /* src ... dst, in travel order */
    int num_intersections;   /* 0 when unreachable */
    long total_cost;         /* -1 when unreachable */
    int reachable;           /* 1 = route exists, 0 = unreachable */
} Route;

/* Work counters, filled when a non-NULL pointer is passed in. */
typedef struct DijkstraStats {
    int vertices_processed;  /* extract-min operations performed */
    long edges_relaxed;      /* active edges examined */
} DijkstraStats;

/* Computes the minimum-cost route from src_id to dst_id.
 * Returns NULL if either ID is not an intersection (or on allocation
 * failure). An unreachable destination returns a Route with
 * reachable == 0 — that is a valid answer, not an error.
 * The caller frees the result with route_free(). */
Route *dijkstra_shortest_route(const Graph *g, int src_id, int dst_id,
                               DijkstraStats *stats);

/* Number of intersections reachable from src_id over active roads,
 * including src_id itself. Returns -1 for an unknown source. */
long dijkstra_count_reachable(const Graph *g, int src_id);

/* Deep-copies a route (used by the route cache). */
Route *route_clone(const Route *route);

/* Prints "id -> id -> ... (total cost C)" or an unreachable notice. */
void route_print(const Route *route);

void route_free(Route *route);

#endif /* DIJKSTRA_H */
