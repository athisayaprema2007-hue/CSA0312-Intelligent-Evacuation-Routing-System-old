/*
 * hashmap.c
 * Separate-chaining hash map (int key -> void* value) with automatic
 * resizing. Average O(1) put/get/remove.
 */
#include <stdlib.h>
#include "hashmap.h"

#define HASHMAP_MIN_BUCKETS 8
#define HASHMAP_MAX_LOAD_NUM 3   /* resize when size > buckets * 3/4 */
#define HASHMAP_MAX_LOAD_DEN 4

/* Knuth multiplicative hash; keys may be any int, including negatives. */
static unsigned long hashmap_hash(int key)
{
    return (unsigned long)((unsigned int)key * 2654435761u);
}

static int round_up_pow2(int n)
{
    int p = HASHMAP_MIN_BUCKETS;
    while (p < n && p < (1 << 30)) {
        p <<= 1;
    }
    return p;
}

HashMap *hashmap_create(int initial_buckets)
{
    HashMap *map = (HashMap *)malloc(sizeof(HashMap));
    if (map == NULL) {
        return NULL;
    }
    map->num_buckets = round_up_pow2(initial_buckets);
    map->size = 0;
    map->buckets = (HashMapEntry **)calloc((size_t)map->num_buckets,
                                           sizeof(HashMapEntry *));
    if (map->buckets == NULL) {
        free(map);
        return NULL;
    }
    return map;
}

/* Doubles the bucket array and re-links every entry. */
static int hashmap_resize(HashMap *map)
{
    int new_count = map->num_buckets * 2;
    HashMapEntry **new_buckets =
        (HashMapEntry **)calloc((size_t)new_count, sizeof(HashMapEntry *));
    int i;

    if (new_buckets == NULL) {
        return -1;
    }
    for (i = 0; i < map->num_buckets; i++) {
        HashMapEntry *entry = map->buckets[i];
        while (entry != NULL) {
            HashMapEntry *next = entry->next;
            unsigned long idx = hashmap_hash(entry->key)
                                & (unsigned long)(new_count - 1);
            entry->next = new_buckets[idx];
            new_buckets[idx] = entry;
            entry = next;
        }
    }
    free(map->buckets);
    map->buckets = new_buckets;
    map->num_buckets = new_count;
    return 0;
}

int hashmap_put(HashMap *map, int key, void *value)
{
    unsigned long idx;
    HashMapEntry *entry;

    if (map->size * HASHMAP_MAX_LOAD_DEN
            > map->num_buckets * HASHMAP_MAX_LOAD_NUM) {
        if (hashmap_resize(map) != 0) {
            return -1;
        }
    }
    idx = hashmap_hash(key) & (unsigned long)(map->num_buckets - 1);
    for (entry = map->buckets[idx]; entry != NULL; entry = entry->next) {
        if (entry->key == key) {
            entry->value = value;
            return 0;
        }
    }
    entry = (HashMapEntry *)malloc(sizeof(HashMapEntry));
    if (entry == NULL) {
        return -1;
    }
    entry->key = key;
    entry->value = value;
    entry->next = map->buckets[idx];
    map->buckets[idx] = entry;
    map->size++;
    return 1;
}

void *hashmap_get(const HashMap *map, int key)
{
    unsigned long idx = hashmap_hash(key) & (unsigned long)(map->num_buckets - 1);
    const HashMapEntry *entry;

    for (entry = map->buckets[idx]; entry != NULL; entry = entry->next) {
        if (entry->key == key) {
            return entry->value;
        }
    }
    return NULL;
}

int hashmap_contains(const HashMap *map, int key)
{
    unsigned long idx = hashmap_hash(key) & (unsigned long)(map->num_buckets - 1);
    const HashMapEntry *entry;

    for (entry = map->buckets[idx]; entry != NULL; entry = entry->next) {
        if (entry->key == key) {
            return 1;
        }
    }
    return 0;
}

int hashmap_remove(HashMap *map, int key)
{
    unsigned long idx = hashmap_hash(key) & (unsigned long)(map->num_buckets - 1);
    HashMapEntry *entry = map->buckets[idx];
    HashMapEntry *prev = NULL;

    while (entry != NULL) {
        if (entry->key == key) {
            if (prev == NULL) {
                map->buckets[idx] = entry->next;
            } else {
                prev->next = entry->next;
            }
            free(entry);
            map->size--;
            return 1;
        }
        prev = entry;
        entry = entry->next;
    }
    return 0;
}

void hashmap_free(HashMap *map)
{
    int i;

    if (map == NULL) {
        return;
    }
    for (i = 0; i < map->num_buckets; i++) {
        HashMapEntry *entry = map->buckets[i];
        while (entry != NULL) {
            HashMapEntry *next = entry->next;
            free(entry);
            entry = next;
        }
    }
    free(map->buckets);
    free(map);
}
