#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include "tc_malloc.h"

short cl_enum[48] = {8,16,24,32,40,48,56,64,72,80,88,96,104,112,120,128,144,160,176,192,208,224,240,256,288,320,352,384,416,448,480,512,576,640,704,768,832,896,960,1024,1152,1280,1408,1536,1664,1792,1920,2048};
char verbose=0;

/* PageHeap Object */
sem_t *pageheap_lock;
Span* pagemap_[PAGE_DOMAIN];
Span* large_free_[NUM_LARGE_CLASSES+1]; //for clarity, large_free[0] not used

void* heap_base_ptr; //starting address of heap
void* sys_heap_ptr; //heap memory for system objects(span, freelist, etc)
size_t sys_heap_left;

/* central cache */
CentralFreeList central_cache[NUM_SIZE_CLASSES];
sem_t *cc_lock[NUM_SIZE_CLASSES];

/* circularly linked list of Thread Cache*/
sem_t *tc_lock;
ThreadCache *tc_list;
size_t num_threads;

/* PageHeap Management*/
void* sys_heap_alloc(size_t size);
void* heap_grow(int size);
Span *alloc_span(size_t num_pages);
Span* pagemap_lookup(void *addr);
void pagemap_update(Span *s);

/* Span Management */
Span* span_cache_;
Span* span_new(void *start, size_t pagelen);
void span_push(Span *s, Span **list);
void span_remove(Span* s);
void span_free(Span* s);
void large_free_push(Span* s);

/* CentralFreeList Management */
object *CFreeList_pop(int cl, int batch);

/* ThreadCache Management */
ThreadCache *tc_current();
void *FreeList_pop(FreeList *fl);
void FreeList_fetch(FreeList *fl, object *bunch);
void FreeList_push(FreeList *fl, object *obj);

/* Ohters */
int lock_acquire(sem_t *lock);
int lock_release(sem_t *lock);
size_t cl_to_size(int cl);
int size_to_cl(size_t size);


void* sys_heap_alloc(size_t size){
    if(sys_heap_left < size){
        //just ignore previous page(internal fragmentation)
        sys_heap_ptr = sbrk(INCR_SYS_SIZE);
        if(sys_heap_ptr==(void *)-1){
            fprintf(stderr, "sbrk failed!\n");
            exit(-1);
        }
        sys_heap_left = INCR_SYS_SIZE;
    }
    sys_heap_ptr += size;
    sys_heap_left -= size;
    // if(verbose>3)
    //     printf("%ld system heap alloc. left: %ld\n", size, sys_heap_left);
    return sys_heap_ptr-size;
}

void* heap_grow(int size){
    /*
    increases heap size by size if size>INCR_MEMORY_SIZE, otherwise INCR_MEMORY_SIZE.
    set pagemap_ and large_free_ acoordingly.
    heap lock should be already acquired, size should be page-aligned
    */
    Span* s;
    size_t incr_size = size>INCR_MEMORY_SIZE ? size : INCR_MEMORY_SIZE;

    assert(PAGE_ALIGNED(incr_size));
    
    void *new_p = sbrk(incr_size);
    if(new_p == (void *)-1){
        fprintf(stderr, "sbrk failed!\n");
        exit(-1);
    }
    s = pagemap_lookup(new_p-PAGE_SIZE);
    if(s != NULL && s->refcount == 0){
        // if(verbose >= 3)
        //     fprintf(stderr, "coalesce %p %p heap-grow\n", s, new_p);
        size_t lc = SIZE_TO_LC(s->length);
        if(large_free_[lc] == s)
            large_free_[lc] = s->_next;
        s->length += incr_size;
        span_remove(s);
    }else{
        s = span_new(new_p,SIZE_TO_PAGE_NUM(incr_size));
    }
    large_free_push(s);
    pagemap_update(s);
    // if(verbose)
    //     fprintf(stderr, "heap_grow : heap grow successfully occured\n");
        
    return s->start;
}
Span *alloc_span(size_t num_pages){
    int i;
    Span *leftover, *ret;
    lock_acquire(pageheap_lock);
    for(i=num_pages; i<=NUM_LARGE_CLASSES; i++){
        if(large_free_[i] != NULL){
            break;
        }
    }
    if(i>NUM_LARGE_CLASSES){   //every large_free_ was empty
        heap_grow(num_pages * PAGE_SIZE);
        i = NUM_LARGE_CLASSES;    //to point last element of large_free
    }
    ret = large_free_[i];
    assert(ret->refcount==0);
    //pop from large_free_
    large_free_[i] = ret->_next;
    span_remove(ret);

    if(SIZE_TO_PAGE_NUM(ret->length) > num_pages){
        size_t newsize = num_pages * PAGE_SIZE;
        //split span into two, push leftover to appropriate list
        leftover = span_new(ret->start + newsize, SIZE_TO_PAGE_NUM(ret->length - newsize));

        ret->length = newsize;

        //fill pagemapp_ with new span
        pagemap_update(leftover);
        large_free_push(leftover);
    }
    // if(verbose >= 3)
    //     fprintf(stderr,"allocate span %p from large_free(%d) with size %zu\n",ret, i, ret->length);
    ret->refcount+=1;

    lock_release(pageheap_lock);
    
    return ret;
}
Span* pagemap_lookup(void *addr){
    size_t idx = ADDR_TO_IDX(addr);
    return pagemap_[idx];
}
void pagemap_update(Span *s){
    for(int i=ADDR_TO_IDX(s->start); i < ADDR_TO_IDX(s->start + s->length); i++){
        pagemap_[i] = s;
    }
}

