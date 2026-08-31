/*
 * minheap.c
 * Array-based binary min-heap with position tracking for decrease-key.
 */
#include <stdlib.h>
#include "minheap.h"

MinHeap *minheap_create(int capacity)
{
    MinHeap *heap;
    int i;

    if (capacity < 1) {
        capacity = 1;
    }
    heap = (MinHeap *)malloc(sizeof(MinHeap));
    if (heap == NULL) {
        return NULL;
    }
    heap->nodes = (HeapNode *)malloc((size_t)capacity * sizeof(HeapNode));
    heap->pos = (int *)malloc((size_t)capacity * sizeof(int));
    if (heap->nodes == NULL || heap->pos == NULL) {
        free(heap->nodes);
        free(heap->pos);
        free(heap);
        return NULL;
    }
    for (i = 0; i < capacity; i++) {
        heap->pos[i] = -1;
    }
    heap->size = 0;
    heap->capacity = capacity;
    return heap;
}

int minheap_is_empty(const MinHeap *heap)
{
    return heap->size == 0;
}

int minheap_contains(const MinHeap *heap, int vertex_index)
{
    if (vertex_index < 0 || vertex_index >= heap->capacity) {
        return 0;
    }
    return heap->pos[vertex_index] != -1;
}

/* Swaps two heap slots and keeps the position index consistent. */
static void minheap_swap(MinHeap *heap, int a, int b)
{
    HeapNode tmp = heap->nodes[a];

    heap->nodes[a] = heap->nodes[b];
    heap->nodes[b] = tmp;
    heap->pos[heap->nodes[a].vertex_index] = a;
    heap->pos[heap->nodes[b].vertex_index] = b;
}

/* Moves nodes[i] up while it is smaller than its parent. */
static void minheap_sift_up(MinHeap *heap, int i)
{
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (heap->nodes[i].dist >= heap->nodes[parent].dist) {
            break;
        }
        minheap_swap(heap, i, parent);
        i = parent;
    }
}

void minheap_heapify(MinHeap *heap, int i)
{
    for (;;) {
        int left = 2 * i + 1;
        int right = 2 * i + 2;
        int smallest = i;

        if (left < heap->size
                && heap->nodes[left].dist < heap->nodes[smallest].dist) {
            smallest = left;
        }
        if (right < heap->size
                && heap->nodes[right].dist < heap->nodes[smallest].dist) {
            smallest = right;
        }
        if (smallest == i) {
            break;
        }
        minheap_swap(heap, i, smallest);
        i = smallest;
    }
}

int minheap_insert(MinHeap *heap, int vertex_index, long dist)
{
    if (vertex_index < 0 || vertex_index >= heap->capacity
            || heap->size >= heap->capacity
            || heap->pos[vertex_index] != -1) {
        return -1;
    }
    heap->nodes[heap->size].vertex_index = vertex_index;
    heap->nodes[heap->size].dist = dist;
    heap->pos[vertex_index] = heap->size;
    heap->size++;
    minheap_sift_up(heap, heap->size - 1);
    return 0;
}

HeapNode minheap_extract_min(MinHeap *heap)
{
    HeapNode min = heap->nodes[0];

    heap->pos[min.vertex_index] = -1;
    heap->size--;
    if (heap->size > 0) {
        heap->nodes[0] = heap->nodes[heap->size];
        heap->pos[heap->nodes[0].vertex_index] = 0;
        minheap_heapify(heap, 0);
    }
    return min;
}

int minheap_decrease_key(MinHeap *heap, int vertex_index, long new_dist)
{
    int i;

    if (vertex_index < 0 || vertex_index >= heap->capacity) {
        return -1;
    }
    i = heap->pos[vertex_index];
    if (i == -1 || new_dist >= heap->nodes[i].dist) {
        return -1;
    }
    heap->nodes[i].dist = new_dist;
    minheap_sift_up(heap, i);
    return 0;
}

void minheap_free(MinHeap *heap)
{
    if (heap == NULL) {
        return;
    }
    free(heap->nodes);
    free(heap->pos);
    free(heap);
}
