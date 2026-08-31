/*
 * test_runner.c
 * Functional tests for the Intelligent Emergency Evacuation Routing
 * System. Every test prints expected result, actual result and
 * PASS/FAIL. Usage:
 *     test_runner [output_path]
 * With no argument the report goes to stdout.
 *
 * Known test graph (bidirectional roads, weight = travel cost):
 *
 *      1 --4-- 2 --5-- 4 --3-- 5
 *      |      /       /
 *      1    2       12
 *      |  /        /
 *      3 ---------
 *      (6 is an isolated intersection)
 *
 * Roads: 1-2 w4, 1-3 w1, 3-2 w2, 2-4 w5, 3-4 w12, 4-5 w3.
 * Hand-checked shortest route 1 -> 4:
 *   1-3-2-4 = 1+2+5 = 8   (vs 1-2-4 = 9, 1-3-4 = 13)  -> cost 8.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/graph.h"
#include "../src/dijkstra.h"
#include "../src/route_cache.h"

static FILE *out;
static int tests_run = 0;
static int tests_passed = 0;

static void record(const char *name, const char *expected,
                   const char *actual, int pass)
{
    tests_run++;
    if (pass) {
        tests_passed++;
    }
    fprintf(out, "Test %02d: %-42s\n", tests_run, name);
    fprintf(out, "   expected: %s\n", expected);
    fprintf(out, "   actual:   %s\n", actual);
    fprintf(out, "   result:   %s\n\n", pass ? "PASS" : "FAIL");
}

/* Formats a route as "cost C via a -> b -> c" into buf. */
static void format_route(const Route *route, char *buf)
{
    int i;
    char piece[32];

    if (route == NULL) {
        strcpy(buf, "NULL (invalid query)");
        return;
    }
    if (!route->reachable) {
        strcpy(buf, "unreachable");
        return;
    }
    sprintf(buf, "cost %ld via ", route->total_cost);
    for (i = 0; i < route->num_intersections; i++) {
        sprintf(piece, i == 0 ? "%d" : " -> %d", route->intersection_ids[i]);
        strcat(buf, piece);
    }
}

/* Compares a route against an expected cost and vertex sequence. */
static int route_matches(const Route *route, long cost,
                         const int *ids, int n)
{
    int i;

    if (route == NULL || !route->reachable || route->total_cost != cost
            || route->num_intersections != n) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        if (route->intersection_ids[i] != ids[i]) {
            return 0;
        }
    }
    return 1;
}

static Graph *build_known_graph(void)
{
    Graph *g = graph_create();
    int i;

    for (i = 1; i <= 6; i++) {
        graph_add_intersection(g, i);
    }
    graph_add_road(g, 1, 2, 4);
    graph_add_road(g, 1, 3, 1);
    graph_add_road(g, 3, 2, 2);
    graph_add_road(g, 2, 4, 5);
    graph_add_road(g, 3, 4, 12);
    graph_add_road(g, 4, 5, 3);
    /* intersection 6 is left isolated on purpose */
    return g;
}

