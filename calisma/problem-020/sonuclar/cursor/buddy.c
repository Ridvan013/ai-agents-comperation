#include "buddy.h"

#include <stdlib.h>
#include <stddef.h>

#define PAGE_SIZE 4096
#define MAX_RANK 16

/*
 * Buddy memory allocator.
 *
 * The managed region is [g_base, g_base + g_npages * PAGE_SIZE). Pages are
 * grouped into buddy blocks; a block of rank r spans 2^(r-1) pages and is
 * aligned to that size. Free blocks are kept in per-rank doubly linked lists
 * (sorted by page index so allocation returns the lowest available address).
 *
 * Metadata is stored in page-indexed arrays:
 *   g_state[i] : 0 = interior page (not a block head)
 *                1 = head of a free block
 *                2 = head of an allocated block
 *   g_order[i] : rank of the block whose head is page i (valid when head)
 *   g_next/g_prev : intrusive free-list links (valid for free block heads)
 */

static char *g_base = NULL;
static int g_npages = 0;

static unsigned char *g_order = NULL;
static unsigned char *g_state = NULL;
static int *g_next = NULL;
static int *g_prev = NULL;

static int free_head[MAX_RANK + 1];
static int free_tail[MAX_RANK + 1];
static int free_cnt[MAX_RANK + 1];

/* Insert a block (head page = idx) into the sorted free list for `rank`.
 * The list is kept ascending by page index; we scan from the tail so that
 * the common case (appending an increasing address) is O(1). */
static void fl_insert(int idx, int rank) {
    g_order[idx] = (unsigned char)rank;
    g_state[idx] = 1;

    int t = free_tail[rank];
    while (t != -1 && t > idx) t = g_prev[t];

    if (t == -1) {
        g_prev[idx] = -1;
        g_next[idx] = free_head[rank];
        if (free_head[rank] != -1)
            g_prev[free_head[rank]] = idx;
        else
            free_tail[rank] = idx;
        free_head[rank] = idx;
    } else {
        int n = g_next[t];
        g_next[idx] = n;
        g_prev[idx] = t;
        if (n != -1)
            g_prev[n] = idx;
        else
            free_tail[rank] = idx;
        g_next[t] = idx;
    }
    free_cnt[rank]++;
}

static void fl_remove(int idx, int rank) {
    int p = g_prev[idx];
    int n = g_next[idx];
    if (p != -1) g_next[p] = n; else free_head[rank] = n;
    if (n != -1) g_prev[n] = p; else free_tail[rank] = p;
    free_cnt[rank]--;
    g_state[idx] = 0;
}

/* Translate a user pointer to a page index, validating it lies on a page
 * boundary inside the managed region. Returns -1 when illegal. */
static long pointer_to_page(void *pp) {
    if (g_base == NULL) return -1;
    char *q = (char *)pp;
    if (q < g_base) return -1;
    ptrdiff_t off = q - g_base;
    if (off % PAGE_SIZE != 0) return -1;
    long idx = (long)(off / PAGE_SIZE);
    if (idx >= g_npages) return -1;
    return idx;
}

int init_page(void *p, int pgcount) {
    free(g_order);
    free(g_state);
    free(g_next);
    free(g_prev);
    g_order = NULL;
    g_state = NULL;
    g_next = NULL;
    g_prev = NULL;

    g_base = (char *)p;
    g_npages = pgcount;

    for (int r = 0; r <= MAX_RANK; r++) {
        free_head[r] = -1;
        free_tail[r] = -1;
        free_cnt[r] = 0;
    }

    if (pgcount <= 0) return OK;

    g_order = (unsigned char *)malloc((size_t)pgcount);
    g_state = (unsigned char *)calloc((size_t)pgcount, 1);
    g_next = (int *)malloc((size_t)pgcount * sizeof(int));
    g_prev = (int *)malloc((size_t)pgcount * sizeof(int));
    if (!g_order || !g_state || !g_next || !g_prev) return -ENOSPC;

    /* Greedy decomposition into maximal aligned buddy blocks. */
    long i = 0;
    while (i < pgcount) {
        int r = MAX_RANK;
        while (r >= 1) {
            long size = 1L << (r - 1);
            if ((i & (size - 1)) == 0 && i + size <= pgcount) break;
            r--;
        }
        fl_insert((int)i, r);
        i += 1L << (r - 1);
    }
    return OK;
}

void *alloc_pages(int rank) {
    if (rank < 1 || rank > MAX_RANK) return ERR_PTR(-EINVAL);
    if (g_base == NULL) return ERR_PTR(-ENOSPC);

    int r = rank;
    while (r <= MAX_RANK && free_cnt[r] == 0) r++;
    if (r > MAX_RANK) return ERR_PTR(-ENOSPC);

    int idx = free_head[r];
    fl_remove(idx, r);

    /* Split down to the requested rank, freeing the upper buddy each step. */
    while (r > rank) {
        r--;
        int buddy = idx + (1 << (r - 1));
        fl_insert(buddy, r);
    }

    g_state[idx] = 2;
    g_order[idx] = (unsigned char)rank;
    return (void *)(g_base + (size_t)idx * PAGE_SIZE);
}

int return_pages(void *pp) {
    long idx = pointer_to_page(pp);
    if (idx < 0) return -EINVAL;
    if (g_state[idx] != 2) return -EINVAL; /* not an allocated block head */

    int r = g_order[idx];
    g_state[idx] = 0;

    /* Coalesce with free buddies while possible. */
    while (r < MAX_RANK) {
        int size = 1 << (r - 1);
        int buddy = (int)(idx ^ size);
        if (buddy >= g_npages) break;
        if (g_state[buddy] != 1 || g_order[buddy] != r) break;

        fl_remove(buddy, r);
        if (buddy < idx) idx = buddy;
        r++;
    }

    fl_insert((int)idx, r);
    return OK;
}

int query_ranks(void *pp) {
    long idx = pointer_to_page(pp);
    if (idx < 0) return -EINVAL;

    for (int r = 1; r <= MAX_RANK; r++) {
        int size = 1 << (r - 1);
        long head = idx & ~((long)size - 1);
        if (g_state[head] != 0 && g_order[head] == r) return r;
    }
    return -EINVAL;
}

int query_page_counts(int rank) {
    if (rank < 1 || rank > MAX_RANK) return -EINVAL;
    return free_cnt[rank];
}
