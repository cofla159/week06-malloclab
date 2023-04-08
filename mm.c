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

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
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
    ""};

/* single word (4) or double word (8) alignment */
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1 << 12)

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (DSIZE - 1)) & ~0x7)
#define PACK(size, alloc) ((size) | (alloc))
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(p) ((char *)(p)-WSIZE)                            // 헤더 주소
#define FTRP(p) ((char *)(p) + GET_SIZE(HDRP(p)) - DSIZE)      // 푸터 주소
#define NEXT_BLKP(p) ((char *)(p) + GET_SIZE(HDRP(p)))         // 다음 블록 주소
#define PREV_BLKP(p) ((char *)(p)-GET_SIZE((char *)(p)-DSIZE)) // 이전 블록 주소
#define GET_NEXT_HDR(p) (HDRP(NEXT_BLKP(p)))                   // 다음 블록 헤더값 가져오기
#define GET_PREV_HDR(p) (HDRP(PREV_BLKP(p)))                   // 다음 블록 헤더값 가져오기
#define GET_NEXT_FTR(p) (FTRP(NEXT_BLKP(p)))                   // 다음 블록 헤더값 가져오기
#define GET_PREV_FTR(p) (FTRP(PREV_BLKP(p)))                   // 다음 블록 헤더값 가져오기

static void *extend_heap(size_t words)
{
    size_t size = words % 2 ? (words + 1) * WSIZE : words * WSIZE;
    void *start_p = mem_sbrk(size);
    if (start_p == (void *)-1)
    {
        return NULL;
    }
    PUT(HDRP(start_p), PACK(size, 0));
    PUT(FTRP(start_p), PACK(size, 0));
    PUT(NEXT_BLKP(start_p), PACK(0, 1));

    return coalesce(start_p);
}

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    void *heap_listp = mem_sbrk(4 * WSIZE);
    if (heap_listp == (void *)-1)
    {
        return -1;
    }
    PUT(heap_listp, 0);
    PUT(heap_listp + WSIZE, PACK(DSIZE, 0));
    PUT(heap_listp + WSIZE * 2, PACK(DSIZE, 0));
    PUT(heap_listp + WSIZE * 3, PACK(0, 1));
    heap_listp += DSIZE;

    extend_heap(CHUNKSIZE / WSIZE);
}

static void *coalesce(void *ptr)
{
    size_t prev_allocated = GET_ALLOC(GET_PREV_FTR(ptr));
    size_t next_allocated = GET_ALLOC(GET_NEXT_HDR(ptr));
    size_t now_size = GET_SIZE(HDRP(ptr));
    if (prev_allocated && !next_allocated)
    {
        now_size += GET_SIZE(GET_NEXT_HDR(ptr));
        PUT(HDRP(ptr), PACK(now_size, 0));
        PUT(FTRP(ptr), PACK(now_size, 0));
    }
    else if (!prev_allocated && next_allocated)
    {
        now_size += GET_SIZE(GET_PREV_FTR(ptr));
        PUT(FTRP(ptr), PACK(now_size, 0));
        PUT(GET_PREV_HDR(ptr), PACK(now_size, 0));
        ptr = PREV_BLKP(ptr);
    }
    else if (!prev_allocated && !next_allocated)
    {
        now_size += GET_SIZE(GET_PREV_FTR(ptr)) + GET_SIZE(GET_NEXT_HDR(ptr));
        PUT(GET_PREV_HDR(ptr), PACK(now_size, 0));
        PUT(GET_NEXT_FTR(ptr), PACK(now_size, 0));
        ptr = PREV_BLKP(ptr);
    }
    return ptr;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    int newsize = ALIGN(size + SIZE_T_SIZE);
    void *p = mem_sbrk(newsize);
    if (p == (void *)-1)
        return NULL;
    else
    {
        *(size_t *)p = size;
        return (void *)((char *)p + SIZE_T_SIZE);
    }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
        copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}
