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

#define ALIGN 8 // alignment condition
#define HEAD_SIZE 4 // amount of header/footer
#define CHUNKSIZE (1<<12) // extend heap by this amount (bytes)
#define ALIGNED_SIZE(size) ((size + HEAD_SIZE + HEAD_SIZE + ALIGN-1) & ~(ALIGN-1)) // required block size(header+payload+footer). round up to alignment condition.
#define MIN_SIZE ALIGNED_SIZE(1+2*HEAD_SIZE)
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define GET(p) (*(size_t *)(p))
#define PUT(p, val) (*(size_t *)(p) = (val))

#define ADDR_HEAD(bp) ((char *)(bp) - HEAD_SIZE) // get address of header

#define GET_SIZE(bp) (GET(ADDR_HEAD(bp)) & ~(ALIGN-1))
#define IS_ALLOC(bp) (GET(ADDR_HEAD(bp)) & 0x1)

#define PREV_BLOCK(bp) ((char *)(bp) - (GET((char *)(bp) - 2*HEAD_SIZE) & ~(ALIGN-1)))
#define NEXT_BLOCK(bp) ((char *)(bp) + GET_SIZE(bp))

#define ADDR_FOOT(p) ((char *)(p) + GET_SIZE(p) - HEAD_SIZE - HEAD_SIZE) // get address of footer

#define N_CLASS 18 // # of size classes

typedef struct _node{
    struct _node *prev;
    struct _node *next;
} Node;

static char *heap_listp;
static Node *segregated_list;
static void *coalesce(Node *bp);
static void *extend_heap(size_t words);

// int main() {
//     Node *test, *test2, *test3;

//     mem_init();

//     mm_init();
//     test = mm_malloc(4096);
//     test2 = mm_malloc(16);
//     test3 = mm_malloc(8);
//     mm_free(test);
//     mm_free(test2);
//     mm_realloc(test3, 15);
//     mm_free(test3);

//     return 0;
// }

// 연결 리스트에 블록을 삭제
void disconnect_block(Node *bp) {
    if (bp->prev != NULL)
        bp->prev->next = bp->next;
    if (bp->next != NULL)
        bp->next->prev = bp->prev;
}

// 연결 리스트에 블록을 끼워넣기
void connect_block(Node *pre, Node *bp) {
    if (pre->next != NULL)
        pre->next->prev = bp;
    bp->next = pre->next;
    pre->next = bp;
    bp->prev = pre;
}

// log2 size를 올림한 수를 반환
int find_class(size_t words) {
    int pow = 0, i = 1;
    
    while (words > i && pow < N_CLASS-1) {
        i = i << 1;
        pow++;
    }
    return pow;
}

//////////////////////////////////////////////////////////////////////////////////////////

int mm_init(void)
{
    // prologue block
    size_t size = ALIGNED_SIZE(N_CLASS*2*HEAD_SIZE);
    Node *bp;

    heap_listp = mem_sbrk(size+2*HEAD_SIZE);
    if (heap_listp == (void *)-1) return -1;

    heap_listp += ALIGN - HEAD_SIZE;

    PUT(heap_listp, size+1); // prologue header
    PUT(heap_listp+size-HEAD_SIZE, size+1); // prologue footer
    PUT(heap_listp+size, 1); // epilogue header

    heap_listp += HEAD_SIZE; // points prologue payload
    segregated_list = (Node *)heap_listp;

    for (int i = 0; i < N_CLASS; i++) {
        (segregated_list[i]).prev = NULL;
        (segregated_list[i]).next = NULL;
    }
    // extend_heap(1);
    if ((bp = extend_heap(CHUNKSIZE/HEAD_SIZE)) == NULL)
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

    connect_block(segregated_list + find_class(size/ALIGN), (Node *)bp);

    return coalesce(bp);
}

