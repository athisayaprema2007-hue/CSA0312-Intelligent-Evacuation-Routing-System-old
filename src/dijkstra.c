/*
 * dijkstra.c
 * Dijkstra's algorithm over the HashMap-based adjacency list, driven by
 * the binary min-heap (insert / extract-min / decrease-key) and guarded
 * by a HashSet of already-finalized intersections.
 */
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "dijkstra.h"
#include "minheap.h"
#include "hashset.h"

#define DIST_INF LONG_MAX

/*
 * Core search shared by the two public functions. Fills dist[] and
 * prev[] (both indexed by dense vertex index; prev holds the dense index
 * of the predecessor, or -1). If dst_index >= 0 the search stops as soon
 * as that vertex is finalized; pass -1 to settle every reachable vertex.
 * Returns 0 on success, -1 on allocation failure.
 */
static int dijkstra_core(const Graph *g, int src_index, int dst_index,
                         long *dist, int *prev, DijkstraStats *stats)
{
    int n = g->num_vertices;
    MinHeap *heap = minheap_create(n);
    HashSet *processed = hashset_create(n);
    int i;

    if (heap == NULL || processed == NULL) {
        minheap_free(heap);
        hashset_free(processed);
        return -1;
    }
    for (i = 0; i < n; i++) {
        dist[i] = DIST_INF;
        prev[i] = -1;
    }
    dist[src_index] = 0;
    minheap_insert(heap, src_index, 0);

    while (!minheap_is_empty(heap)) {
        HeapNode min = minheap_extract_min(heap);
        Vertex *u = g->by_index[min.vertex_index];
        Edge *e;

        /* The HashSet guarantees each intersection is finalized at most
         * once, so cycles in the road network cannot cause re-work. */
        if (hashset_contains(processed, min.vertex_index)) {
            continue;
        }
        hashset_add(processed, min.vertex_index);
        if (stats != NULL) {
            stats->vertices_processed++;
        }
        if (min.vertex_index == dst_index) {
            break;   /* destination finalized: its distance is optimal */
        }
        for (e = u->edges; e != NULL; e = e->next) {
            Vertex *v;
            long candidate;

            if (!e->active) {
                continue;   /* blocked road */
            }
            v = graph_get_vertex(g, e->dest_id);
            if (hashset_contains(processed, v->index)) {
                continue;
            }
            if (stats != NULL) {
                stats->edges_relaxed++;
            }
            candidate = dist[min.vertex_index] + e->weight;
            if (candidate < dist[v->index]) {
                dist[v->index] = candidate;
                prev[v->index] = min.vertex_index;
                if (minheap_contains(heap, v->index)) {
                    minheap_decrease_key(heap, v->index, candidate);
                } else {
                    minheap_insert(heap, v->index, candidate);
                }
            }
        }
    }
    minheap_free(heap);
    hashset_free(processed);
    return 0;
}

/* Walks prev[] backwards from the destination and reverses the result
 * into a Route holding external intersection IDs in travel order. */
static Route *reconstruct_route(const Graph *g, int src_id, int dst_id,
                                const long *dist, const int *prev,
                                int dst_index)
{
    Route *route = (Route *)malloc(sizeof(Route));
    int hops;
    int at;
    int i;

    if (route == NULL) {
        return NULL;
    }
    route->src_id = src_id;
    route->dst_id = dst_id;
    if (dist[dst_index] == DIST_INF) {
        route->intersection_ids = NULL;
        route->num_intersections = 0;
        route->total_cost = -1;
        route->reachable = 0;
        return route;
    }
    hops = 1;
    for (at = dst_index; prev[at] != -1; at = prev[at]) {
        hops++;
    }
    route->intersection_ids = (int *)malloc((size_t)hops * sizeof(int));
    if (route->intersection_ids == NULL) {
        free(route);
        return NULL;
    }
    at = dst_index;
    for (i = hops - 1; i >= 0; i--) {
        route->intersection_ids[i] = g->by_index[at]->id;
        at = prev[at];
    }
    route->num_intersections = hops;
    route->total_cost = dist[dst_index];
    route->reachable = 1;
    return route;
}

Route *dijkstra_shortest_route(const Graph *g, int src_id, int dst_id,
                               DijkstraStats *stats)
{
    Vertex *src = graph_get_vertex(g, src_id);
    Vertex *dst = graph_get_vertex(g, dst_id);
    long *dist;
    int *prev;
    Route *route;

    if (src == NULL || dst == NULL) {
        return NULL;   /* invalid source or destination */
    }
    if (stats != NULL) {
        stats->vertices_processed = 0;
        stats->edges_relaxed = 0;
    }
    dist = (long *)malloc((size_t)g->num_vertices * sizeof(long));
    prev = (int *)malloc((size_t)g->num_vertices * sizeof(int));
    if (dist == NULL || prev == NULL) {
        free(dist);
        free(prev);
        return NULL;
    }
    if (dijkstra_core(g, src->index, dst->index, dist, prev, stats) != 0) {
        free(dist);
        free(prev);
        return NULL;
    }
    route = reconstruct_route(g, src_id, dst_id, dist, prev, dst->index);
    free(dist);
    free(prev);
    return route;
}

long dijkstra_count_reachable(const Graph *g, int src_id)
{
    Vertex *src = graph_get_vertex(g, src_id);
    long *dist;
    int *prev;
    long count = 0;
    int i;

    if (src == NULL) {
        return -1;
    }
    dist = (long *)malloc((size_t)g->num_vertices * sizeof(long));
    prev = (int *)malloc((size_t)g->num_vertices * sizeof(int));
    if (dist == NULL || prev == NULL) {
        free(dist);
        free(prev);
        return -1;
    }
    if (dijkstra_core(g, src->index, -1, dist, prev, NULL) != 0) {
        free(dist);
        free(prev);
        return -1;
    }
    for (i = 0; i < g->num_vertices; i++) {
        if (dist[i] != DIST_INF) {
            count++;
        }
    }
    free(dist);
    free(prev);
    return count;
}

Route *route_clone(const Route *route)
{
    Route *copy;
    int i;

    if (route == NULL) {
        return NULL;
    }
    copy = (Route *)malloc(sizeof(Route));
    if (copy == NULL) {
        return NULL;
    }
    *copy = *route;
    copy->intersection_ids = NULL;
    if (route->num_intersections > 0) {
        copy->intersection_ids =
            (int *)malloc((size_t)route->num_intersections * sizeof(int));
        if (copy->intersection_ids == NULL) {
            free(copy);
            return NULL;
        }
        for (i = 0; i < route->num_intersections; i++) {
            copy->intersection_ids[i] = route->intersection_ids[i];
        }
    }
    return copy;
}

void route_print(const Route *route)
{
    int i;

    if (route == NULL) {
        printf("(no route: invalid query)\n");
        return;
    }
    if (!route->reachable) {
        printf("Destination %d is UNREACHABLE from %d over active roads.\n",
               route->dst_id, route->src_id);
        return;
    }
    printf("Route: ");
    for (i = 0; i < route->num_intersections; i++) {
        if (i > 0) {
            printf(" -> ");
        }
        printf("%d", route->intersection_ids[i]);
    }
    printf("  (total travel cost %ld, %d intersections)\n",
           route->total_cost, route->num_intersections);
}

void route_free(Route *route)
{
    if (route == NULL) {
        return;
    }
    free(route->intersection_ids);
    free(route);
}
