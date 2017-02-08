/*
 * mm.c
 *
 * Name: Tianyu Mi
 * Andrew ID: tianyum
 * Date: Nov/14/2014
 */

/*
 * Brief Introduction:
 * This solution uses segremented free list to maximize space utility
 * and throughput. Each free list contains blocks with the same size.
 *
 * There are 14 free lists which accordingly store block with size of
 * [1,4],[5,8],[9,16],...,[9193,+inf].
 *
 * Block layout:
 *
 * 1.allocated block
 *
 *   [header: block size | 1]
 *   [payload...]
 *   [...]
 *   [footer: block size | 1]
 * 
 * 2.free block
 *
 *   [header: block size | 0]
 *   [8 byte pointer to next block]
 *   [8 byte pointer to prev block]
 *   [...]
 *   [footer: block size | 0]
 *
 *
 * Fit Strategy:
 * Use first fit to find the free block satisfying the target size
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#include "mm.h"
#include "memlib.h"

/* If you want debugging output, use the following macro.  When you hand
 * in, remove the #define DEBUG line. */
#define DEBUG
#ifdef DEBUG
# define dbg_printf(...) printf(__VA_ARGS__)
#else
# define dbg_printf(...)
#endif


/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif /* def DRIVER */

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(p) (((size_t)(p) + (ALIGNMENT-1)) & ~0x7)

# define WSIZE 8
# define DSIZE 16
# define CHUNKSIZE (1<<8)
# define NUMLIST 14

/* Global variable */
/* first block */
static char *heap_listp;
/* array of free lists */
static char *free_lists[NUMLIST];

/*
 * Return whether the pointer is in the heap.
 * May be useful for debugging.
 */
static int in_heap(const void *p) {
    return p <= mem_heap_hi() && p >= mem_heap_lo();
}

/*
 * Return whether the pointer is aligned.
 * May be useful for debugging.
 */
static int aligned(const void *p) {
    return (size_t)ALIGN(p) == (size_t)p;
}

/* MAX between the two */
# define MAX(a,b) ((a)>(b)? (a):(b))


/* transform a size and allocated status into a word*/
# define form(size,alloc) ((size) |(alloc))

/* read a word at given address */
# define get(p) (*(size_t *)(p))

/* write a word at given address */
# define put(p,x) (get(p)=x)

/* get the size of a word */
# define getsize(p) (get(p)& ~0x7)

/* get the allocated status of a word */
# define getalloc(p) (get(p)& 0x1)

/* get the head address given block pointer*/
# define head(bp) ((char *)bp-WSIZE)

/* get the foot address given block pointer*/
# define foot(bp) ((char *)bp-DSIZE+getsize(head(bp)))

/* get next block pointer given block pointer */
# define nextblock(bp) ((char *)bp+getsize((char *)bp-WSIZE))

/* get prev block pointer given block pointer */
# define prevblock(bp) ((char *)bp-getsize((char *)bp-DSIZE))

/* get the previous block given the block pointer */
static void *getprev(void *bp) {
    return (void *)(*((size_t *)bp));
}
/* get the next block given the block pointer*/
static void *getnext(void *bp) {
    size_t *next = (size_t *)((char *)bp + WSIZE);
    return (void *)(* next);
}

/* set previous block for a free block*/
static void setprev(void *bp, size_t prev) {
    *((size_t *)bp) = prev;
}

/* set next block for a free block */
static void setnext(void *bp, size_t next){
    size_t *helper= (size_t *)((char *)bp + WSIZE);
    *helper= next;
}



/* compute list index given block size */
static int getindex(size_t size){
    size = size-1;

    if (size > (1<<(NUMLIST))){
        return (NUMLIST-2);
    }
    /* first list */
    else if (size <= 4){
        return 0;
    }
    int i=0;
    for (i=-1; size !=1; i++) {
        size= (size>>1);
    }
    return i;
}

/* insert a block into free lists*/
static void insert(void *bp){
    size_t size = getsize(head(bp));
    
    int i=0;
    i= getindex(size);
    
    char *freelist= free_lists[i];
    /* there is no block of that size in the list*/
    if (!freelist){
        freelist=bp;
        setprev(bp,0);
        setnext(bp,0);
    }
    /* there is block of that size in the list */
    else{
        void *blk1=freelist;
        freelist=bp;
        setnext(bp,(size_t)blk1);
        setprev(bp,0);
        setprev(blk1,(size_t)bp);
    }
    
    free_lists[i]=freelist;
}

