#include "buddy.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define NULL ((void *)0)

#define PAGE_SIZE 4096
#define MAX_RANK 16

enum block_state {
    BLOCK_NONE = 0,
    BLOCK_FREE = 1,
    BLOCK_USED = 2,
};

static char *pool_base = NULL;
static int pool_pages = 0;
static unsigned char *block_rank = NULL;
static unsigned char *block_state = NULL;
static int *free_prev = NULL;
static int *free_next = NULL;
static int free_head[MAX_RANK + 1];
static int free_count[MAX_RANK + 1];

static int pages_for_rank(int rank) {
    return 1 << (rank - 1);
}

static int rank_is_valid(int rank) {
    return rank >= 1 && rank <= MAX_RANK;
}

static void reset_allocator(void) {
    free(block_rank);
    free(block_state);
    free(free_prev);
    free(free_next);

    block_rank = NULL;
    block_state = NULL;
    free_prev = NULL;
    free_next = NULL;
    pool_base = NULL;
    pool_pages = 0;

    for (int rank = 0; rank <= MAX_RANK; ++rank) {
        free_head[rank] = -1;
        free_count[rank] = 0;
    }
}

static void add_free_block(int start, int rank) {
    block_rank[start] = (unsigned char)rank;
    block_state[start] = BLOCK_FREE;
    free_prev[start] = -1;
    free_next[start] = free_head[rank];
    if (free_head[rank] != -1) {
        free_prev[free_head[rank]] = start;
    }
    free_head[rank] = start;
    free_count[rank] += 1;
}

static void remove_free_block(int start, int rank) {
    int prev = free_prev[start];
    int next = free_next[start];

    if (prev != -1) {
        free_next[prev] = next;
    } else {
        free_head[rank] = next;
    }

    if (next != -1) {
        free_prev[next] = prev;
    }

    free_prev[start] = -1;
    free_next[start] = -1;
    block_rank[start] = 0;
    block_state[start] = BLOCK_NONE;
    free_count[rank] -= 1;
}

static int page_index_from_pointer(void *p) {
    uintptr_t addr;
    uintptr_t base;
    uintptr_t size_bytes;

    if (pool_base == NULL || p == NULL) {
        return -1;
    }

    addr = (uintptr_t)p;
    base = (uintptr_t)pool_base;
    size_bytes = (uintptr_t)pool_pages * PAGE_SIZE;

    if (addr < base || addr >= base + size_bytes) {
        return -1;
    }

    if (((addr - base) % PAGE_SIZE) != 0) {
        return -1;
    }

    return (int)((addr - base) / PAGE_SIZE);
}

int init_page(void *p, int pgcount) {
    int current = 0;
    int remaining = pgcount;

    if (p == NULL || pgcount <= 0) {
        return -EINVAL;
    }

    reset_allocator();

    pool_base = (char *)p;
    pool_pages = pgcount;

    block_rank = (unsigned char *)calloc((size_t)pgcount, sizeof(*block_rank));
    block_state = (unsigned char *)calloc((size_t)pgcount, sizeof(*block_state));
    free_prev = (int *)malloc((size_t)pgcount * sizeof(*free_prev));
    free_next = (int *)malloc((size_t)pgcount * sizeof(*free_next));

    if (block_rank == NULL || block_state == NULL ||
        free_prev == NULL || free_next == NULL) {
        reset_allocator();
        return -ENOSPC;
    }

    for (int index = 0; index < pgcount; ++index) {
        free_prev[index] = -1;
        free_next[index] = -1;
    }

    while (remaining > 0) {
        int rank = MAX_RANK;

        while (rank > 1) {
            int pages = pages_for_rank(rank);
            if (pages <= remaining && (current % pages) == 0) {
                break;
            }
            rank -= 1;
        }

        add_free_block(current, rank);
        current += pages_for_rank(rank);
        remaining -= pages_for_rank(rank);
    }

    return OK;
}

void *alloc_pages(int rank) {
    int current_rank;
    int start;

    if (!rank_is_valid(rank) || pool_base == NULL) {
        return ERR_PTR(-EINVAL);
    }

    current_rank = rank;
    while (current_rank <= MAX_RANK && free_head[current_rank] == -1) {
        current_rank += 1;
    }

    if (current_rank > MAX_RANK) {
        return ERR_PTR(-ENOSPC);
    }

    start = free_head[current_rank];
    remove_free_block(start, current_rank);

    while (current_rank > rank) {
        int half_pages;
        int buddy_start;

        current_rank -= 1;
        half_pages = pages_for_rank(current_rank);
        buddy_start = start + half_pages;
        add_free_block(buddy_start, current_rank);
    }

    block_rank[start] = (unsigned char)rank;
    block_state[start] = BLOCK_USED;

    return pool_base + ((size_t)start * PAGE_SIZE);
}

int return_pages(void *p) {
    int start = page_index_from_pointer(p);
    int rank;

    if (start < 0) {
        return -EINVAL;
    }

    if (block_rank[start] == 0 || block_state[start] != BLOCK_USED) {
        return -EINVAL;
    }

    rank = block_rank[start];
    block_rank[start] = 0;
    block_state[start] = BLOCK_NONE;

    while (rank < MAX_RANK) {
        int buddy = start ^ pages_for_rank(rank);

        if (buddy < 0 || buddy >= pool_pages) {
            break;
        }

        if (block_rank[buddy] != rank || block_state[buddy] != BLOCK_FREE) {
            break;
        }

        remove_free_block(buddy, rank);
        if (buddy < start) {
            start = buddy;
        }
        rank += 1;
    }

    add_free_block(start, rank);
    return OK;
}

int query_ranks(void *p) {
    int page = page_index_from_pointer(p);

    if (page < 0) {
        return -EINVAL;
    }

    for (int rank = MAX_RANK; rank >= 1; --rank) {
        int pages = pages_for_rank(rank);
        int start = page & ~(pages - 1);

        if (start >= pool_pages || start + pages > pool_pages) {
            continue;
        }

        if (block_rank[start] == rank) {
            return rank;
        }
    }

    return -EINVAL;
}

int query_page_counts(int rank) {
    if (!rank_is_valid(rank) || pool_base == NULL) {
        return -EINVAL;
    }

    return free_count[rank];
}
