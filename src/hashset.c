/*
 * hashset.c
 * Separate-chaining integer hash set with automatic resizing.
 * Average O(1) add/contains.
 */
#include <stdlib.h>
#include "hashset.h"

#define HASHSET_MIN_BUCKETS 8
#define HASHSET_MAX_LOAD_NUM 3   /* resize when size > buckets * 3/4 */
#define HASHSET_MAX_LOAD_DEN 4

static unsigned long hashset_hash(int key)
{
    return (unsigned long)((unsigned int)key * 2654435761u);
}

static int round_up_pow2(int n)
{
    int p = HASHSET_MIN_BUCKETS;
    while (p < n && p < (1 << 30)) {
        p <<= 1;
    }
    return p;
}

HashSet *hashset_create(int initial_buckets)
{
    HashSet *set = (HashSet *)malloc(sizeof(HashSet));
    if (set == NULL) {
        return NULL;
    }
    set->num_buckets = round_up_pow2(initial_buckets);
    set->size = 0;
    set->buckets = (HashSetEntry **)calloc((size_t)set->num_buckets,
                                           sizeof(HashSetEntry *));
    if (set->buckets == NULL) {
        free(set);
        return NULL;
    }
    return set;
}

static int hashset_resize(HashSet *set)
{
    int new_count = set->num_buckets * 2;
    HashSetEntry **new_buckets =
        (HashSetEntry **)calloc((size_t)new_count, sizeof(HashSetEntry *));
    int i;

    if (new_buckets == NULL) {
        return -1;
    }
    for (i = 0; i < set->num_buckets; i++) {
        HashSetEntry *entry = set->buckets[i];
        while (entry != NULL) {
            HashSetEntry *next = entry->next;
            unsigned long idx = hashset_hash(entry->key)
                                & (unsigned long)(new_count - 1);
            entry->next = new_buckets[idx];
            new_buckets[idx] = entry;
            entry = next;
        }
    }
    free(set->buckets);
    set->buckets = new_buckets;
    set->num_buckets = new_count;
    return 0;
}

int hashset_add(HashSet *set, int key)
{
    unsigned long idx;
    HashSetEntry *entry;

    if (set->size * HASHSET_MAX_LOAD_DEN
            > set->num_buckets * HASHSET_MAX_LOAD_NUM) {
        if (hashset_resize(set) != 0) {
            return -1;
        }
    }
    idx = hashset_hash(key) & (unsigned long)(set->num_buckets - 1);
    for (entry = set->buckets[idx]; entry != NULL; entry = entry->next) {
        if (entry->key == key) {
            return 0;
        }
    }
    entry = (HashSetEntry *)malloc(sizeof(HashSetEntry));
    if (entry == NULL) {
        return -1;
    }
    entry->key = key;
    entry->next = set->buckets[idx];
    set->buckets[idx] = entry;
    set->size++;
    return 1;
}

int hashset_contains(const HashSet *set, int key)
{
    unsigned long idx = hashset_hash(key) & (unsigned long)(set->num_buckets - 1);
    const HashSetEntry *entry;

    for (entry = set->buckets[idx]; entry != NULL; entry = entry->next) {
        if (entry->key == key) {
            return 1;
        }
    }
    return 0;
}

void hashset_clear(HashSet *set)
{
    int i;

    for (i = 0; i < set->num_buckets; i++) {
        HashSetEntry *entry = set->buckets[i];
        while (entry != NULL) {
            HashSetEntry *next = entry->next;
            free(entry);
            entry = next;
        }
        set->buckets[i] = NULL;
    }
    set->size = 0;
}

void hashset_free(HashSet *set)
{
    if (set == NULL) {
        return;
    }
    hashset_clear(set);
    free(set->buckets);
    free(set);
}
