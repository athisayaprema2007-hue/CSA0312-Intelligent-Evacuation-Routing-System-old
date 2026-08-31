/*
 * minheap.h
 * Binary min-heap keyed by tentative distance, with a position index so
 * decrease-key runs in O(log n). This is the priority queue that drives
 * Dijkstra's algorithm: extract-min always yields the unfinalized
 * intersection with the smallest tentative travel cost.
 */
#ifndef MINHEAP_H
#define MINHEAP_H

/* One heap element: an intersection (by dense array index) and its
 * current tentative distance from the source. */
typedef struct HeapNode {
    int vertex_index;
    long dist;
} HeapNode;

typedef struct MinHeap {
    HeapNode *nodes;   /* array-based complete binary tree */
    int *pos;          /* pos[vertex_index] = slot in nodes[], or -1 */
    int size;
    int capacity;      /* also the size of the pos[] index */
} MinHeap;

/* Creates a heap able to hold `capacity` vertices with indices in
 * [0, capacity). Returns NULL on allocation failure. */
MinHeap *minheap_create(int capacity);

/* Returns 1 if the heap holds no elements. */
int minheap_is_empty(const MinHeap *heap);

/* Returns 1 if the vertex is currently in the heap. */
int minheap_contains(const MinHeap *heap, int vertex_index);

/* Inserts a vertex with its tentative distance. O(log n).
 * Returns 0 on success, -1 if full, already present or out of range. */
int minheap_insert(MinHeap *heap, int vertex_index, long dist);

/* Removes and returns the minimum-distance node. O(log n).
 * The heap must not be empty (check minheap_is_empty first). */
HeapNode minheap_extract_min(MinHeap *heap);

/* Lowers the distance of a vertex already in the heap and restores heap
 * order by sifting up. O(log n). Returns 0 on success, -1 if the vertex
 * is absent or new_dist is not smaller. */
int minheap_decrease_key(MinHeap *heap, int vertex_index, long new_dist);

/* Classic heapify: sifts nodes[i] down until the min-heap property holds
 * for the subtree rooted at i. Used by extract-min. */
void minheap_heapify(MinHeap *heap, int i);

/* Frees the heap. */
void minheap_free(MinHeap *heap);

#endif /* MINHEAP_H */
