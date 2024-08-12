/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"
// #include "memlib.c"


team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};


#define ALIGN 8 // bytes
#define HEAD_SIZE 4 // bytes
#define CHUNKSIZE (1<<12) // extend heap by this amount (bytes)
#define ALIGNED_SIZE(size) ((size + HEAD_SIZE + HEAD_SIZE + ALIGN-1) & ~(ALIGN-1))

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define GET(p) (*(size_t *)(p))
#define PUT(p, val) (*(size_t *)(p) = (val))

#define ADDR_HEAD(bp) ((char *)(bp) - HEAD_SIZE)

#define GET_SIZE(bp) (GET(ADDR_HEAD(bp)) & ~(ALIGN-1))
#define IS_ALLOC(bp) (GET(ADDR_HEAD(bp)) & 0x1)

#define PREV_BLOCK(bp) ((char *)(bp) - (GET((char *)(bp) - 2*HEAD_SIZE) & ~(ALIGN-1)))
#define NEXT_BLOCK(bp) ((char *)(bp) + GET_SIZE(bp))

#define ADDR_FOOT(p) ((char *)(p) + GET_SIZE(p) - HEAD_SIZE - HEAD_SIZE)

static char *heap_listp;
static void *coalesce(void *bp);
static void *extend_heap(size_t words);

int mm_init(void)
{
    // prologue block
    size_t size = ALIGNED_SIZE(0);

    heap_listp = mem_sbrk(size+2*HEAD_SIZE);
    if (heap_listp == (void *)-1) return -1;

    heap_listp += ALIGN - HEAD_SIZE;

    PUT(heap_listp, size+1); // prologue header
    PUT(heap_listp+size-HEAD_SIZE, size+1); // prologue footer
    PUT(heap_listp+size, 1); // epilogue header
    heap_listp += size-HEAD_SIZE; // points prologue footer

    if (extend_heap(CHUNKSIZE/HEAD_SIZE) == NULL)
        return -1;
    return 0;
}

static void *extend_heap(size_t words) {
    void *bp;
    size_t size = (words%2) ? (words+1) * HEAD_SIZE : words * HEAD_SIZE;

    bp = mem_sbrk(size);
    if (bp == (void *)-1) return NULL;

    PUT(ADDR_HEAD(bp), size); // header of new free block
    PUT(ADDR_FOOT(bp), size); // footer of new free block
    PUT(ADDR_HEAD(NEXT_BLOCK(bp)), 1); // header of epilogue block

    return coalesce(bp);
}

static void *coalesce(void *bp) {
    size_t *prev_bp = (size_t *)PREV_BLOCK(bp);
    size_t *next_bp = (size_t *)NEXT_BLOCK(bp);
    size_t prev_alloc = IS_ALLOC(prev_bp);
    size_t next_alloc = IS_ALLOC(next_bp);
    size_t size = GET_SIZE(bp);

    if (!next_alloc) {
        size += GET_SIZE(next_bp);
        PUT(ADDR_HEAD(bp), size);
        PUT(ADDR_FOOT(bp), size);
    }
    if (!prev_alloc) {
        size += GET_SIZE(prev_bp);
        bp = prev_bp;
        PUT(ADDR_HEAD(bp), size);
        PUT(ADDR_FOOT(bp), size);
    }

    return bp;
}

static void *find_fit(size_t asize) {
    char *cur_blk = heap_listp;
    size_t b_size;

    b_size = GET_SIZE(cur_blk);
    while (IS_ALLOC(cur_blk) || b_size < asize) {
        if (b_size == 0) return NULL;
        cur_blk += b_size;
        b_size = GET_SIZE(cur_blk);
    }

    return cur_blk;
}

static void place(void *bp, size_t asize) {
    static size_t min_size = ALIGNED_SIZE(1+2*HEAD_SIZE);
    size_t old_size;

    if (asize < (old_size = GET_SIZE(bp)) - min_size) {
        PUT(ADDR_HEAD(bp), asize+1);
        PUT(ADDR_FOOT(bp), asize+1);
        PUT(ADDR_HEAD(NEXT_BLOCK(bp)), old_size-asize);
        PUT(ADDR_FOOT(NEXT_BLOCK(bp)), old_size-asize);
    }
}

void *mm_malloc(size_t size)
{
    size_t asize; // adjusted size
    char *bp;
    
    if (size <= 0)
        return NULL;

    asize = ALIGNED_SIZE(size+2*HEAD_SIZE);

    bp = find_fit(asize);
    if (bp == NULL) {
        asize = MAX(asize, CHUNKSIZE);
        bp = extend_heap(asize/HEAD_SIZE);
        if (bp == NULL)
            return NULL;
    }

    place(bp, asize);
    return bp;
}

void mm_free(void *bp)
{
    size_t size = GET_SIZE(bp);

    PUT(ADDR_HEAD(bp), size);
    PUT(ADDR_FOOT(bp), size);
    coalesce(bp);
}

void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;
    copySize = *(size_t *)((char *)oldptr - sizeof(size_t));
    if (size < copySize)
        copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

// int main() {
//     void *test, *test2, *test3;

//     mem_init();

//     mm_init();
//     test = mm_malloc(4096);
//     test2 = mm_malloc(16);
//     test3 = mm_malloc(8);
//     mm_free(test);
//     mm_free(test2);
//     mm_free(test3);

//     return 0;
// }