/*
 * graph.c
 * HashMap-based adjacency-list graph with dynamic weight and status
 * updates, validation, and a version counter for cache invalidation.
 */
#include <stdlib.h>
#include "graph.h"

#define GRAPH_INITIAL_CAPACITY 64

Graph *graph_create(void)
{
    Graph *g = (Graph *)malloc(sizeof(Graph));

    if (g == NULL) {
        return NULL;
    }
    g->vertices = hashmap_create(GRAPH_INITIAL_CAPACITY);
    g->by_index = (Vertex **)malloc((size_t)GRAPH_INITIAL_CAPACITY
                                    * sizeof(Vertex *));
    if (g->vertices == NULL || g->by_index == NULL) {
        hashmap_free(g->vertices);
        free(g->by_index);
        free(g);
        return NULL;
    }
    g->num_vertices = 0;
    g->cap_vertices = GRAPH_INITIAL_CAPACITY;
    g->num_logical_roads = 0;
    g->version = 0;
    return g;
}

GraphResult graph_add_intersection(Graph *g, int id)
{
    Vertex *v;

    if (hashmap_contains(g->vertices, id)) {
        return GRAPH_ERR_DUPLICATE_VERTEX;
    }
    if (g->num_vertices == g->cap_vertices) {
        int new_cap = g->cap_vertices * 2;
        Vertex **grown = (Vertex **)realloc(g->by_index,
                                            (size_t)new_cap * sizeof(Vertex *));
        if (grown == NULL) {
            return GRAPH_ERR_ALLOC;
        }
        g->by_index = grown;
        g->cap_vertices = new_cap;
    }
    v = (Vertex *)malloc(sizeof(Vertex));
    if (v == NULL) {
        return GRAPH_ERR_ALLOC;
    }
    v->id = id;
    v->index = g->num_vertices;
    v->edges = NULL;
    v->degree = 0;
    if (hashmap_put(g->vertices, id, v) < 0) {
        free(v);
        return GRAPH_ERR_ALLOC;
    }
    g->by_index[g->num_vertices] = v;
    g->num_vertices++;
    return GRAPH_OK;
}

Vertex *graph_get_vertex(const Graph *g, int id)
{
    return (Vertex *)hashmap_get(g->vertices, id);
}

Edge *graph_find_edge(const Graph *g, int u_id, int v_id)
{
    Vertex *u = graph_get_vertex(g, u_id);
    Edge *e;

    if (u == NULL) {
        return NULL;
    }
    for (e = u->edges; e != NULL; e = e->next) {
        if (e->dest_id == v_id) {
            return e;
        }
    }
    return NULL;
}

/* Prepends one directed edge to a vertex's adjacency list. */
static GraphResult add_directed_edge(Vertex *from, int dest_id, long weight)
{
    Edge *e = (Edge *)malloc(sizeof(Edge));

    if (e == NULL) {
        return GRAPH_ERR_ALLOC;
    }
    e->dest_id = dest_id;
    e->weight = weight;
    e->active = 1;
    e->next = from->edges;
    from->edges = e;
    from->degree++;
    return GRAPH_OK;
}

GraphResult graph_add_road(Graph *g, int u_id, int v_id, long weight)
{
    Vertex *u = graph_get_vertex(g, u_id);
    Vertex *v = graph_get_vertex(g, v_id);
    GraphResult r;

    if (u == NULL || v == NULL) {
        return GRAPH_ERR_INVALID_VERTEX;
    }
    if (u_id == v_id) {
        return GRAPH_ERR_SELF_LOOP;
    }
    if (weight < 0) {
        return GRAPH_ERR_NEGATIVE_WEIGHT;
    }
    if (graph_find_edge(g, u_id, v_id) != NULL) {
        return GRAPH_ERR_DUPLICATE_ROAD;
    }
    r = add_directed_edge(u, v_id, weight);
    if (r != GRAPH_OK) {
        return r;
    }
    r = add_directed_edge(v, u_id, weight);
    if (r != GRAPH_OK) {
        /* Roll back the first half so the road is not left one-way. */
        Edge *half = u->edges;
        u->edges = half->next;
        u->degree--;
        free(half);
        return r;
    }
    g->num_logical_roads++;
    g->version++;
    return GRAPH_OK;
}

GraphResult graph_update_road_weight(Graph *g, int u_id, int v_id,
                                     long new_weight)
{
    Edge *uv;
    Edge *vu;

    if (graph_get_vertex(g, u_id) == NULL
            || graph_get_vertex(g, v_id) == NULL) {
        return GRAPH_ERR_INVALID_VERTEX;
    }
    if (new_weight < 0) {
        return GRAPH_ERR_NEGATIVE_WEIGHT;
    }
    uv = graph_find_edge(g, u_id, v_id);
    vu = graph_find_edge(g, v_id, u_id);
    if (uv == NULL || vu == NULL) {
        return GRAPH_ERR_NO_SUCH_ROAD;
    }
    uv->weight = new_weight;
    vu->weight = new_weight;
    g->version++;
    return GRAPH_OK;
}

GraphResult graph_set_road_status(Graph *g, int u_id, int v_id, int active)
{
    Edge *uv;
    Edge *vu;

    if (graph_get_vertex(g, u_id) == NULL
            || graph_get_vertex(g, v_id) == NULL) {
        return GRAPH_ERR_INVALID_VERTEX;
    }
    uv = graph_find_edge(g, u_id, v_id);
    vu = graph_find_edge(g, v_id, u_id);
    if (uv == NULL || vu == NULL) {
        return GRAPH_ERR_NO_SUCH_ROAD;
    }
    uv->active = active ? 1 : 0;
    vu->active = uv->active;
    g->version++;
    return GRAPH_OK;
}

const char *graph_result_str(GraphResult r)
{
    switch (r) {
    case GRAPH_OK:                  return "OK";
    case GRAPH_ERR_ALLOC:           return "allocation failure";
    case GRAPH_ERR_INVALID_VERTEX:  return "invalid intersection";
    case GRAPH_ERR_DUPLICATE_VERTEX:return "duplicate intersection";
    case GRAPH_ERR_DUPLICATE_ROAD:  return "duplicate road";
    case GRAPH_ERR_NO_SUCH_ROAD:    return "no such road";
    case GRAPH_ERR_NEGATIVE_WEIGHT: return "negative weight rejected";
    case GRAPH_ERR_SELF_LOOP:       return "self-loop rejected";
    default:                        return "unknown";
    }
}

void graph_free(Graph *g)
{
    int i;

    if (g == NULL) {
        return;
    }
    for (i = 0; i < g->num_vertices; i++) {
        Vertex *v = g->by_index[i];
        Edge *e = v->edges;
        while (e != NULL) {
            Edge *next = e->next;
            free(e);
            e = next;
        }
        free(v);
    }
    hashmap_free(g->vertices);
    free(g->by_index);
    free(g);
}