//wrapper for lock acquire/free
int lock_acquire(sem_t *lock){
    int n=0;
    while(1){
        if((n=sem_trywait(lock))==0){
            return 0;
        }
        usleep(1);
    }
}
int lock_release(sem_t *lock){
    return sem_post(lock);
}

size_t cl_to_size(int cl){
    if(cl<=47){
        return cl_enum[cl];
    }else{
        return (cl-39)*256;
    }
}
int size_to_cl(size_t size){
    int thres=128, pos=0, sep=3, prev=0;
    if(size>MIN_LARGE){
        fprintf(stderr, "ERROR: size_to_cl: input size is over 32K, which should be assigned to large chunk!\n");
        exit(-1);
    }
    while(sep<8){
        if(size<=thres){
            return ((size-prev-1)>>sep) + pos;
        }else{
            pos += (thres-prev)>>sep;
            prev=thres;
            sep += 1;
            thres <<= 1;
        }
    }
    return ((size-prev-1)>>sep) + pos;
}

ThreadCache * tc_current(){
    assert(tc_list != NULL);

    lock_acquire(tc_lock);
    ThreadCache *tcp = tc_list, *head = tc_list;
    pthread_t cur = pthread_self();
    do{
        if(tcp->tid == cur){
            tc_list = tcp;
            lock_release(tc_lock);
            return tcp;
        }else tcp = tcp->next;
    }while(tcp != head);
    lock_release(tc_lock);

    fprintf(stderr, "ERROR: tid %ld not found\n", cur);
    return NULL;
}
/* Thread Cache Management */
void *FreeList_pop(FreeList *fl){
    object *ret;
    assert(fl->length_>0);
    ret = fl->list_;
    fl->list_ = ret->next;
    fl->length_--;
    return (void *)ret;
}
void FreeList_fetch(FreeList *fl, object *bunch){
    object *tmp;
    while(bunch != NULL){
        tmp = bunch->next;
        bunch->next = fl->list_;
        fl->list_ = bunch;
        bunch = tmp;
        fl->length_ ++;
    }
    return;
}
void FreeList_push(FreeList *fl, object *obj){
    obj->next = fl->list_;
    fl->list_ = obj;
    fl->length_++;
    return;
}

Span* span_new(void *start, size_t pagelen){
    Span* ret;
    if(span_cache_!=NULL){
        ret=span_cache_;
        span_cache_ = ret->_next;
    }else{
        ret = (Span *)sys_heap_alloc(sizeof(Span));
    }
    //initialize
    ret->start = start;
    ret->length = PAGE_SIZE * pagelen;
    ret->size_class=-1;
    ret->_next = ret->_prev = NULL;
    ret->objects = NULL;
    return ret;
}
void span_push(Span *s, Span **list){
    assert(s->_next == NULL && s->_prev == NULL);
    Span *head = *list;
    if(head != NULL){
        head->_prev = s;
    }
    s->_next = head;
    *list = s;
}
//remove span from linked list
void span_remove(Span* s){
    if(s->_prev != NULL)
        s->_prev->_next = s->_next;
    if(s->_next != NULL)
        s->_next->_prev = s->_prev;
    s->_prev = NULL;
    s->_next = NULL;
}
//free span(for coalesce)
void span_free(Span* s){
    //break link, push it to span cache
    span_remove(s);
    span_push(s, &span_cache_);
}



