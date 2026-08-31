/*
 * main.c
 * Intelligent Emergency Evacuation Routing System - CSA0312 Slot B.
 *
 * Modes:
 *   evacsim demo                 - narrated small-city walkthrough
 *   evacsim bench [csv_path]     - large-network benchmark (>8000
 *                                  intersections, >20000 logical roads)
 *   evacsim fail  [txt_path]     - 30% road-failure experiment
 *
 * Timing uses standard C clock(); graph generation and file writing are
 * kept outside every timed region.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "graph.h"
#include "dijkstra.h"
#include "route_cache.h"
#include "console_ui.h"

/* ------------------------------------------------------------------ */
/* Deterministic PRNG (xorshift32). rand() is avoided because its      */
/* RAND_MAX and sequence differ between C libraries; this generator    */
/* reproduces the exact same synthetic city on every platform.         */
/* ------------------------------------------------------------------ */

#define CITY_SEED 20260831UL

static unsigned long rng_state = CITY_SEED;

static void rng_seed(unsigned long seed)
{
    rng_state = (seed == 0) ? 1 : seed;
}

static unsigned long rng_next(void)
{
    rng_state ^= (rng_state << 13) & 0xFFFFFFFFUL;
    rng_state ^= rng_state >> 17;
    rng_state ^= (rng_state << 5) & 0xFFFFFFFFUL;
    rng_state &= 0xFFFFFFFFUL;
    return rng_state;
}

static int rng_range(int n)
{
    return (int)(rng_next() % (unsigned long)n);
}

/* ------------------------------------------------------------------ */
/* Synthetic city generation (deterministic, fixed seed)               */
/* ------------------------------------------------------------------ */

#define CITY_VERTICES 8200
#define CITY_ROADS    21000   /* logical bidirectional roads */
#define MAX_WEIGHT    100
#define NUM_QUERIES   10

typedef struct RoadList {
    int *u;
    int *v;
    long count;
} RoadList;

/*
 * Builds a connected city: vertices 0..CITY_VERTICES-1; first a random
 * spanning tree (each vertex i>0 links to a random earlier vertex, which
 * guarantees every intersection is reachable initially), then extra
 * random roads until CITY_ROADS logical roads exist. Weights are 1..100.
 * Every generated road is recorded in `roads` for the failure experiment.
 */
static Graph *build_city(RoadList *roads)
{
    Graph *g = graph_create();
    long added = 0;
    int i;

    if (g == NULL) {
        return NULL;
    }
    roads->u = (int *)malloc((size_t)CITY_ROADS * sizeof(int));
    roads->v = (int *)malloc((size_t)CITY_ROADS * sizeof(int));
    roads->count = 0;
    if (roads->u == NULL || roads->v == NULL) {
        free(roads->u);
        free(roads->v);
        graph_free(g);
        return NULL;
    }
    rng_seed(CITY_SEED);
    for (i = 0; i < CITY_VERTICES; i++) {
        graph_add_intersection(g, i);
    }
    for (i = 1; i < CITY_VERTICES; i++) {
        int parent = rng_range(i);
        long w = 1 + rng_range(MAX_WEIGHT);
        if (graph_add_road(g, i, parent, w) == GRAPH_OK) {
            roads->u[roads->count] = i;
            roads->v[roads->count] = parent;
            roads->count++;
            added++;
        }
    }
    while (added < CITY_ROADS) {
        int a = rng_range(CITY_VERTICES);
        int b = rng_range(CITY_VERTICES);
        long w = 1 + rng_range(MAX_WEIGHT);
        if (a != b && graph_add_road(g, a, b, w) == GRAPH_OK) {
            roads->u[roads->count] = a;
            roads->v[roads->count] = b;
            roads->count++;
            added++;
        }
        /* duplicates and self-loops are rejected and simply retried */
    }
    return g;
}

