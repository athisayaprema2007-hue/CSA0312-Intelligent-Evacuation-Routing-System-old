/*
 * hashset.h
 * Separate-chaining hash set of integers.
 * Used inside Dijkstra's algorithm to record intersections that have
 * already been finalized, so no intersection is ever processed twice
 * (this is what makes cyclic road networks safe to traverse).
 */
#ifndef HASHSET_H
#define HASHSET_H

typedef struct HashSetEntry {
    int key;
    struct HashSetEntry *next;
} HashSetEntry;

typedef struct HashSet {
    HashSetEntry **buckets;
    int num_buckets;   /* always a power of two */
    int size;          /* number of stored keys */
} HashSet;

/* Creates a set. initial_buckets is rounded up to a power of two (min 8).
 * Returns NULL on allocation failure. */
HashSet *hashset_create(int initial_buckets);

/* Adds a key. Returns 1 if newly added, 0 if already present,
 * -1 on allocation failure. */
int hashset_add(HashSet *set, int key);

/* Returns 1 if the key is present, 0 otherwise. */
int hashset_contains(const HashSet *set, int key);

/* Removes every key but keeps the set usable. */
void hashset_clear(HashSet *set);

/* Frees the set. */
void hashset_free(HashSet *set);

#endif /* HASHSET_H */
