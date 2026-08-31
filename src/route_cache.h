/*
 * route_cache.h
 * Hash-table cache of computed routes for repeated source-to-destination
 * queries. Every entry remembers the graph version it was computed
 * against; when a road weight or status changes the graph version
 * increases, so stale entries are detected on lookup and evicted.
 */
#ifndef ROUTE_CACHE_H
#define ROUTE_CACHE_H

#include "graph.h"
#include "dijkstra.h"

typedef struct RouteCacheEntry {
    int src_id;
    int dst_id;
    unsigned long graph_version;   /* version the route was computed at */
    Route *route;                  /* owned deep copy */
    struct RouteCacheEntry *next;
} RouteCacheEntry;

typedef struct RouteCache {
    RouteCacheEntry **buckets;
    int num_buckets;               /* power of two */
    int size;
    /* statistics, useful for tests and benchmarks */
    unsigned long hits;
    unsigned long misses;
    unsigned long stale_evictions; /* entries dropped as out of date */
} RouteCache;

/* Creates a cache. Returns NULL on allocation failure. */
RouteCache *route_cache_create(int initial_buckets);

/* Returns the cached route for (src, dst) if present AND computed at the
 * graph's current version; otherwise returns NULL. A stale entry is
 * evicted, counted in stale_evictions, and reported as a miss. */
const Route *route_cache_lookup(RouteCache *cache, const Graph *g,
                                int src_id, int dst_id);

/* Stores a deep copy of the route, stamped with the graph's current
 * version. Returns 0 on success, -1 on allocation failure. */
int route_cache_store(RouteCache *cache, const Graph *g, const Route *route);

/* Removes every entry but keeps the cache usable. */
void route_cache_clear(RouteCache *cache);

void route_cache_free(RouteCache *cache);

/*
 * High-level query used by the demo, tests and benchmark:
 * answer from the cache when possible, otherwise run Dijkstra and cache
 * the result (including "unreachable" results, so repeated queries for
 * unreachable centers are also fast).
 * Returns a route owned by the cache (do NOT free it), or NULL when
 * src/dst is not a known intersection. If from_cache is non-NULL it is
 * set to 1 on a cache hit and 0 on a miss.
 */
const Route *evac_query(Graph *g, RouteCache *cache, int src_id, int dst_id,
                        int *from_cache);

#endif /* ROUTE_CACHE_H */
