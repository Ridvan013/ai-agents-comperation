#include <stdlib.h>

#include "buddy.h"

#define MAXRANK 16

/*
 * Buddy memory allocator.
 *
 * A block of rank r spans 2^(r-1) contiguous 4K pages and is aligned to that
 * size (in page units).  We keep one page-descriptor per page and a free list
 * per rank.  Only the first page of a block ("head") carries valid metadata.
 */
struct pmeta {
    int  order;   /* rank of the block whose head this page is */
    int  next;    /* free-list links (page indices), -1 == none */
    int  prev;
    char state;   /* 0 = free, 1 = allocated                    */
    char is_head; /* 1 if this page is the head of its block    */
};

#define PAGE_SIZE 4096

static struct pmeta *meta = NULL;
static void         *g_base = NULL;
static long          g_pages = 0;

static int free_head[MAXRANK + 1];
static int free_cnt[MAXRANK + 1];

/* block size in pages for a given rank */
static inline int rank_pages(int r) { return 1 << (r - 1); }

static void list_add(int i, int r)
{
    meta[i].order   = r;
    meta[i].state   = 0;
    meta[i].is_head = 1;
    meta[i].prev    = -1;
    meta[i].next    = free_head[r];
    if (free_head[r] != -1)
        meta[free_head[r]].prev = i;
    free_head[r] = i;
    free_cnt[r]++;
}

static void list_remove(int i, int r)
{
    int pv = meta[i].prev;
    int nx = meta[i].next;
    if (pv != -1)
        meta[pv].next = nx;
    else
        free_head[r] = nx;
    if (nx != -1)
        meta[nx].prev = pv;
    free_cnt[r]--;
}

int init_page(void *p, int pgcount)
{
    long i;

    if (pgcount <= 0)
        return -EINVAL;

    if (meta != NULL)
        free(meta);

    meta = (struct pmeta *)calloc((size_t)pgcount, sizeof(struct pmeta));
    if (meta == NULL)
        return -ENOSPC;

    g_base  = p;
    g_pages = pgcount;

    for (i = 1; i <= MAXRANK; ++i) {
        free_head[i] = -1;
        free_cnt[i]  = 0;
    }

    /* Carve the region into the largest aligned blocks that fit. */
    i = 0;
    while (i < pgcount) {
        int k = 0; /* rank index: rank = k + 1, size = 2^k pages */
        while (k + 1 <= MAXRANK - 1 &&
               (i % (1 << (k + 1)) == 0) &&
               (i + (1 << (k + 1)) <= pgcount))
            k++;
        list_add((int)i, k + 1);
        i += (1 << k);
    }

    return OK;
}

void *alloc_pages(int rank)
{
    int o, i;

    if (rank < 1 || rank > MAXRANK)
        return ERR_PTR(-EINVAL);

    /* smallest available block of at least the requested rank */
    for (o = rank; o <= MAXRANK; ++o)
        if (free_head[o] != -1)
            break;

    if (o > MAXRANK)
        return ERR_PTR(-ENOSPC);

    i = free_head[o];
    list_remove(i, o);

    /* split down to the requested rank, freeing the upper halves */
    while (o > rank) {
        int child   = o - 1;
        int halfsz  = rank_pages(child); /* 2^(o-2) pages */
        int j       = i + halfsz;
        list_add(j, child);
        o = child;
    }

    meta[i].state   = 1;
    meta[i].order   = rank;
    meta[i].is_head = 1;

    return (void *)((char *)g_base + (long)i * PAGE_SIZE);
}

int return_pages(void *p)
{
    long off;
    int  i, o;

    if (p == NULL || meta == NULL)
        return -EINVAL;

    off = (char *)p - (char *)g_base;
    if (off < 0 || off >= g_pages * PAGE_SIZE || off % PAGE_SIZE != 0)
        return -EINVAL;

    i = (int)(off / PAGE_SIZE);
    if (!meta[i].is_head || meta[i].state != 1)
        return -EINVAL;

    o = meta[i].order;
    meta[i].state = 0;

    /* coalesce with free buddies of the same rank */
    while (o < MAXRANK) {
        int size = rank_pages(o);
        int b    = i ^ size;
        int h, other;

        if (b >= g_pages)
            break;
        if (!meta[b].is_head || meta[b].state != 0 || meta[b].order != o)
            break;

        list_remove(b, o);
        h     = (i < b) ? i : b;
        other = (i < b) ? b : i;
        meta[other].is_head = 0;
        i = h;
        o++;
    }

    list_add(i, o);
    return OK;
}

int query_ranks(void *p)
{
    long off;
    int  i, j;

    if (p == NULL || meta == NULL)
        return -EINVAL;

    off = (char *)p - (char *)g_base;
    if (off < 0 || off >= g_pages * PAGE_SIZE || off % PAGE_SIZE != 0)
        return -EINVAL;

    i = (int)(off / PAGE_SIZE);
    if (meta[i].is_head)
        return meta[i].order;

    /* internal page: the containing block is the nearest head at or below i */
    for (j = i; j >= 0; --j)
        if (meta[j].is_head)
            return meta[j].order;

    return -EINVAL;
}

int query_page_counts(int rank)
{
    if (rank < 1 || rank > MAXRANK)
        return -EINVAL;
    return free_cnt[rank];
}