/* Fixed, deterministic query set reused by bench and fail modes. */
static void make_queries(int src[NUM_QUERIES], int dst[NUM_QUERIES])
{
    int i;

    rng_seed(CITY_SEED ^ 0x5A5A5A5AUL);
    for (i = 0; i < NUM_QUERIES; i++) {
        src[i] = rng_range(CITY_VERTICES);
        dst[i] = rng_range(CITY_VERTICES);
        if (src[i] == dst[i]) {
            dst[i] = (dst[i] + 1) % CITY_VERTICES;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Timing helpers (standard C clock(); resolution is about 1 ms, so    */
/* fast operations are timed in batches and averaged).                 */
/* ------------------------------------------------------------------ */

#define UNCACHED_REPS_PER_PASS 3
#define CACHED_LOOKUPS_PER_PASS 20000
#define TIMING_PASSES 5

/* Average milliseconds of one uncached Dijkstra query (route allocation
 * and freeing included, since they are part of answering a query). */
static double time_uncached_once(Graph *g, int s, int d)
{
    clock_t t0;
    clock_t t1;
    int r;

    t0 = clock();
    for (r = 0; r < UNCACHED_REPS_PER_PASS; r++) {
        Route *route = dijkstra_shortest_route(g, s, d, NULL);
        route_free(route);
    }
    t1 = clock();
    return (double)(t1 - t0) * 1000.0 / (double)CLOCKS_PER_SEC
           / (double)UNCACHED_REPS_PER_PASS;
}

/* Average milliseconds of one cached lookup (cache must be primed). */
static double time_cached_once(Graph *g, RouteCache *cache, int s, int d)
{
    clock_t t0;
    clock_t t1;
    int r;

    t0 = clock();
    for (r = 0; r < CACHED_LOOKUPS_PER_PASS; r++) {
        evac_query(g, cache, s, d, NULL);
    }
    t1 = clock();
    return (double)(t1 - t0) * 1000.0 / (double)CLOCKS_PER_SEC
           / (double)CACHED_LOOKUPS_PER_PASS;
}

typedef struct TimingSummary {
    double min_ms;
    double avg_ms;
    double max_ms;
} TimingSummary;

static void summarize(const double *samples, int n, TimingSummary *out)
{
    double total = 0.0;
    int i;

    out->min_ms = samples[0];
    out->max_ms = samples[0];
    for (i = 0; i < n; i++) {
        total += samples[i];
        if (samples[i] < out->min_ms) {
            out->min_ms = samples[i];
        }
        if (samples[i] > out->max_ms) {
            out->max_ms = samples[i];
        }
    }
    out->avg_ms = total / (double)n;
}

/* ------------------------------------------------------------------ */
/* Mode: demo                                                          */
/* ------------------------------------------------------------------ */

static void demo_query(Graph *g, RouteCache *cache, int s, int d)
{
    int from_cache = 0;
    const Route *route = evac_query(g, cache, s, d, &from_cache);

    printf("Query %d -> %d [%s]: ", s, d,
           from_cache ? "cache HIT" : "cache MISS, Dijkstra run");
    if (route == NULL) {
        printf("rejected: invalid intersection ID\n");
        return;
    }
    route_print(route);
}

static int run_demo(void)
{
    Graph *g = graph_create();
    RouteCache *cache = route_cache_create(16);
    GraphResult r;
    int ids[8];
    int i;

    if (g == NULL || cache == NULL) {
        graph_free(g);
        route_cache_free(cache);
        return 1;
    }
    printf("=== Intelligent Emergency Evacuation Routing System: demo ===\n\n");

    printf("[1] Adding 8 intersections (IDs 1..8; 8 stays isolated)...\n");
    for (i = 0; i < 8; i++) {
        ids[i] = i + 1;
        r = graph_add_intersection(g, ids[i]);
        printf("    add intersection %d: %s\n", ids[i], graph_result_str(r));
    }

    printf("\n[2] Adding bidirectional roads (weight = safety-adjusted cost)...\n");
    printf("    road 1-2 w4:  %s\n", graph_result_str(graph_add_road(g, 1, 2, 4)));
    printf("    road 1-3 w1:  %s\n", graph_result_str(graph_add_road(g, 1, 3, 1)));
    printf("    road 3-2 w2:  %s\n", graph_result_str(graph_add_road(g, 3, 2, 2)));
    printf("    road 2-4 w5:  %s\n", graph_result_str(graph_add_road(g, 2, 4, 5)));
    printf("    road 3-4 w12: %s\n", graph_result_str(graph_add_road(g, 3, 4, 12)));
    printf("    road 4-5 w3:  %s\n", graph_result_str(graph_add_road(g, 4, 5, 3)));
    printf("    road 5-6 w2:  %s\n", graph_result_str(graph_add_road(g, 5, 6, 2)));
    printf("    road 6-7 w1:  %s\n", graph_result_str(graph_add_road(g, 6, 7, 1)));
    printf("    duplicate road 1-2 again: %s\n",
           graph_result_str(graph_add_road(g, 1, 2, 9)));
    printf("    negative road 1-4 w-3:    %s\n",
           graph_result_str(graph_add_road(g, 1, 4, -3)));

    printf("\n[3] First query (evacuation center at 4): expect cache MISS.\n");
    demo_query(g, cache, 1, 4);

    printf("\n[4] Repeated query: expect cache HIT with identical route.\n");
    demo_query(g, cache, 1, 4);

    printf("\n[5] Disaster update: congestion on road 3-2, weight 2 -> 10.\n");
    r = graph_update_road_weight(g, 3, 2, 10);
    printf("    update: %s (graph version is now %lu)\n",
           graph_result_str(r), g->version);
    printf("    Same query again: expect cache MISS (entry invalidated).\n");
    demo_query(g, cache, 1, 4);

    printf("\n[6] Road 2-4 is blocked by debris.\n");
    r = graph_set_road_status(g, 2, 4, 0);
    printf("    block: %s\n", graph_result_str(r));
    demo_query(g, cache, 1, 4);

    printf("\n[7] Road 2-4 is cleared and reopened.\n");
    r = graph_set_road_status(g, 2, 4, 1);
    printf("    reopen: %s\n", graph_result_str(r));
    demo_query(g, cache, 1, 4);

    printf("\n[8] Multiple queries from different disaster sites.\n");
    demo_query(g, cache, 2, 7);
    demo_query(g, cache, 3, 6);
    demo_query(g, cache, 7, 1);

    printf("\n[9] Unreachable center: intersection 8 has no roads.\n");
    demo_query(g, cache, 1, 8);

    printf("\n[10] Invalid queries.\n");
    demo_query(g, cache, 99, 4);
    demo_query(g, cache, 1, -5);

    printf("\n[11] Cache statistics: hits=%lu misses=%lu stale_evictions=%lu\n",
           cache->hits, cache->misses, cache->stale_evictions);

    printf("\nDemo finished; freeing all memory.\n");
    route_cache_free(cache);
    graph_free(g);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Mode: bench                                                         */
/* ------------------------------------------------------------------ */

static int run_bench(const char *csv_path)
{
    Graph *g;
    RoadList roads;
    RouteCache *cache;
    int src[NUM_QUERIES];
    int dst[NUM_QUERIES];
    double uncached[NUM_QUERIES][TIMING_PASSES];
    double cached[NUM_QUERIES][TIMING_PASSES];
    long cost[NUM_QUERIES];
    int hops[NUM_QUERIES];
    int reach[NUM_QUERIES];
    FILE *csv;
    int q;
    int pass;

    printf("Generating deterministic city (seed %lu): %d intersections, "
           "%d logical roads...\n", CITY_SEED, CITY_VERTICES, CITY_ROADS);
    g = build_city(&roads);
    if (g == NULL) {
        fprintf(stderr, "error: city generation failed (out of memory)\n");
        return 1;
    }
    cache = route_cache_create(64);
    if (cache == NULL) {
        fprintf(stderr, "error: cache creation failed\n");
        free(roads.u);
        free(roads.v);
        graph_free(g);
        return 1;
    }
    printf("Graph ready: %d vertices, %ld logical roads (%ld directed edges).\n",
           g->num_vertices, g->num_logical_roads, g->num_logical_roads * 2);
    make_queries(src, dst);

    /* -------- timed region: routing only, no generation, no I/O ---- */
    for (q = 0; q < NUM_QUERIES; q++) {
        Route *route;

        for (pass = 0; pass < TIMING_PASSES; pass++) {
            uncached[q][pass] = time_uncached_once(g, src[q], dst[q]);
        }
        route = dijkstra_shortest_route(g, src[q], dst[q], NULL);
        reach[q] = route->reachable;
        cost[q] = route->total_cost;
        hops[q] = route->num_intersections;
        route_free(route);

        evac_query(g, cache, src[q], dst[q], NULL);   /* prime the cache */
        for (pass = 0; pass < TIMING_PASSES; pass++) {
            cached[q][pass] = time_cached_once(g, cache, src[q], dst[q]);
        }
    }
    /* -------- end of timed region ---------------------------------- */

    csv = fopen(csv_path, "w");
    if (csv == NULL) {
        fprintf(stderr, "error: cannot write %s\n", csv_path);
        route_cache_free(cache);
        free(roads.u);
        free(roads.v);
        graph_free(g);
        return 1;
    }
    fprintf(csv, "record,query,src,dst,reachable,total_cost,intersections,"
                 "passes,min_ms,avg_ms,max_ms\n");
    fprintf(csv, "meta,vertices,%d,,,,,,,,\n", g->num_vertices);
    fprintf(csv, "meta,logical_roads,%ld,,,,,,,,\n", g->num_logical_roads);
    fprintf(csv, "meta,seed,%lu,,,,,,,,\n", CITY_SEED);
    fprintf(csv, "meta,uncached_reps_per_pass,%d,,,,,,,,\n",
            UNCACHED_REPS_PER_PASS);
    fprintf(csv, "meta,cached_lookups_per_pass,%d,,,,,,,,\n",
            CACHED_LOOKUPS_PER_PASS);
    for (q = 0; q < NUM_QUERIES; q++) {
        TimingSummary su;
        TimingSummary sc;

        summarize(uncached[q], TIMING_PASSES, &su);
        summarize(cached[q], TIMING_PASSES, &sc);
        fprintf(csv, "uncached,%d,%d,%d,%d,%ld,%d,%d,%.4f,%.4f,%.4f\n",
                q + 1, src[q], dst[q], reach[q], cost[q], hops[q],
                TIMING_PASSES, su.min_ms, su.avg_ms, su.max_ms);
        fprintf(csv, "cached,%d,%d,%d,%d,%ld,%d,%d,%.6f,%.6f,%.6f\n",
                q + 1, src[q], dst[q], reach[q], cost[q], hops[q],
                TIMING_PASSES, sc.min_ms, sc.avg_ms, sc.max_ms);
        printf("Query %2d: %5d -> %5d  cost=%6ld  hops=%4d  "
               "uncached avg %.3f ms  cached avg %.6f ms\n",
               q + 1, src[q], dst[q], cost[q], hops[q],
               su.avg_ms, sc.avg_ms);
    }
    fclose(csv);
    printf("Benchmark written to %s\n", csv_path);
    printf("Cache stats: hits=%lu misses=%lu stale_evictions=%lu\n",
           cache->hits, cache->misses, cache->stale_evictions);

    route_cache_free(cache);
    free(roads.u);
    free(roads.v);
    graph_free(g);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Mode: fail (30% of logical roads become unavailable)                */
/* ------------------------------------------------------------------ */

static int run_fail(const char *txt_path)
{
    Graph *g;
    RoadList roads;
    RouteCache *cache;
    int src[NUM_QUERIES];
    int dst[NUM_QUERIES];
    long cost_before[NUM_QUERIES];
    long cost_after[NUM_QUERIES];
    int hops_before[NUM_QUERIES];
    int hops_after[NUM_QUERIES];
    int reach_before[NUM_QUERIES];
    int reach_after[NUM_QUERIES];
    double t_before[NUM_QUERIES];
    double t_after[NUM_QUERIES];
    long *order;
    long roads_to_block;
    long reachable_before;
    long reachable_after;
    unsigned long stale_before_rerun;
    int reachable_queries_before = 0;
    int reachable_queries_after = 0;
    FILE *out;
    long i;
    int q;

    printf("Building the same deterministic city as the benchmark...\n");
    g = build_city(&roads);
    if (g == NULL) {
        fprintf(stderr, "error: city generation failed (out of memory)\n");
        return 1;
    }
    cache = route_cache_create(64);
    order = (long *)malloc((size_t)roads.count * sizeof(long));
    if (cache == NULL || order == NULL) {
        fprintf(stderr, "error: out of memory\n");
        free(order);
        route_cache_free(cache);
        free(roads.u);
        free(roads.v);
        graph_free(g);
        return 1;
    }
    make_queries(src, dst);

    /* ---- baseline: timings, routes, connectivity, primed cache ---- */
    for (q = 0; q < NUM_QUERIES; q++) {
        Route *route;

        t_before[q] = time_uncached_once(g, src[q], dst[q]);
        route = dijkstra_shortest_route(g, src[q], dst[q], NULL);
        reach_before[q] = route->reachable;
        cost_before[q] = route->total_cost;
        hops_before[q] = route->num_intersections;
        if (route->reachable) {
            reachable_queries_before++;
        }
        route_free(route);
        evac_query(g, cache, src[q], dst[q], NULL);   /* fill the cache */
    }
    reachable_before = dijkstra_count_reachable(g, src[0]);

    /* ---- deterministically block exactly 30% of logical roads ----- */
    roads_to_block = (roads.count * 30L) / 100L;
    for (i = 0; i < roads.count; i++) {
        order[i] = i;
    }
    rng_seed(CITY_SEED ^ 0xC0FFEEUL);
    for (i = roads.count - 1; i > 0; i--) {   /* Fisher-Yates shuffle */
        long j = (long)(rng_next() % (unsigned long)(i + 1));
        long tmp = order[i];
        order[i] = order[j];
        order[j] = tmp;
    }
    for (i = 0; i < roads_to_block; i++) {
        graph_set_road_status(g, roads.u[order[i]], roads.v[order[i]], 0);
    }
    printf("Blocked %ld of %ld logical roads (30%%).\n",
           roads_to_block, roads.count);

    /* ---- rerun the same queries on the damaged network ------------ */
    stale_before_rerun = cache->stale_evictions;
    for (q = 0; q < NUM_QUERIES; q++) {
        Route *route;

        t_after[q] = time_uncached_once(g, src[q], dst[q]);
        route = dijkstra_shortest_route(g, src[q], dst[q], NULL);
        reach_after[q] = route->reachable;
        cost_after[q] = route->total_cost;
        hops_after[q] = route->num_intersections;
        if (route->reachable) {
            reachable_queries_after++;
        }
        route_free(route);
        evac_query(g, cache, src[q], dst[q], NULL);   /* stale -> evict */
    }
    reachable_after = dijkstra_count_reachable(g, src[0]);

    /* ---- write the analysis (outside all timed regions) ----------- */
    out = fopen(txt_path, "w");
    if (out == NULL) {
        fprintf(stderr, "error: cannot write %s\n", txt_path);
        free(order);
        route_cache_free(cache);
        free(roads.u);
        free(roads.v);
        graph_free(g);
        return 1;
    }
    fprintf(out, "30%% ROAD FAILURE EXPERIMENT\n");
    fprintf(out, "===========================\n\n");
    fprintf(out, "Network: %d intersections, %ld logical bidirectional roads\n",
            g->num_vertices, g->num_logical_roads);
    fprintf(out, "Deterministic generation seed: %lu\n", CITY_SEED);
    fprintf(out, "Roads blocked: %ld of %ld (exactly 30%%), selected by a\n",
            roads_to_block, roads.count);
    fprintf(out, "Fisher-Yates shuffle seeded with %lu ^ 0xC0FFEE.\n\n",
            CITY_SEED);
    fprintf(out, "Timing: standard C clock(), avg of %d Dijkstra runs per "
                 "query,\nresolution about 1 ms; graph generation and file "
                 "writing excluded.\n\n", UNCACHED_REPS_PER_PASS);

    fprintf(out, "PER-QUERY RESULTS (before -> after blocking)\n");
    fprintf(out, "%-3s %-6s %-6s | %-11s %-11s | %-9s %-9s | %-6s %-6s | %-9s %-9s\n",
            "Q", "src", "dst", "reach_pre", "reach_post", "cost_pre",
            "cost_post", "hops0", "hops1", "ms_pre", "ms_post");
    for (q = 0; q < NUM_QUERIES; q++) {
        fprintf(out, "%-3d %-6d %-6d | %-11s %-11s | %-9ld %-9ld | %-6d %-6d | %-9.3f %-9.3f\n",
                q + 1, src[q], dst[q],
                reach_before[q] ? "yes" : "NO",
                reach_after[q] ? "yes" : "NO",
                cost_before[q], cost_after[q],
                hops_before[q], hops_after[q],
                t_before[q], t_after[q]);
    }

    fprintf(out, "\nSUMMARY\n");
    fprintf(out, "Reachable destination queries: %d/%d before, %d/%d after.\n",
            reachable_queries_before, NUM_QUERIES,
            reachable_queries_after, NUM_QUERIES);
    fprintf(out, "Intersections reachable from intersection %d: %ld of %d "
                 "before,\n%ld of %d after blocking.\n",
            src[0], reachable_before, g->num_vertices,
            reachable_after, g->num_vertices);
    for (q = 0; q < NUM_QUERIES; q++) {
        if (reach_before[q] && reach_after[q]) {
            fprintf(out, "Query %d: cost %ld -> %ld (%+ld), route length "
                         "%d -> %d intersections.\n",
                    q + 1, cost_before[q], cost_after[q],
                    cost_after[q] - cost_before[q],
                    hops_before[q], hops_after[q]);
        } else if (reach_before[q] && !reach_after[q]) {
            fprintf(out, "Query %d: destination became UNREACHABLE after "
                         "the failures.\n", q + 1);
        }
    }
    fprintf(out, "\nCACHE BEHAVIOUR\n");
    fprintf(out, "Every route cached before the failures was invalidated: "
                 "blocking\nroads bumped the graph version, so the rerun "
                 "evicted %lu stale\nentries and recomputed (hits=%lu, "
                 "misses=%lu, stale_evictions=%lu).\n",
            cache->stale_evictions - stale_before_rerun,
            cache->hits, cache->misses, cache->stale_evictions);

    fclose(out);
    printf("Analysis written to %s\n", txt_path);

    free(order);
    route_cache_free(cache);
    free(roads.u);
    free(roads.v);
    graph_free(g);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Interactive dashboard (no-argument mode)                            */
/*                                                                     */
/* A presentation layer over the existing engine: it builds a small    */
/* demonstration city through the normal graph API and lets an         */
/* evaluator run queries, updates, blockings and the automated modes   */
/* from a menu. All routing behaviour is the engine's own.             */
/* ------------------------------------------------------------------ */

/* Same city as the guided demo: intersections 1..8 (8 isolated),
 * roads 1-2 w4, 1-3 w1, 3-2 w2, 2-4 w5, 3-4 w12, 4-5 w3, 5-6 w2,
 * 6-7 w1. */
static Graph *build_planner_city(void)
{
    Graph *g = graph_create();
    int i;

    if (g == NULL) {
        return NULL;
    }
    for (i = 1; i <= 8; i++) {
        graph_add_intersection(g, i);
    }
    graph_add_road(g, 1, 2, 4);
    graph_add_road(g, 1, 3, 1);
    graph_add_road(g, 3, 2, 2);
    graph_add_road(g, 2, 4, 5);
    graph_add_road(g, 3, 4, 12);
    graph_add_road(g, 4, 5, 3);
    graph_add_road(g, 5, 6, 2);
    graph_add_road(g, 6, 7, 1);
    return g;
}

/* Re-prompts while the input is not a valid number.
 * Returns 0 on success, -2 when input ended (cancel). */
static int read_int_retry(const char *prompt, int *out)
{
    for (;;) {
        int rc = ui_read_int(prompt, out);
        if (rc == 0) {
            return 0;
        }
        if (rc == -2) {
            return -2;
        }
        ui_styled_line(UI_ERROR,
                       "  That is not a valid whole number - try again.");
    }
}

static int read_long_retry(const char *prompt, long *out)
{
    for (;;) {
        int rc = ui_read_long(prompt, out);
        if (rc == 0) {
            return 0;
        }
        if (rc == -2) {
            return -2;
        }
        ui_styled_line(UI_ERROR,
                       "  That is not a valid whole number - try again.");
    }
}

static void planner_show_city(const Graph *g)
{
    int i;
    char buf[128];

    ui_rule('-');
    ui_box_center(UI_TITLE, "DEMONSTRATION CITY MAP");
    ui_rule('-');
    sprintf(buf, "%d", g->num_vertices);
    ui_kv("Intersections", buf);
    sprintf(buf, "%ld (bidirectional)", g->num_logical_roads);
    ui_kv("Logical roads", buf);
    sprintf(buf, "%lu", g->version);
    ui_kv("Graph version", buf);
    printf("\n  %-14s %-10s %s\n", "ROAD", "WEIGHT", "STATUS");
    for (i = 0; i < g->num_vertices; i++) {
        const Vertex *u = g->by_index[i];
        const Edge *e;
        for (e = u->edges; e != NULL; e = e->next) {
            if (u->id < e->dest_id) {   /* print each logical road once */
                printf("  %4d -- %-6d %-10ld ", u->id, e->dest_id,
                       e->weight);
                if (e->active) {
                    ui_styled_line(UI_SUCCESS, "OPEN");
                } else {
                    ui_styled_line(UI_ERROR, "BLOCKED");
                }
            }
        }
    }
    for (i = 0; i < g->num_vertices; i++) {
        if (g->by_index[i]->degree == 0) {
            sprintf(buf, "  Intersection %d has no roads (isolated).",
                    g->by_index[i]->id);
            ui_styled_line(UI_WARN, buf);
        }
    }
}

static void planner_find_route(Graph *g, RouteCache *cache)
{
    int src;
    int dst;
    int from_cache = 0;
    const Route *route;
    char buf[192];
    char path[512];
    int i;

    if (read_int_retry("  Source intersection ID", &src) != 0
            || read_int_retry("  Destination intersection ID", &dst) != 0) {
        return;
    }
    if (graph_get_vertex(g, src) == NULL) {
        sprintf(buf, "  Intersection %d does not exist in this city.", src);
        ui_styled_line(UI_ERROR, buf);
        return;
    }
    if (graph_get_vertex(g, dst) == NULL) {
        sprintf(buf, "  Intersection %d does not exist in this city.", dst);
        ui_styled_line(UI_ERROR, buf);
        return;
    }
    route = evac_query(g, cache, src, dst, &from_cache);
    if (route == NULL) {
        ui_styled_line(UI_ERROR, "  Query failed unexpectedly.");
        return;
    }
    printf("\n");
    ui_rule('-');
    if (!route->reachable) {
        ui_box_center(UI_ERROR, "DESTINATION UNREACHABLE");
        ui_rule('-');
        sprintf(buf, "%d", src);
        ui_kv("Source", buf);
        sprintf(buf, "%d", dst);
        ui_kv("Destination", buf);
        sprintf(buf,
                "  No open road chain connects %d to %d right now.",
                src, dst);
        ui_styled_line(UI_ERROR, buf);
        ui_kv("Answered from", from_cache ? "route cache (HIT)"
                                          : "Dijkstra run (cache MISS)");
        return;
    }
    ui_box_center(UI_SUCCESS, "EVACUATION ROUTE FOUND");
    ui_rule('-');
    sprintf(buf, "%d", src);
    ui_kv("Source", buf);
    sprintf(buf, "%d", dst);
    ui_kv("Destination", buf);
    path[0] = '\0';
    for (i = 0; i < route->num_intersections; i++) {
        char piece[32];
        sprintf(piece, i == 0 ? "%d" : " -> %d",
                route->intersection_ids[i]);
        if (strlen(path) + strlen(piece) + 1 < sizeof(path)) {
            strcat(path, piece);
        }
    }
    ui_kv("Route", path);
    sprintf(buf, "%ld", route->total_cost);
    ui_kv("Total travel cost", buf);
    sprintf(buf, "%d", route->num_intersections);
    ui_kv("Intersections", buf);
    ui_kv("Answered from", from_cache ? "route cache (HIT)"
                                      : "Dijkstra run (cache MISS)");
}

/* action: 0 = update weight, 1 = block, 2 = reopen. */
static void planner_edit_road(Graph *g, int action)
{
    int u;
    int v;
    long w = 0;
    GraphResult rc;
    char buf[160];

    if (read_int_retry("  First intersection ID", &u) != 0
            || read_int_retry("  Second intersection ID", &v) != 0) {
        return;
    }
    if (action == 0) {
        if (read_long_retry("  New travel cost (>= 0)", &w) != 0) {
            return;
        }
        rc = graph_update_road_weight(g, u, v, w);
    } else {
        rc = graph_set_road_status(g, u, v, action == 2 ? 1 : 0);
    }
    if (rc != GRAPH_OK) {
        sprintf(buf, "  Operation rejected: %s.", graph_result_str(rc));
        ui_styled_line(UI_ERROR, buf);
        return;
    }
    if (action == 0) {
        sprintf(buf, "  Road %d-%d now has travel cost %ld.", u, v, w);
    } else if (action == 1) {
        sprintf(buf, "  Road %d-%d is now BLOCKED in both directions.",
                u, v);
    } else {
        sprintf(buf, "  Road %d-%d is OPEN again with its stored cost.",
                u, v);
    }
    ui_styled_line(UI_SUCCESS, buf);
    sprintf(buf, "  Cached routes are now stale (graph version %lu).",
            g->version);
    ui_styled_line(UI_WARN, buf);
}

static void planner_cache_stats(const RouteCache *cache, const Graph *g)
{
    char buf[64];

    ui_rule('-');
    ui_box_center(UI_TITLE, "ROUTE CACHE STATISTICS");
    ui_rule('-');
    sprintf(buf, "%lu", cache->hits);
    ui_kv("Cache hits", buf);
    sprintf(buf, "%lu", cache->misses);
    ui_kv("Cache misses", buf);
    sprintf(buf, "%lu", cache->stale_evictions);
    ui_kv("Stale evictions", buf);
    sprintf(buf, "%d", cache->size);
    ui_kv("Routes stored", buf);
    sprintf(buf, "%lu", g->version);
    ui_kv("Graph version", buf);
}

static int run_planner(void)
{
    Graph *g = build_planner_city();
    RouteCache *cache = route_cache_create(16);
    int running = 1;

    if (g == NULL || cache == NULL) {
        graph_free(g);
        route_cache_free(cache);
        ui_styled_line(UI_ERROR, "  Out of memory.");
        return 1;
    }
    while (running) {
        int choice;
        int rc;

        ui_clear();
        ui_rule('=');
        ui_box_center(UI_TITLE, "INTERACTIVE EVACUATION ROUTE PLANNER");
        ui_rule('=');
        printf("  [1] View intersections and roads\n");
        printf("  [2] Find an evacuation route\n");
        printf("  [3] Update a road weight\n");
        printf("  [4] Block a road\n");
        printf("  [5] Reopen a road\n");
        printf("  [6] View route-cache statistics\n");
        printf("  [7] Reset the demonstration city\n");
        printf("  [0] Back to main menu\n");
        ui_rule('-');
        rc = ui_read_int("  Choose an option", &choice);
        if (rc == -2) {
            break;
        }
        if (rc != 0) {
            ui_styled_line(UI_ERROR,
                           "  Please enter one of the menu numbers.");
            ui_pause();
            continue;
        }
        printf("\n");
        switch (choice) {
        case 1:
            planner_show_city(g);
            break;
        case 2:
            planner_find_route(g, cache);
            break;
        case 3:
            planner_edit_road(g, 0);
            break;
        case 4:
            planner_edit_road(g, 1);
            break;
        case 5:
            planner_edit_road(g, 2);
            break;
        case 6:
            planner_cache_stats(cache, g);
            break;
        case 7:
            graph_free(g);
            route_cache_free(cache);
            g = build_planner_city();
            cache = route_cache_create(16);
            if (g == NULL || cache == NULL) {
                ui_styled_line(UI_ERROR, "  Out of memory.");
                running = 0;
                break;
            }
            ui_styled_line(UI_SUCCESS,
                "  Demonstration city rebuilt with its original roads.");
            break;
        case 0:
            running = 0;
            break;
        default:
            ui_styled_line(UI_ERROR,
                           "  Please enter one of the menu numbers.");
            break;
        }
        if (running) {
            printf("\n");
            ui_pause();
        }
    }
    graph_free(g);
    route_cache_free(cache);
    return 0;
}

static void show_architecture(void)
{
    ui_rule('-');
    ui_box_center(UI_TITLE, "SYSTEM ARCHITECTURE AND FEATURES");
    ui_rule('-');
    printf("  %-16s %s\n", "MODULE", "ROLE");
    printf("  %-16s %s\n", "graph.c",
           "HashMap-based adjacency list, dynamic updates, versioning");
    printf("  %-16s %s\n", "hashmap.c",
           "intersection ID -> vertex lookup, O(1) average");
    printf("  %-16s %s\n", "hashset.c",
           "processed-intersection set inside Dijkstra");
    printf("  %-16s %s\n", "minheap.c",
           "insert / extract-min / heapify / decrease-key, O(log V)");
    printf("  %-16s %s\n", "dijkstra.c",
           "O((V+E) log V) shortest routes + route reconstruction");
    printf("  %-16s %s\n", "route_cache.c",
           "version-stamped cache for repeated queries");
    printf("  %-16s %s\n", "console_ui.c",
           "this dashboard (presentation only)");
    printf("  %-16s %s\n", "main.c",
           "program flow: interactive / demo / bench / fail");
    ui_rule('-');
    printf("  Key properties:\n");
    printf("  - roads are bidirectional with non-negative,"
           " safety-adjusted costs\n");
    printf("  - blocked roads are skipped, reopening restores the"
           " stored cost\n");
    printf("  - every mutation bumps the graph version and invalidates"
           " the cache\n");
    printf("  - unreachable evacuation centers are reported, never"
           " looped on\n");
    printf("  - benchmark city: 8,200 intersections / 21,000 logical"
           " roads (seed %lu)\n", (unsigned long)CITY_SEED);
    ui_styled_line(UI_WARN,
        "  Console-based C data-structures prototype on synthetic data"
        " - not a\n  deployed navigation application.");
}

static int run_interactive(void)
{
    int running = 1;

    while (running) {
        int choice;
        int rc;

        ui_clear();
        ui_banner();
        printf("\n");
        printf("  [1] Interactive Evacuation Route Planner\n");
        printf("  [2] Run Guided System Demonstration\n");
        printf("  [3] Run Full-Scale Performance Benchmark\n");
        printf("  [4] Simulate 30%% Road Failure\n");
        printf("  [5] View System Architecture and Features\n");
        printf("  [0] Exit\n");
        ui_rule('-');
        rc = ui_read_int("  Choose an option", &choice);
        if (rc == -2) {
            break;
        }
        if (rc != 0) {
            ui_styled_line(UI_ERROR,
                           "  Please enter one of the menu numbers.");
            ui_pause();
            continue;
        }
        printf("\n");
        switch (choice) {
        case 1:
            run_planner();
            break;
        case 2:
            ui_clear();
            run_demo();
            printf("\n");
            ui_styled_line(UI_SUCCESS, "  Guided demonstration finished.");
            ui_pause();
            break;
        case 3:
            ui_styled_line(UI_WARN,
                "  Building the 8,200-intersection / 21,000-road city"
                " and timing");
            ui_styled_line(UI_WARN,
                "  10 queries (uncached and cached). This takes a few"
                " seconds...");
            printf("\n");
            if (run_bench("results/benchmark_results.csv") == 0) {
                ui_styled_line(UI_SUCCESS,
                    "  Benchmark finished - see"
                    " results/benchmark_results.csv.");
            } else {
                ui_styled_line(UI_ERROR, "  Benchmark failed.");
            }
            ui_pause();
            break;
        case 4:
            ui_styled_line(UI_WARN,
                "  Simulating a disaster that blocks 30% of all"
                " logical roads...");
            printf("\n");
            if (run_fail("results/road_failure_analysis.txt") == 0) {
                ui_styled_line(UI_SUCCESS,
                    "  Simulation finished - see"
                    " results/road_failure_analysis.txt.");
            } else {
                ui_styled_line(UI_ERROR, "  Simulation failed.");
            }
            ui_pause();
            break;
        case 5:
            show_architecture();
            ui_pause();
            break;
        case 0:
            running = 0;
            break;
        default:
            ui_styled_line(UI_ERROR,
                           "  Please enter one of the menu numbers.");
            ui_pause();
            break;
        }
    }
    ui_styled_line(UI_SUCCESS,
        "  All memory released. Stay safe - goodbye!");
    return 0;
}

/* ------------------------------------------------------------------ */

int main(int argc, char **argv)
{
    ui_init();
    if (argc < 2) {
        return run_interactive();
    }
    if (strcmp(argv[1], "demo") == 0) {
        return run_demo();
    }
    if (strcmp(argv[1], "bench") == 0) {
        return run_bench(argc >= 3 ? argv[2]
                                   : "results/benchmark_results.csv");
    }
    if (strcmp(argv[1], "fail") == 0) {
        return run_fail(argc >= 3 ? argv[2]
                                  : "results/road_failure_analysis.txt");
    }
    fprintf(stderr,
            "usage: %s [demo | bench [csv_path] | fail [txt_path]]\n"
            "       (no argument starts the interactive dashboard)\n",
            argv[0]);
    return 2;
}