/* delete a block from free lists */
static void delete(void *bp){
    size_t size= getsize(head(bp));
    int i=getindex(size);
    
    char *freelist=free_lists[i];
    void *prev=getprev(bp);
    void *next=getnext(bp);
    /* the block is the first block */
    if(!prev){
        freelist=next;
    }
    /* the block is not the first block */
    else{
        setnext(prev,(size_t)next);
    }
    if (next){
        setprev(next,(size_t)prev);
    }
    /* remove bp */
    setnext(bp,0);
    setprev(bp,0);
    
    free_lists[i]=freelist;
}



/* extend heap by the given words*/
static void *extend_heap (size_t words) {
    void *bp;
    size_t size;
    
    /* to maintain alignment */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    
    if (((long) (bp = mem_sbrk(size))) == -1) {
        return NULL;
    }
    /* free blk header */
    put(head(bp), form(size, 0));
    /* free blk footer */
    put(foot(bp), form(size, 0));
    /* new epilogue header */
    put(head(nextblock(bp)), form(0, 1));
    
    insert(bp);
    size_t prevalloc = getalloc(foot(prevblock(bp)));
    size_t nextalloc = getalloc(head(nextblock(bp)));
    size_t sizehelper = getsize(head(bp));
    
    /* both prev& next block are allocated, no coalesce*/
    if (prevalloc && nextalloc){
    }
    /* coalesce with next blk */
    else if (prevalloc && !nextalloc){
        delete(bp);
        delete(nextblock(bp));
        sizehelper += getsize(head(nextblock(bp)));
        put(head(bp),form(sizehelper,0));
        put(foot(bp),form(sizehelper,0));
        insert(bp);
    }
    /* coalesce with prev blk */
    else if (!prevalloc && nextalloc){
        void *prev = prevblock(bp);
        delete(bp);
        delete(prev);
        sizehelper += getsize(head(prev));
        put(foot(bp),form(sizehelper,0));
        put(head(prev),form(sizehelper,0));
        bp=prev;
        insert(bp);
    }
    /* coalesce with both prev and next blk*/
    else {
        void *prev =prevblock(bp);
        void *next =nextblock(bp);
        delete(prev);
        delete(next);
        delete(bp);
        sizehelper += getsize(head(prev));
        sizehelper += getsize(foot(next));
        put(head(prev),form(sizehelper,0));
        put(foot(next),form(sizehelper,0));
        bp=prev;
        insert(bp);
    }
    return bp;
}

/* first fit*/
static void *findfit (size_t asize){
    char *bp;
    int i=getindex(asize);
    for(;i<NUMLIST;i++){
        bp = free_lists[i];
        while(bp){
            if(asize<=(getsize(head(bp)))){
                return bp;
            }
            else{
                bp=getnext(bp);
            }
        }
    }
    return NULL;
    
}

/* set head and foot for a newly allocated block
 * split blocks if there is enough remaining space
 */
static void place (void *bp, size_t asize) {
    size_t free_size = getsize(head(bp));
    size_t remain = free_size - asize;
    
    delete(bp);
    
    if (remain >= 2 * DSIZE) {
        put(head(bp), form(asize, 1));
        put(foot(bp), form(asize, 1));
        setnext(bp, 0);
        setprev(bp, 0);
        void *next = nextblock(bp);
        put(head(next), form(remain, 0));
        put(foot(next), form(remain, 0));
        insert(next);
    }
    else{
        asize = free_size;
        put(head(bp), form(asize, 1));
        put(foot(bp), form(asize, 1));
        setnext(bp, 0);
        setprev(bp, 0);    }

}



/*
 * Initialize: return -1 on error, 0 on success.
 */
int mm_init(void) {
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *) -1) {
        return -1;
    }
    
    put(heap_listp, 0);
    put(heap_listp +  WSIZE, form(DSIZE, 1));
    put(heap_listp + (2 * WSIZE), form(DSIZE, 1));
    put(heap_listp + (3 * WSIZE), form(0, 1));
    heap_listp = heap_listp + DSIZE;
    
    memset(free_lists, 0, sizeof(char *) * NUMLIST);
    
    mm_checkheap(1);
    
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) {
        return -1;
    }
    return 0;
}

/*
 * malloc
 */
void *malloc (size_t size) {
    size_t asize;  // Adjusted block size
    size_t extendsize; // Amount to extend heap if no fit
    char *bp;
    
    // Ignore spurious requests
    if (size == 0) {
        return NULL;
    }
    
    // Adjust block size to include overhead and alignment reqs
    if (size <= DSIZE) {
        asize = 2 * DSIZE;
    }
    else {
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    }
    
    // Search the free list for a fit
    if ((bp = findfit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }
    
    // No fit found. Get more memory and place the block
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL) {
        return NULL;
    }
    place(bp, asize);
    
    return bp;
}

/*
 * free
 */