void large_free_push(Span* s){
    size_t lc = SIZE_TO_LC(s->length);
    span_push(s, &large_free_[lc]);
    // if(verbose)
    //     fprintf(stderr, "pagelen %ld span was added to class %ld\n", SIZE_TO_PAGE_NUM(s->length), lc);
}

int batch_size(size_t size){
    if(size>512)
        return SIZE_TO_PAGE_NUM(size*CENTRAL_BATCH);
    else return 4;
}
/* Central Free List Management */
object *CFreeList_pop(int cl, int batch){
    CentralFreeList *cf;
    object *ret=NULL, *tmp=NULL;
    int count=0;
    lock_acquire(cc_lock[cl]);
    cf = &central_cache[cl];
    while(count<batch && cf->tc_slots_ != NULL){
        tmp = ret;
        ret = cf->tc_slots_;
        cf->tc_slots_ = ret->next;
        ret->next = tmp;
        count++;
    }
    if(count>=batch){
        lock_release(cc_lock[cl]);
        return ret;
    }
CFree_span_scan:
    while(cf->span_ != NULL){
        if(cf->span_->objects == NULL){
            Span *head = cf->span_;
            cf->span_ = head->_next;
            span_remove(head);
            span_push(head, &cf->empty_);
        }else{
            while(count<batch && cf->span_->objects != NULL){
                tmp = ret;
                ret = cf->span_->objects;
                cf->span_->objects = ret->next;
                ret->next = tmp;
                cf->span_->refcount += 1;
                count++;
            }
            if(count>=batch){
                lock_release(cc_lock[cl]);
                return ret;
            }
        }
    }
    if(cf->span_ == NULL){
        //get new span from pageheap
        //get pages for CENTRAL_BATCH object
        size_t size = cl_to_size(cl);
        int req_num = batch_size(size);
        
        Span *new_s = alloc_span(req_num);
        if(new_s == NULL){
            //if fail, try with minimal size
            req_num = SIZE_TO_PAGE_NUM(size);
            new_s = alloc_span(req_num);
            if(new_s == NULL){
                fprintf(stderr, "CFreeList_pop: failed to allocate span for %d pages", req_num);
                exit(-1);
            }
        }
        new_s->size_class = cl;
        object *tmp;
        for(int i=0;i<new_s->length/size; i++){
            tmp=(object *)(new_s->start + (size*i));
            tmp->next = new_s->objects;
            new_s->objects = tmp;
        }
        span_push(new_s, &cf->span_);
        // if(verbose)
        //     printf("CFreeList_pop: acquired %d pages from page heap\n", req_num);
    }
    goto CFree_span_scan;
}

void *tc_thread_init(){
    ThreadCache *tc, *head;
    lock_acquire(pageheap_lock);
    tc = (ThreadCache *)sys_heap_alloc(sizeof(ThreadCache));
    memset(tc, 0, sizeof(ThreadCache));
    tc->tid = pthread_self();
    for(int i=0; i < NUM_SIZE_CLASSES; i++){
        tc->list_[i] = (FreeList *)sys_heap_alloc(sizeof(FreeList));
        memset(tc->list_[i], 0, sizeof(FreeList));
        tc->list_[i]->max_length_ = 100;
    }
    lock_acquire(tc_lock);
    if(tc_list == NULL){
        tc->next = tc;
        tc->prev = tc;
    }else{
        head = tc_list;
        tc->next = head;
        tc->prev = head->prev;
        head->prev->next = tc;
        head->prev = tc;
    }
    tc_list = tc;
    lock_release(tc_lock);
    lock_release(pageheap_lock);
    // if(verbose)
    //     printf("thread %ld: initialized\n", pthread_self());
    return (void *)tc;
}
void *tc_malloc(size_t size){
    if(size>MIN_LARGE){
        int num_pages = SIZE_TO_PAGE_NUM(size);
        Span *ret = alloc_span(num_pages);
        // if(verbose>=4)
        //     printf("thread %ld: large obj with %d pages allocated\n", pthread_self(), num_pages);
        return ret->start;
    }else{
        // if(verbose>=4)
        //     printf("thread %ld: small obj(%ld) requested\n",pthread_self(), size);
        int cl;
        FreeList* fl;
        ThreadCache* cur = tc_current();

        cl = size_to_cl(size);
        fl = cur->list_[cl];
        if(fl->length_ > 0){
            return FreeList_pop(fl);
        }else{
            //get THREAD_BATCH amount of objcet from Central Free List
            object *bunch = NULL;
            int batch = size>512? THREAD_BATCH:THREAD_BATCH*4;
            bunch = CFreeList_pop(cl, batch);
            FreeList_fetch(fl, bunch);
            return FreeList_pop(fl);
        }
    }
}

