#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#define ALIGNMENT 16
#define MIN_ALLOC ALIGNMENT

struct stats {
    size_t alloc_calls;
    size_t free_calls;
    size_t split_count;
    size_t coalesce_count;

    size_t bytes_requested;
    size_t bytes_in_use;
    size_t peak_bytes_in_use;
};

static struct stats g_stats = {0};


static inline size_t align_up(size_t x)
{
    return (x + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1);
}

#define HDR_SIZE align_up(sizeof(struct block))
#define FTR_SIZE ALIGNMENT



#define HEAP_PAGES 16
void* heap_start = NULL;
size_t heap_size = 0;
struct block* freelist = NULL;

struct block{
    size_t size;
    bool free;
    struct block* next;

};


int init_allocator(void)
{
    size_t page_size = sysconf(_SC_PAGESIZE);
    heap_size = page_size * HEAP_PAGES;

    heap_start = mmap(NULL, heap_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
    if(heap_start == MAP_FAILED) return -1;

    struct block* initial = (struct block*)heap_start;
    initial->size = heap_size - HDR_SIZE - FTR_SIZE;
    initial->free = true;
    initial->next = NULL;

    size_t* footer = (size_t*)((char*)heap_start + HDR_SIZE + initial->size);
    *footer = initial->size;

    freelist = initial;
    return 0;
}

void destroy_allocator(void)
{
    if(heap_start) 
    {
        munmap(heap_start,heap_size);
    }
    heap_start = NULL;
    heap_size = 0;
    freelist = NULL;
}



void split_block(struct block *blk, size_t requested_size)
{
    size_t original_size = blk->size;

    if (original_size < requested_size + sizeof(struct block) + FTR_SIZE + MIN_ALLOC) return;

    struct block* new_block = (struct block*)((char*)blk + HDR_SIZE + requested_size + FTR_SIZE);
    new_block->size = original_size - HDR_SIZE - requested_size - FTR_SIZE;
    new_block->free = true;
    new_block->next = blk->next;
    size_t* new_footer = (size_t*)((char*)new_block + HDR_SIZE + new_block->size);
    *new_footer = new_block->size;

    blk->size = requested_size;
    blk->free = false;
    blk->next = new_block;
    size_t* old_footer = (size_t*)((char*)blk + HDR_SIZE + requested_size);
    *old_footer = blk->size;

    g_stats.split_count++;

}


void* my_malloc( size_t size)
{
    if (size == 0) return NULL;
    size = align_up(size);

    g_stats.alloc_calls++;
    g_stats.bytes_requested += size;
    g_stats.bytes_in_use += size;
    if (g_stats.bytes_in_use > g_stats.peak_bytes_in_use)
        g_stats.peak_bytes_in_use = g_stats.bytes_in_use;


    struct block *curr = freelist;
    struct block *prev = NULL;
    while(curr!=NULL)
    {
        if(curr->size>=size)
        {
            if(curr->size >= size + sizeof(struct block) + FTR_SIZE + MIN_ALLOC)
            {
                split_block(curr,size);

                if(prev)
                {
                    prev->next = curr->next;
                }
                else
                {
                    freelist = curr->next;
                }
            }
            else
            {
                if(prev)
                {
                    prev->next = curr->next;
                }
                else 
                {
                    freelist = curr->next;
                }
                
                size_t* footer = (size_t*)((char*)curr + sizeof(struct block) + curr->size);
                *footer = curr->size;
                curr->free = false;
            }

            void *user_ptr = (void*)((char*)curr + HDR_SIZE);

            if (((uintptr_t)user_ptr % ALIGNMENT) != 0)
            {
                printf("ALIGNMENT ERROR: %p\n", user_ptr);
            }

            return user_ptr;
        }
        
        prev = curr;
        curr = curr->next;        
    }
    return NULL;
}



    void my_free(void* ptr)
    {
        if (!ptr) return;

        struct block *blk = (struct block*)((char*)ptr - HDR_SIZE);
        blk->free = true;

        g_stats.free_calls++;
        g_stats.bytes_in_use -= blk->size;


        size_t H = HDR_SIZE;
        size_t F = FTR_SIZE;

        //backward coalescing
        if((char*)blk > (char*)heap_start + H)
        {
            char* prev_footer = (char*)blk - F;
            size_t prev_size = *(size_t*)prev_footer;

            struct block* prev_block = (struct block*)((char*)blk - F - prev_size - H);

            if(prev_block->free)
            {
                struct block *temp = freelist;
                struct block *prev = NULL;

                while(temp!=NULL)
                {
                    if(temp==prev_block)
                    {
                        if(prev)
                        {
                            prev->next = temp->next;
                        }
                        else 
                        {
                            freelist = temp->next;
                        }
                        break;
                    }

                    prev = temp;
                    temp = temp->next;
                }

                size_t new_size = blk->size + prev_block->size + H + F;
                blk = prev_block;
                blk->size = new_size;

                g_stats.coalesce_count++;

            }
        }


        //forward coalescing
        struct block *next_block = ((struct block*)((char*)blk + H + blk->size + F));
        
        if((char*)next_block < (char*)heap_start + heap_size)
        {
            if(next_block->free == true) 
            {
                struct block *temp = freelist;
                struct block *prev = NULL;

                while(temp!=NULL)
                {
                    if(temp==next_block)
                    {
                        if(prev)
                        {
                            prev->next = temp->next;
                        }
                        else 
                        {
                            freelist = temp->next;
                        }
                        break;
                    }

                    prev = temp;
                    temp = temp->next;
                }

                blk->size = blk->size + H + next_block->size + F;

                g_stats.coalesce_count++;
            }
        }

        blk->next = freelist;
        freelist = blk;


        size_t* footer = ((size_t*)((char*)blk + blk->size + H));
        *footer = blk->size;

    }   
    

static void dump_freelist(void)
{
    printf("---- freelist ----\n");
    struct block *cur = freelist;
    int i = 0;
    while (cur)
    {
        printf("[%d] block=%p size=%zu free=%d next=%p\n",
               i, (void*)cur, cur->size, cur->free, (void*)cur->next);
        cur = cur->next;
        i++;
    }
    printf("------------------\n");
}


static void print_stats(void)
{
    printf("\n==== allocator stats ====\n");
    printf("alloc calls: %zu\n", g_stats.alloc_calls);
    printf("free calls: %zu\n", g_stats.free_calls);
    printf("splits: %zu\n", g_stats.split_count);
    printf("coalesces: %zu\n", g_stats.coalesce_count);
    printf("bytes requested (aligned sum): %zu\n", g_stats.bytes_requested);
    printf("peak bytes in use: %zu\n", g_stats.peak_bytes_in_use);
    printf("=========================\n");
}


int main()
{
    if (init_allocator() != 0)
    {
        perror("init_allocator failed");
        return 1;
    }

    printf("Initial:\n");
    dump_freelist();

    void *a = my_malloc(1);
    void *b = my_malloc(17);
    void *c = my_malloc(100);

    printf("Allocated:\n");
    printf("a=%p b=%p c=%p\n", a, b, c);
    dump_freelist();

    my_free(b);
    printf("Freed b:\n");
    dump_freelist();

    my_free(a);
    printf("Freed a (may coalesce with b if adjacent):\n");
    dump_freelist();

    my_free(c);
    printf("Freed c (should become one big block again):\n");
    dump_freelist();

    print_stats();
    destroy_allocator();
    return 0;
}
