#include "buddy.h"

#define MAX_PAGES 262144

struct page {
    unsigned char rank;
    unsigned char is_free;
    int next;
    int prev;
};

static struct page page_desc[MAX_PAGES];
static int free_head[17];
static int free_count[17];
static void *base_addr;
static int num_pages;

static void list_remove(int rank, int idx) {
    int prev = page_desc[idx].prev;
    int next = page_desc[idx].next;
    if (prev != -1) page_desc[prev].next = next;
    else free_head[rank] = next;
    if (next != -1) page_desc[next].prev = prev;
    page_desc[idx].prev = -1;
    page_desc[idx].next = -1;
    free_count[rank]--;
}

static void list_add(int rank, int idx) {
    page_desc[idx].next = free_head[rank];
    page_desc[idx].prev = -1;
    if (free_head[rank] != -1) {
        page_desc[free_head[rank]].prev = idx;
    }
    free_head[rank] = idx;
    free_count[rank]++;
}

int init_page(void *p, int pgcount) {
    if (pgcount <= 0 || pgcount > MAX_PAGES) return -EINVAL;
    base_addr = p;
    num_pages = pgcount;
    
    for (int i = 0; i <= 16; i++) {
        free_head[i] = -1;
        free_count[i] = 0;
    }
    
    for (int i = 0; i < num_pages; i++) {
        page_desc[i].rank = 0;
        page_desc[i].is_free = 0;
        page_desc[i].next = -1;
        page_desc[i].prev = -1;
    }
    
    int idx = 0;
    while (idx < num_pages) {
        int r = 16;
        while (r >= 1) {
            int size = 1 << (r - 1);
            if ((idx % size == 0) && (idx + size <= num_pages)) {
                break;
            }
            r--;
        }
        if (r < 1) return -EINVAL;
        
        page_desc[idx].rank = r;
        page_desc[idx].is_free = 1;
        list_add(r, idx);
        idx += (1 << (r - 1));
    }
    return OK;
}

void *alloc_pages(int rank) {
    if (rank < 1 || rank > 16) return ERR_PTR(-EINVAL);
    
    int r = rank;
    while (r <= 16 && free_head[r] == -1) {
        r++;
    }
    if (r > 16) return ERR_PTR(-ENOSPC);
    
    int idx = free_head[r];
    list_remove(r, idx);
    
    while (r > rank) {
        r--;
        int buddy_idx = idx + (1 << (r - 1));
        page_desc[buddy_idx].rank = r;
        page_desc[buddy_idx].is_free = 1;
        list_add(r, buddy_idx);
    }
    
    page_desc[idx].rank = rank;
    page_desc[idx].is_free = 0;
    
    return (void *)((char *)base_addr + idx * 4096);
}

int return_pages(void *p) {
    if (!p) return -EINVAL;
    if ((char *)p < (char *)base_addr) return -EINVAL;
    long diff = (char *)p - (char *)base_addr;
    if (diff % 4096 != 0) return -EINVAL;
    
    int idx = diff / 4096;
    if (idx >= num_pages) return -EINVAL;
    
    if (page_desc[idx].is_free) return -EINVAL;
    
    int r = page_desc[idx].rank;
    if (r < 1 || r > 16) return -EINVAL;
    
    if (idx % (1 << (r - 1)) != 0) return -EINVAL;
    
    page_desc[idx].is_free = 1;
    
    while (r < 16) {
        int buddy_idx = idx ^ (1 << (r - 1));
        if (buddy_idx >= num_pages) break;
        if (!page_desc[buddy_idx].is_free) break;
        if (page_desc[buddy_idx].rank != r) break;
        
        list_remove(r, buddy_idx);
        
        int min_idx = idx < buddy_idx ? idx : buddy_idx;
        int max_idx = idx > buddy_idx ? idx : buddy_idx;
        
        page_desc[max_idx].rank = 0;
        
        idx = min_idx;
        r++;
        page_desc[idx].rank = r;
    }
    
    list_add(r, idx);
    return OK;
}

int query_ranks(void *p) {
    if (!p) return -EINVAL;
    if ((char *)p < (char *)base_addr) return -EINVAL;
    long diff = (char *)p - (char *)base_addr;
    if (diff % 4096 != 0) return -EINVAL;
    
    int idx = diff / 4096;
    if (idx >= num_pages) return -EINVAL;
    
    for (int r = 16; r >= 1; r--) {
        int head_idx = idx & ~((1 << (r - 1)) - 1);
        if (page_desc[head_idx].rank == r) {
            return r;
        }
    }
    
    return -EINVAL;
}

int query_page_counts(int rank) {
    if (rank < 1 || rank > 16) return -EINVAL;
    return free_count[rank];
}
