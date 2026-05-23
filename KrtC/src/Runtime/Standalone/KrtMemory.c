#include "KrtStandalone.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#ifdef _WIN32

static HANDLE g_heap = NULL;

static void ensure_heap(void) {
    if (!g_heap) {
        
        g_heap = HeapCreate(0, 1024 * 1024, 100 * 1024 * 1024);
    }
}

void* KrtMalloc(size_t size) {
    if (size == 0) return NULL;
    ensure_heap();
    return HeapAlloc(g_heap, 0, size);
}

void* KrtCalloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    if (total == 0) return NULL;
    
    void* ptr = KrtMalloc(total);
    if (ptr) {
        KrtMemset(ptr, 0, total);
    }
    return ptr;
}

void* KrtRealloc(void* ptr, size_t size) {
    if (!ptr) return KrtMalloc(size);
    if (size == 0) {
        KrtFree(ptr);
        return NULL;
    }
    
    ensure_heap();
    return HeapReAlloc(g_heap, 0, ptr, size);
}

void KrtFree(void* ptr) {
    if (ptr && g_heap) {
        HeapFree(g_heap, 0, ptr);
    }
}

#else

static void* heap_start = NULL;
static size_t heap_size = 0;
static size_t heap_used = 0;

#define HEAP_INITIAL_SIZE (1024 * 1024)  

typedef struct block_header {
    size_t size;
    int free;
    struct block_header* next;
} block_header_t;

static block_header_t* free_list = NULL;

static void init_heap(void) {
    if (!heap_start) {
        heap_start = mmap(NULL, HEAP_INITIAL_SIZE, 
                          PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (heap_start == MAP_FAILED) {
            heap_start = NULL;
            return;
        }
        
        heap_size = HEAP_INITIAL_SIZE;
        heap_used = 0;
        
        block_header_t* block = (block_header_t*)heap_start;
        block->size = HEAP_INITIAL_SIZE - sizeof(block_header_t);
        block->free = 1;
        block->next = NULL;
        
        free_list = block;
    }
}

static void split_block(block_header_t* block, size_t size) {
    if (block->size > size + sizeof(block_header_t) + 16) {
        block_header_t* new_block = (block_header_t*)((char*)block + sizeof(block_header_t) + size);
        new_block->size = block->size - size - sizeof(block_header_t);
        new_block->free = 1;
        new_block->next = block->next;
        
        block->size = size;
        block->next = new_block;
    }
}

void* KrtMalloc(size_t size) {
    if (size == 0) return NULL;
    
    init_heap();
    if (!heap_start) return NULL;
    
    size = (size + 7) & ~7;
    
    block_header_t* prev = NULL;
    block_header_t* curr = free_list;
    
    while (curr) {
        if (curr->free && curr->size >= size) {
            split_block(curr, size);
            curr->free = 0;
            return (char*)curr + sizeof(block_header_t);
        }
        prev = curr;
        curr = curr->next;
    }
    
    return NULL;  
}

void* KrtCalloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    if (total == 0) return NULL;
    
    void* ptr = KrtMalloc(total);
    if (ptr) {
        KrtMemset(ptr, 0, total);
    }
    return ptr;
}

void* KrtRealloc(void* ptr, size_t size) {
    if (!ptr) return KrtMalloc(size);
    if (size == 0) {
        KrtFree(ptr);
        return NULL;
    }
    
    block_header_t* block = (block_header_t*)((char*)ptr - sizeof(block_header_t));
    
    if (block->size >= size) {
        return ptr;  
    }
    
    void* new_ptr = KrtMalloc(size);
    if (new_ptr) {
        KrtMemcpy(new_ptr, ptr, block->size);
        KrtFree(ptr);
    }
    return new_ptr;
}

void KrtFree(void* ptr) {
    if (!ptr) return;
    
    block_header_t* block = (block_header_t*)((char*)ptr - sizeof(block_header_t));
    block->free = 1;
    
    block_header_t* curr = free_list;
    while (curr && curr->next) {
        if (curr->free && curr->next->free) {
            
            if ((char*)curr + sizeof(block_header_t) + curr->size == (char*)curr->next) {
                curr->size += sizeof(block_header_t) + curr->next->size;
                curr->next = curr->next->next;
                continue;
            }
        }
        curr = curr->next;
    }
}

#endif

typedef struct KrtMempool {
    size_t block_size;
    size_t block_count;
    void* blocks;
    void** free_list;
    size_t free_count;
} KrtMempoolT;

KrtMempoolT* KrtMempoolCreate(size_t block_size, size_t block_count) {
    
    if (block_size < sizeof(void*)) {
        block_size = sizeof(void*);
    }
    
    block_size = (block_size + 7) & ~7;
    
    KrtMempoolT* pool = (KrtMempoolT*)KrtMalloc(sizeof(KrtMempoolT));
    if (!pool) return NULL;
    
    pool->block_size = block_size;
    pool->block_count = block_count;
    pool->free_count = block_count;
    
    size_t total_size = block_size * block_count;
    pool->blocks = KrtMalloc(total_size);
    if (!pool->blocks) {
        KrtFree(pool);
        return NULL;
    }
    
    pool->free_list = (void**)KrtMalloc(block_count * sizeof(void*));
    if (!pool->free_list) {
        KrtFree(pool->blocks);
        KrtFree(pool);
        return NULL;
    }
    
    for (size_t i = 0; i < block_count; i++) {
        pool->free_list[i] = (char*)pool->blocks + i * block_size;
    }
    
    return pool;
}

void KrtMempoolDestroy(KrtMempoolT* pool) {
    if (!pool) return;
    
    KrtFree(pool->blocks);
    KrtFree(pool->free_list);
    KrtFree(pool);
}

void* KrtMempoolAlloc(KrtMempoolT* pool) {
    if (!pool || pool->free_count == 0) return NULL;
    
    return pool->free_list[--pool->free_count];
}

void KrtMempoolFree(KrtMempoolT* pool, void* ptr) {
    if (!pool || !ptr) return;
    
    char* pool_start = (char*)pool->blocks;
    char* pool_end = pool_start + pool->block_size * pool->block_count;
    char* ptr_char = (char*)ptr;
    
    if (ptr_char < pool_start || ptr_char >= pool_end) {
        return;  
    }
    
    if (pool->free_count < pool->block_count) {
        pool->free_list[pool->free_count++] = ptr;
    }
}