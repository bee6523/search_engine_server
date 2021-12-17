#include <stdio.h>

// if set to 1, use tcmalloc. else use ordinary malloc.
#define TC_MALLOC_ENABLED 1

//2^20 pages should be managed
#define PAGE_DOMAIN 1<<20
#define PAGE_SIZE 4096
#define PAGE_MASK 4095

#define OBJ_PER_BUNCH 100
#define MIN_LARGE 32768

#define NUM_SIZE_CLASSES 168
#define NUM_LARGE_CLASSES 256

//heap memory start with 32MB, increases by 32MB
#define INIT_SYS_SIZE (1<<16)
#define INCR_SYS_SIZE (1<<13)
#define INIT_MEMORY_SIZE (1<<24)
#define INCR_MEMORY_SIZE (1<<24)

#define CENTRAL_BATCH 8
#define THREAD_BATCH 8

#define SIZE_TO_PAGE_NUM(s) (((s)+PAGE_MASK) >> 12)
#define SIZE_TO_LC(s) (SIZE_TO_PAGE_NUM(s) > 256 ? 256 : SIZE_TO_PAGE_NUM(s))
#define ADDR_TO_PAGE(s) ((s) >> 12)
#define ADDR_TO_IDX(s) (ADDR_TO_PAGE((size_t)(s - (size_t)heap_base_ptr)))
#define PAGE_ALIGNED(s) (((s) & PAGE_MASK) == 0)
#define ALIGN(s) ((s) & (~PAGE_MASK))

typedef struct object{
    struct object* next;
} object;

typedef struct fl{
    object* list_;
    size_t length_;
    size_t lowater_;
    size_t max_length_;
} FreeList;

typedef struct tc{
    pthread_t tid;
    struct tc* next;
    struct tc* prev;
    size_t size_;
    size_t max_size;
    FreeList *list_[NUM_SIZE_CLASSES];
} ThreadCache;

typedef struct Span{
    struct Span* _next;
    struct Span* _prev;

    object* objects;
    size_t refcount;

    void* start;
    size_t length;
    int size_class;
} Span;

typedef struct {
    Span* span_;
    Span* empty_;
    object *tc_slots_;
} CentralFreeList;


/*
Performs initialization of the central heap when called in a main thread and
returns a pointer to the starting address of list. TAs do not explicitly define a specific size of the initial
central heap, so find your optimal size through doing iterative tests.
*/
void *tc_central_init();

/*
Creates a thread-local free list when invoked by a child thread and returns a
pointer to the starting address of list.
*/
void *tc_thread_init();

/*
Allocates an object desired by a child thread with size bytes and returns a pointer
that indicates the allocated object. Note that when tc_malloc is called, you need to consider the
above-mentioned allocation techniques according to the requested size (i.e., the small/large allocation
policies) and manages the spans. Care must be taken when allocating large objects, since a child
thread requests to access the central heap
*/
void *tc_malloc(size_t size);

/*
Frees the object pointed by *ptr and returns nothing.
*/
void tc_free(void *ptr);