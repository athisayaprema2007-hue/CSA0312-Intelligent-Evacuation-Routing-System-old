/*
 * graph.h
 * City road network as a HashMap-based adjacency list.
 *
 * Design decisions (documented in the report as well):
 *  - Roads are BIDIRECTIONAL: one logical road between intersections u and
 *    v is stored as two directed edges (u->v and v->u) that are always
 *    updated together. num_logical_roads counts each road once.
 *  - Every road weight is a non-negative "safety-adjusted travel cost"
 *    that already folds in distance, congestion, damage and emergency
 *    restrictions. Negative weights are rejected.
 *  - A blocked road is kept in the list with active == 0, so it can be
 *    reopened later without losing its weight.
 *  - graph->version increases on every mutation that can change routing
 *    (road added, weight updated, road blocked/reopened). The route cache
 *    compares this number to detect stale cached routes.
 */
#ifndef GRAPH_H
#define GRAPH_H

#include "hashmap.h"

/* Result codes for every mutating graph operation. */
typedef enum {
    GRAPH_OK = 0,
    GRAPH_ERR_ALLOC,
    GRAPH_ERR_INVALID_VERTEX,
    GRAPH_ERR_DUPLICATE_VERTEX,
    GRAPH_ERR_DUPLICATE_ROAD,
    GRAPH_ERR_NO_SUCH_ROAD,
    GRAPH_ERR_NEGATIVE_WEIGHT,
    GRAPH_ERR_SELF_LOOP
} GraphResult;

/* One directed half of a road, stored in the source vertex's list. */
typedef struct Edge {
    int dest_id;        /* ID of the destination intersection */
    long weight;        /* non-negative safety-adjusted travel cost */
    int active;         /* 1 = open, 0 = blocked */
    struct Edge *next;
} Edge;

/* One intersection. */
typedef struct Vertex {
    int id;             /* external intersection ID (any int) */
    int index;          /* dense 0..n-1 index for algorithm arrays */
    Edge *edges;        /* head of the adjacency list */
    int degree;         /* number of outgoing directed edges */
} Vertex;

/* The whole network. */
typedef struct Graph {
    HashMap *vertices;        /* intersection ID -> Vertex* */
    Vertex **by_index;        /* dense index -> Vertex* */
    int num_vertices;
    int cap_vertices;         /* allocated length of by_index */
    long num_logical_roads;   /* each bidirectional road counted once */
    unsigned long version;    /* bumped on every routing-relevant change */
} Graph;

Graph *graph_create(void);

/* Adds an intersection with the given ID. */
GraphResult graph_add_intersection(Graph *g, int id);

/* Adds one bidirectional road (two directed edges). Rejects unknown
 * intersections, self-loops, negative weights and duplicate roads. */
GraphResult graph_add_road(Graph *g, int u_id, int v_id, long weight);

/* Changes the weight of an existing road in both directions. */
GraphResult graph_update_road_weight(Graph *g, int u_id, int v_id,
                                     long new_weight);

/* Blocks (active = 0) or reopens (active = 1) an existing road in both
 * directions. Blocking an already-blocked road is a harmless no-op. */
GraphResult graph_set_road_status(Graph *g, int u_id, int v_id, int active);

/* Returns the vertex for an ID, or NULL if the ID is unknown. */
Vertex *graph_get_vertex(const Graph *g, int id);

/* Returns the directed edge u->v, or NULL if absent. */
Edge *graph_find_edge(const Graph *g, int u_id, int v_id);

/* Human-readable name for a result code. */
const char *graph_result_str(GraphResult r);

/* Frees the graph and everything it owns. */
void graph_free(Graph *g);

#endif /* GRAPH_H */