int main(int argc, char **argv)
{
    Graph *g;
    RouteCache *cache;
    Route *route;
    const Route *cached;
    char actual[512];
    char expected[512];
    int from_cache;
    GraphResult gr;

    out = stdout;
    if (argc >= 2) {
        out = fopen(argv[1], "w");
        if (out == NULL) {
            fprintf(stderr, "error: cannot write %s\n", argv[1]);
            return 2;
        }
    }
    fprintf(out, "CSA0312 - Intelligent Emergency Evacuation Routing System\n");
    fprintf(out, "Functional test results\n");
    fprintf(out, "=========================================================\n");
    fprintf(out, "Known graph: intersections 1..6 (6 isolated); roads\n");
    fprintf(out, "1-2 w4, 1-3 w1, 3-2 w2, 2-4 w5, 3-4 w12, 4-5 w3.\n\n");

    g = build_known_graph();
    cache = route_cache_create(16);

    /* 1. Known graph with a manually verifiable shortest route -------- */
    route = dijkstra_shortest_route(g, 1, 4, NULL);
    format_route(route, actual);
    record("known shortest route 1 -> 4",
           "cost 8 via 1 -> 3 -> 2 -> 4", actual,
           route_matches(route, 8, (const int[]){1, 3, 2, 4}, 4));
    route_free(route);

    /* 2. Route reconstruction ---------------------------------------- */
    route = dijkstra_shortest_route(g, 1, 5, NULL);
    format_route(route, actual);
    record("route reconstruction 1 -> 5",
           "cost 11 via 1 -> 3 -> 2 -> 4 -> 5", actual,
           route_matches(route, 11, (const int[]){1, 3, 2, 4, 5}, 5));
    route_free(route);

    /* 3. Weight update changes the best route ------------------------ */
    gr = graph_update_road_weight(g, 3, 2, 10);
    route = dijkstra_shortest_route(g, 1, 4, NULL);
    format_route(route, actual);
    sprintf(expected, "update OK; new best cost 9 via 1 -> 2 -> 4");
    record("weight update 3-2: 2 -> 10 reroutes 1 -> 4", expected, actual,
           gr == GRAPH_OK
           && route_matches(route, 9, (const int[]){1, 2, 4}, 3));
    route_free(route);

    /* 4. Blocking a road forces an alternative route ----------------- */
    gr = graph_set_road_status(g, 2, 4, 0);
    route = dijkstra_shortest_route(g, 1, 4, NULL);
    format_route(route, actual);
    record("blocking road 2-4 reroutes 1 -> 4",
           "block OK; new best cost 13 via 1 -> 3 -> 4", actual,
           gr == GRAPH_OK
           && route_matches(route, 13, (const int[]){1, 3, 4}, 3));
    route_free(route);

    /* 5. Reopening the road restores the previous route -------------- */
    gr = graph_set_road_status(g, 2, 4, 1);
    route = dijkstra_shortest_route(g, 1, 4, NULL);
    format_route(route, actual);
    record("reopening road 2-4 restores the route",
           "reopen OK; best cost 9 via 1 -> 2 -> 4 again", actual,
           gr == GRAPH_OK
           && route_matches(route, 9, (const int[]){1, 2, 4}, 3));
    route_free(route);

    /* 6. Invalid source ---------------------------------------------- */
    route = dijkstra_shortest_route(g, 99, 4, NULL);
    record("invalid source intersection 99",
           "query rejected (NULL route)",
           route == NULL ? "NULL (invalid query)" : "a route was returned",
           route == NULL);
    route_free(route);

    /* 7. Invalid destination ----------------------------------------- */
    route = dijkstra_shortest_route(g, 1, 77, NULL);
    record("invalid destination intersection 77",
           "query rejected (NULL route)",
           route == NULL ? "NULL (invalid query)" : "a route was returned",
           route == NULL);
    route_free(route);

    /* 8. Duplicate road ---------------------------------------------- */
    gr = graph_add_road(g, 1, 2, 7);
    record("duplicate road 1-2 rejected",
           "duplicate road", graph_result_str(gr),
           gr == GRAPH_ERR_DUPLICATE_ROAD);

    /* 9. Negative weight rejection ----------------------------------- */
    gr = graph_add_road(g, 5, 6, -5);
    sprintf(actual, "add: %s", graph_result_str(gr));
    {
        GraphResult gr2 = graph_update_road_weight(g, 1, 2, -1);
        char buf[64];
        sprintf(buf, "; update: %s", graph_result_str(gr2));
        strcat(actual, buf);
        record("negative weights rejected (add and update)",
               "add: negative weight rejected; update: negative weight rejected",
               actual,
               gr == GRAPH_ERR_NEGATIVE_WEIGHT
               && gr2 == GRAPH_ERR_NEGATIVE_WEIGHT);
    }

    /* 10. Unreachable destination ------------------------------------ */
    route = dijkstra_shortest_route(g, 1, 6, NULL);
    format_route(route, actual);
    record("unreachable destination 6 (isolated)",
           "unreachable", actual,
           route != NULL && !route->reachable);
    route_free(route);

    /* 11. Multiple source-to-destination queries --------------------- */
    {
        Route *r1 = dijkstra_shortest_route(g, 1, 4, NULL);
        Route *r2 = dijkstra_shortest_route(g, 1, 5, NULL);
        Route *r3 = dijkstra_shortest_route(g, 2, 5, NULL);
        int ok = route_matches(r1, 9, (const int[]){1, 2, 4}, 3)
                 && r2 != NULL && r2->reachable && r2->total_cost == 12
                 && r3 != NULL && r3->reachable && r3->total_cost == 8;
        sprintf(actual, "1->4 cost %ld, 1->5 cost %ld, 2->5 cost %ld",
                r1 == NULL ? -99 : r1->total_cost,
                r2 == NULL ? -99 : r2->total_cost,
                r3 == NULL ? -99 : r3->total_cost);
        record("multiple queries 1->4, 1->5, 2->5",
               "costs 9, 12 and 8", actual, ok);
        route_free(r1);
        route_free(r2);
        route_free(r3);
    }

    /* 12. Cache miss on first query ---------------------------------- */
    cached = evac_query(g, cache, 1, 4, &from_cache);
    sprintf(actual, "from_cache=%d, %s", from_cache,
            cached != NULL && cached->reachable ? "route returned"
                                                : "no route");
    record("first cached query 1 -> 4 is a MISS",
           "from_cache=0, route returned", actual,
           cached != NULL && from_cache == 0 && cached->reachable
           && cached->total_cost == 9);

    /* 13. Cache hit on repeated query -------------------------------- */
    cached = evac_query(g, cache, 1, 4, &from_cache);
    sprintf(actual, "from_cache=%d, cost %ld", from_cache,
            cached == NULL ? -99 : cached->total_cost);
    record("repeated query 1 -> 4 is a HIT",
           "from_cache=1, cost 9", actual,
           cached != NULL && from_cache == 1 && cached->total_cost == 9);

    /* 14. Cache invalidation after an update ------------------------- */
    {
        unsigned long stale0 = cache->stale_evictions;
        graph_update_road_weight(g, 2, 4, 6);   /* 1->4 now 4+6 = 10 */
        cached = evac_query(g, cache, 1, 4, &from_cache);
        sprintf(actual, "from_cache=%d, cost %ld, stale_evictions +%lu",
                from_cache, cached == NULL ? -99 : cached->total_cost,
                cache->stale_evictions - stale0);
        record("cache invalidated after weight update 2-4: 5 -> 6",
               "from_cache=0, cost 10, stale_evictions +1", actual,
               cached != NULL && from_cache == 0
               && cached->total_cost == 10
               && cache->stale_evictions == stale0 + 1);
    }

    /* 15. Cyclic graph processed without repetition ------------------ */
    {
        Graph *cyc = graph_create();
        DijkstraStats stats;
        int i;

        for (i = 1; i <= 4; i++) {
            graph_add_intersection(cyc, i);
        }
        /* Cycle 1-2-3-1 plus a spur 3-4: bidirectional roads already
         * create cycles; this adds an explicit triangle. */
        graph_add_road(cyc, 1, 2, 1);
        graph_add_road(cyc, 2, 3, 1);
        graph_add_road(cyc, 3, 1, 1);
        graph_add_road(cyc, 3, 4, 1);
        route = dijkstra_shortest_route(cyc, 1, 4, &stats);
        sprintf(actual, "cost %ld, vertices processed %d (of 4)",
                route == NULL ? -99 : route->total_cost,
                stats.vertices_processed);
        record("cyclic graph: each intersection processed at most once",
               "cost 2, vertices processed <= 4", actual,
               route != NULL && route->reachable && route->total_cost == 2
               && stats.vertices_processed <= 4);
        route_free(route);
        graph_free(cyc);
    }

    /* 16. Isolated vertex -------------------------------------------- */
    {
        long reachable = dijkstra_count_reachable(g, 6);
        route = dijkstra_shortest_route(g, 6, 1, NULL);
        format_route(route, actual);
        {
            char buf[64];
            sprintf(buf, "; %ld intersection(s) reachable from 6", reachable);
            strcat(actual, buf);
        }
        record("isolated intersection 6 as a source",
               "unreachable; 1 intersection(s) reachable from 6", actual,
               route != NULL && !route->reachable && reachable == 1);
        route_free(route);
    }

    fprintf(out, "=========================================================\n");
    fprintf(out, "TOTAL: %d/%d tests passed\n", tests_passed, tests_run);

    route_cache_free(cache);
    graph_free(g);
    if (out != stdout) {
        fclose(out);
    }
    printf("test_runner: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