void tc_free(void *ptr){
    Span *s = pagemap_lookup(ptr);
    size_t lc;
    if(s->size_class==-1){  //large alloc
        Span *prev, *next;
        lock_acquire(pageheap_lock);
        assert(large_free_[SIZE_TO_LC(s->length)] != s);
        s->refcount=0;
        prev = pagemap_lookup(s->start-PAGE_SIZE);
        next = pagemap_lookup(s->start + s->length + PAGE_MASK);
        if(next != NULL && next->refcount == 0){
            // if(verbose >= 3)
            //     fprintf(stderr, "coalesce %p %p cur-next\n", s, next);
            lc = SIZE_TO_LC(next->length);
            if(large_free_[lc] == next)
                large_free_[lc] = next->_next;
            s->length += next->length;
            span_free(next);
        }
        if(prev != NULL && prev->refcount == 0){
            // if(verbose >= 3)
            //     fprintf(stderr, "coalesce %p %p prev-cur\n", prev, s);
            lc = SIZE_TO_LC(prev->length);
            if(large_free_[lc] == prev)
                large_free_[lc] = prev->_next;
            prev->length += s->length;
            span_remove(prev);
            span_free(s);
            s=prev;
        }
        pagemap_update(s);
        large_free_push(s);
        lock_release(pageheap_lock);
    }else{
        ThreadCache *tc = tc_current();
        object *obj = (object *) ptr;
        obj->next = NULL;
        // if(verbose>=4)
        //     fprintf(stderr, "Thread %zx: ptr %p freed to cl %d\n", pthread_self(), ptr, s->size_class);
        FreeList_push(tc->list_[s->size_class], obj);
    }
}

void test_size_to_cl(){
    for(int i=0;i<168;i++){
        assert(size_to_cl(cl_to_size(i))==i);
    }
}

void test_span_new(){
    //check if Span_new correctly allocate&init memory
    Span *span = span_new(NULL,0);
    assert(span->length==0 && span->objects==NULL && 
            span->refcount==0 && span->size_class==-1 && 
            span->start==NULL && span->_next==NULL &&
            span->_prev==NULL);
    span_free(span);
}
void test_simple(){
    test_span_new();
}

void *tc_central_init(){
    //heap allocation using mmap 
    //void *ptr=mmap(NULL, INIT_MEMORY_SIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0);
    span_cache_ = NULL;
    num_threads = 1;
    tc_list = NULL;

    // if(verbose)
    //     printf("initial heap: %p\n", sbrk(0));
    
    memset(pagemap_,0, sizeof(pagemap_));
    memset(large_free_,0, sizeof(large_free_));
    memset(central_cache,0,sizeof(central_cache));
    sem_unlink("/tc_lock");
    sem_unlink("/pageheap_lock");
    tc_lock = sem_open("/tc_lock",O_CREAT,0644,1);
    pageheap_lock = sem_open("/pageheap_lock",O_CREAT, 0644,1);

    for(int i=0; i<NUM_SIZE_CLASSES; i++){
        char buf[20];
        sprintf(buf, "/cc_lock%d",i);
        sem_unlink(buf);
        cc_lock[i] = sem_open(buf,O_CREAT, 0644,1);
    }
    lock_acquire(pageheap_lock);
    //heap allocation using sbrk
    heap_base_ptr = sbrk(0);
    sys_heap_left = ALIGN((size_t)heap_base_ptr + INIT_SYS_SIZE)-(size_t)heap_base_ptr;
    sys_heap_ptr = sbrk(sys_heap_left);

    assert(PAGE_ALIGNED((size_t)sbrk(0)));
    if(verbose)
        printf("initial sys_heap memory: %ld, addr: %p\n", sys_heap_left, sys_heap_ptr);

    void* ptr = heap_grow(INIT_MEMORY_SIZE);
    lock_release(pageheap_lock);

    test_size_to_cl();
    test_simple();
    if(verbose)
        fprintf(stderr, "basic test passed\n");
    return ptr;
}