static void *coalesce(Node *bp) {
    Node *prev_bp = (Node *)PREV_BLOCK(bp);
    Node *next_bp = (Node *)NEXT_BLOCK(bp);
    size_t prev_alloc = IS_ALLOC(prev_bp);
    size_t next_alloc = IS_ALLOC(next_bp);
    size_t size = GET_SIZE(bp);

    if (!next_alloc) {
        size += GET_SIZE(next_bp);
        disconnect_block(next_bp);
        PUT(ADDR_HEAD(bp), size);
        PUT(ADDR_FOOT(bp), size);
    }
    if (!prev_alloc) {
        size += GET_SIZE(prev_bp);
        disconnect_block(bp);
        bp = prev_bp;
        PUT(ADDR_HEAD(bp), size);
        PUT(ADDR_FOOT(bp), size);
    }

    disconnect_block(bp);
    connect_block(segregated_list+find_class(size/ALIGN), bp);

    return bp;
}

static void *find_fit(size_t asize) {
    Node *bp = NULL;
    Node *smallest_bp = NULL;
    size_t size, smallest_size = -1;
    int exp = find_class(asize/ALIGN);

    // for (; bp == NULL && exp < N_CLASS; exp++) {
    //     bp = (segregated_list + exp)->next;
    //     while (bp != NULL && GET_SIZE(bp) < asize)
    //         bp = bp->next;
    // }

    // return bp;

    for (; smallest_bp == NULL && exp < N_CLASS; exp++) {
        bp = (segregated_list + exp)->next;
        while (bp != NULL) {
            size = GET_SIZE(bp);
            // 블록을 넣을 수 있는 경우
            // 딱 맞는 경우 바로 반환
            if (size == asize)
                return bp;
            // 블록이 넉넉하고 최소 블록 크기인 경우 갱신
            else if (size > asize && size < smallest_size) {
                smallest_bp = bp;
                smallest_size = size;
            }
            else
                bp = bp->next;
        }
    }
    
    return smallest_bp;
}

static void place(Node *bp, size_t asize) {
    size_t old_size;
    Node *next;

    if (asize < (old_size = GET_SIZE(bp)) - MIN_SIZE) {
        disconnect_block(bp);
        PUT(ADDR_HEAD(bp), asize+1);
        PUT(ADDR_FOOT(bp), asize+1);

        next = (Node *)(((char *)bp) + asize);
        PUT(ADDR_HEAD(next), old_size-asize);
        PUT(ADDR_FOOT(next), old_size-asize);
        connect_block(segregated_list + find_class((old_size-asize)/ALIGN), next);
    }
    else {
        disconnect_block(bp);
        PUT(ADDR_HEAD(bp), old_size+1);
        PUT(ADDR_FOOT(bp), old_size+1);
    }
}

void *mm_malloc(size_t size)
{
    size_t asize; // adjusted size
    char *bp;
    
    if (size <= 0)
        return NULL;

    asize = ALIGNED_SIZE(size);

    bp = find_fit(asize);
    if (bp == NULL) {
        bp = extend_heap(MAX(asize, CHUNKSIZE)/HEAD_SIZE);
        if (bp == NULL)
            return NULL;
    }

    place((Node *)bp, asize);
    return bp;
}

void mm_free(void *bp)
{
    size_t size = GET_SIZE(bp);
    Node *cur = segregated_list + find_class((size/ALIGN));

    PUT(ADDR_HEAD(bp), size);
    PUT(ADDR_FOOT(bp), size);
    // while ((size_t)(cur->next) > (size_t)bp) cur = cur->next; // list by address
    connect_block(cur, (Node *)bp);

    coalesce(bp);
}

void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    void *next_block = NEXT_BLOCK(ptr);
    size_t old_size = GET_SIZE(ptr);
    size_t dsize;
    size_t copySize;
    
    if (ALIGNED_SIZE(size) <= old_size) {
        return oldptr;
    }

    dsize = ALIGNED_SIZE(size)-old_size;
    if (!IS_ALLOC(next_block) && dsize <= GET_SIZE(next_block)) {
        place(next_block, dsize);
        size = GET_SIZE(next_block);

        PUT(ADDR_HEAD(oldptr), old_size+size+1);
        PUT(ADDR_FOOT(oldptr), old_size+size+1);

        return oldptr;
    }
    else {
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
}