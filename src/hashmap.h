/*
 * hashmap.h
 * Separate-chaining hash map from integer keys to void* values.
 * Used by the graph to map intersection IDs to Vertex records, so that
 * vertex lookup by ID is O(1) on average even for arbitrary, sparse IDs.
 */
#ifndef HASHMAP_H
#define HASHMAP_H

/* One key/value pair inside a bucket chain. */
typedef struct HashMapEntry {
    int key;
    void *value;
    struct HashMapEntry *next;
} HashMapEntry;

typedef struct HashMap {
    HashMapEntry **buckets;
    int num_buckets;   /* always a power of two */
    int size;          /* number of stored entries */
} HashMap;

/* Creates a map. initial_buckets is rounded up to a power of two (min 8).
 * Returns NULL on allocation failure. */
HashMap *hashmap_create(int initial_buckets);

/* Inserts or replaces. Returns 1 if a new entry was inserted,
 * 0 if an existing key's value was replaced, -1 on allocation failure. */
int hashmap_put(HashMap *map, int key, void *value);

/* Returns the stored value, or NULL if the key is absent. */
void *hashmap_get(const HashMap *map, int key);

/* Returns 1 if the key is present, 0 otherwise. */
int hashmap_contains(const HashMap *map, int key);

/* Removes a key. Returns 1 if removed, 0 if it was not present.
 * The stored value itself is not freed (the caller owns it). */
int hashmap_remove(HashMap *map, int key);

/* Frees the map's own memory. Stored values are not freed. */
void hashmap_free(HashMap *map);

#endif /* HASHMAP_H */