void free (void *ptr) {
    /* invalid pointer */
    if(!ptr){
        return;
    }
    if (!in_heap(ptr)) {
        printf("the blk to be freed is not in the heap. \n");
    }
    size_t size = getsize(head(ptr));
    
    put(head(ptr),form(size,0));
    put(foot(ptr),form(size,0));
    insert(ptr);
    
    // Coalesce if the previous block was free
    size_t prevalloc = getalloc(foot(prevblock(ptr)));
    size_t nextalloc = getalloc(head(nextblock(ptr)));
    size_t sizehelper = getsize(head(ptr));
    
    /* both prev& next block are allocated, no coalesce*/
    if (prevalloc && nextalloc){
        
    }
    /* coalesce with next blk */
    else if (prevalloc && !nextalloc){
        delete(ptr);
        delete(nextblock(ptr));
        sizehelper += getsize(head(nextblock(ptr)));
        put(head(ptr),form(sizehelper,0));
        put(foot(ptr),form(sizehelper,0));
        insert(ptr);
    }
    /* coalesce with prev blk */
    else if (!prevalloc && nextalloc){
        void *prev = prevblock(ptr);
        delete(ptr);
        delete(prev);
        sizehelper += getsize(head(prev));
        put(foot(ptr),form(sizehelper,0));
        put(head(prev),form(sizehelper,0));
        ptr=prev;
        insert(ptr);
    }
    /* coalesce with both prev and next blk*/
    else {
        void *prev =prevblock(ptr);
        void *next =nextblock(ptr);
        delete(prev);
        delete(next);
        delete(ptr);
        sizehelper += getsize(head(prev));
        sizehelper += getsize(foot(next));
        put(head(prev),form(sizehelper,0));
        put(foot(next),form(sizehelper,0));
        ptr=prev;
        insert(ptr);
    }
    }

/*
 * realloc - change the size of the block by mallocing a new block,
 *           and copy its data.
 */
void *realloc(void *oldptr, size_t size) {
    size_t osize;
    void *npointer;
    
    /* size = 0 */
    if (!size){
        free(oldptr);
        return NULL;
    }
    
    /* if oldptr is null, then just malloc */
    if(!oldptr ){
        return malloc(size);
    }
    
    npointer= malloc(size);
    /* if realloc*/
    if(!npointer){
        return NULL;
    }
    
    /* copy old data */
    osize=getsize(head(oldptr));
    if(size<osize){
        osize=size;
    }
    memcpy(npointer,oldptr,osize);
    
    free(oldptr);
    
    return npointer;
}

/*
 * calloc - allocate the block and set it to zero
 */
void *calloc (size_t nmemb, size_t size) {
    size_t bytes = nmemb * size;
    void *pointer;
    pointer=malloc(bytes);
    memset(pointer,0,bytes);
    
    return pointer;
}
 
 


/*
 * mm_checkheap
 */
void mm_checkheap(int lineno) {
    lineno=lineno;
    
    char *bp= heap_listp;
    size_t size=getsize(head(bp));
    
    while (size!=0) {
        if (!aligned(bp)) {
            printf(" not well aligned. \n");
            exit(1);
        }
        if (getsize(head(bp))!=getsize(foot(bp))) {
            printf(" header and footer not matched. \n");
        }
        if (!in_heap(bp)) {
            printf("out of heap boundary. \n");
        }
        if ((int)getalloc(head(bp))==0 &&
            (int)getalloc(head(nextblock(bp)))==0) {
            printf("two consecutive free blks. \n");
        }
        bp=nextblock(bp);
        size=getsize(head(bp));
    }
    
    for (int i=0; i<NUMLIST; i++) {
        bp=free_lists[i];
        while (bp) {
            if (getalloc(head(bp))) {
                printf(" freelists contain allocated blk. \n ");
                exit(1);
            }
            if (!in_heap(bp)) {
                printf(" not within [mem_heap_lo,mem_heap_high].\n ");
            }
            
            size_t size=getsize(head(bp));
            int index=getindex(size);
            int high;
            if(index>(NUMLIST-2)){
                high=((1<<15)-1);
            }
            else{
                high=1<<(index+2);
            }
            int low;
            if (index==0) {
                low=0;
            }
            else if(index>(NUMLIST-2)){
                low=(1<<NUMLIST)+1;
            }
            else{
                low=1<<(index+1);
            }
            if ( size>(size_t)high ||size<(size_t)low ) {
                printf(" blk not in the list of proper range. \n ");
            }
            if (lineno) {
                printf("%d: \n",i);
                printf("addr=%p,prev=%p,succ=%p \n",
                       bp,prevblock(bp),nextblock(bp));
            }
            bp=getnext(bp);
        }
    }
    
    

}