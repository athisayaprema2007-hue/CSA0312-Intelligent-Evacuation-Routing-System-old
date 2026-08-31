/*
 * route_cache.c
 * Version-stamped route cache with separate chaining, plus the
 * cache-aware evac_query() entry point.
 */
#include <stdlib.h>
#include "route_cache.h"

#define CACHE_MIN_BUCKETS 16

/* Mixes the (src, dst) pair into one hash value. */
static unsigned long cache_hash(int src_id, int dst_id)
{
    unsigned long h = (unsigned long)((unsigned int)src_id * 2654435761u);

    h ^= (unsigned long)((unsigned int)dst_id * 40503u) + (h << 6) + (h >> 2);
    return h;
}

static int round_up_pow2(int n)
{
    int p = CACHE_MIN_BUCKETS;
    while (p < n && p < (1 << 30)) {
        p <<= 1;
    }
    return p;
}

RouteCache *route_cache_create(int initial_buckets)
{
    RouteCache *cache = (RouteCache *)malloc(sizeof(RouteCache));

    if (cache == NULL) {
        return NULL;
    }
    cache->num_buckets = round_up_pow2(initial_buckets);
    cache->buckets = (RouteCacheEntry **)calloc((size_t)cache->num_buckets,
                                                sizeof(RouteCacheEntry *));
    if (cache->buckets == NULL) {
        free(cache);
        return NULL;
    }
    cache->size = 0;
    cache->hits = 0;
    cache->misses = 0;
    cache->stale_evictions = 0;
    return cache;
}

/* Finds an entry without touching statistics or version checks. */
static RouteCacheEntry *cache_find_raw(const RouteCache *cache,
                                       int src_id, int dst_id)
{
    unsigned long idx = cache_hash(src_id, dst_id)
                        & (unsigned long)(cache->num_buckets - 1);
    RouteCacheEntry *entry;

    for (entry = cache->buckets[idx]; entry != NULL; entry = entry->next) {
        if (entry->src_id == src_id && entry->dst_id == dst_id) {
            return entry;
        }
    }
    return NULL;
}

const Route *route_cache_lookup(RouteCache *cache, const Graph *g,
                                int src_id, int dst_id)
{
    unsigned long idx = cache_hash(src_id, dst_id)
                        & (unsigned long)(cache->num_buckets - 1);
    RouteCacheEntry *entry = cache->buckets[idx];
    RouteCacheEntry *prev = NULL;

    while (entry != NULL) {
        if (entry->src_id == src_id && entry->dst_id == dst_id) {
            if (entry->graph_version == g->version) {
                cache->hits++;
                return entry->route;
            }
            /* Road weights or statuses changed since this route was
             * computed: the entry is invalid, so evict it. */
            if (prev == NULL) {
                cache->buckets[idx] = entry->next;
            } else {
                prev->next = entry->next;
            }
            route_free(entry->route);
            free(entry);
            cache->size--;
            cache->stale_evictions++;
            break;
        }
        prev = entry;
        entry = entry->next;
    }
    cache->misses++;
    return NULL;
}

int route_cache_store(RouteCache *cache, const Graph *g, const Route *route)
{
    unsigned long idx;
    RouteCacheEntry *entry;
    Route *copy = route_clone(route);

    if (copy == NULL) {
        return -1;
    }
    entry = (RouteCacheEntry *)malloc(sizeof(RouteCacheEntry));
    if (entry == NULL) {
        route_free(copy);
        return -1;
    }
    entry->src_id = route->src_id;
    entry->dst_id = route->dst_id;
    entry->graph_version = g->version;
    entry->route = copy;
    idx = cache_hash(route->src_id, route->dst_id)
          & (unsigned long)(cache->num_buckets - 1);
    entry->next = cache->buckets[idx];
    cache->buckets[idx] = entry;
    cache->size++;
    return 0;
}

void route_cache_clear(RouteCache *cache)
{
    int i;

    for (i = 0; i < cache->num_buckets; i++) {
        RouteCacheEntry *entry = cache->buckets[i];
        while (entry != NULL) {
            RouteCacheEntry *next = entry->next;
            route_free(entry->route);
            free(entry);
            entry = next;
        }
        cache->buckets[i] = NULL;
    }
    cache->size = 0;
}

void route_cache_free(RouteCache *cache)
{
    if (cache == NULL) {
        return;
    }
    route_cache_clear(cache);
    free(cache->buckets);
    free(cache);
}

const Route *evac_query(Graph *g, RouteCache *cache, int src_id, int dst_id,
                        int *from_cache)
{
    const Route *cached;
    Route *fresh;

    if (graph_get_vertex(g, src_id) == NULL
            || graph_get_vertex(g, dst_id) == NULL) {
        if (from_cache != NULL) {
            *from_cache = 0;
        }
        return NULL;
    }
    cached = route_cache_lookup(cache, g, src_id, dst_id);
    if (cached != NULL) {
        if (from_cache != NULL) {
            *from_cache = 1;
        }
        return cached;
    }
    fresh = dijkstra_shortest_route(g, src_id, dst_id, NULL);
    if (fresh == NULL) {
        return NULL;
    }
    if (route_cache_store(cache, g, fresh) != 0) {
        /* Caching failed; the answer is still valid but cannot be kept.
         * Leak nothing: free the fresh route and report the failure. */
        route_free(fresh);
        return NULL;
    }
    route_free(fresh);
    if (from_cache != NULL) {
        *from_cache = 0;
    }
    /* Return the copy the cache now owns (the miss was already counted
     * by route_cache_lookup above). */
    return cache_find_raw(cache, src_id, dst_id)->route;
}
