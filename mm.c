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
    "team8",
    /* First member's full name */
    "Chaerim Hwang",
    /* First member's email address */
    "cofla159@naver.com",
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
#define NEXT_BLKP(p) ((char *)(p) + GET_SIZE(HDRP(p)))         // (물리적) 다음 블록 주소
#define PREV_BLKP(p) ((char *)(p)-GET_SIZE((char *)(p)-DSIZE)) // (물리적) 이전 블록 주소
#define GET_NEXT_HDR(p) (HDRP(NEXT_BLKP(p)))                   // 다음 블록 헤더값 가져오기
#define GET_PREV_HDR(p) (HDRP(PREV_BLKP(p)))                   // 다음 블록 헤더값 가져오기
#define GET_NEXT_FTR(p) (FTRP(NEXT_BLKP(p)))                   // 다음 블록 헤더값 가져오기
#define GET_PREV_FTR(p) (FTRP(PREV_BLKP(p)))                   // 다음 블록 헤더값 가져오기
#define PRED_PRED(p) (*GET(p))                                 // predecessor의 predecessor가 가지고 있는 값
#define SUCC_SUCC(p) (*GET(p + WSIZE))                         // successor의 successor가 가지고 있는 값

static void *coalesce(void *ptr)
{
    size_t prev_allocated = GET_ALLOC(GET_PREV_FTR(ptr));
    size_t next_allocated = GET_ALLOC(GET_NEXT_HDR(ptr));
    size_t now_size = GET_SIZE(HDRP(ptr));
    // 합치고 난 뒤에 predecessor랑 successor 업데이트 해줘야 함.
    if (prev_allocated && !next_allocated)
    {
        now_size += GET_SIZE(GET_NEXT_HDR(ptr));
        PUT(HDRP(ptr), PACK(now_size, 0));
        PUT(FTRP(ptr), PACK(now_size, 0));
    }
    else if (!prev_allocated && next_allocated)
    {
        now_size += GET_SIZE(GET_PREV_HDR(ptr));
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
    PUT(GET_NEXT_HDR(start_p), PACK(0, 1));

    return coalesce(start_p);
}

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    char *heap_startp = mem_sbrk(4 * WSIZE);
    if (heap_startp == (void *)-1)
    {
        return -1;
    }
    PUT(heap_startp, 0);                          // 패딩
    PUT(heap_startp + WSIZE, PACK(DSIZE, 1));     // 프롤로그 헤더
    PUT(heap_startp + WSIZE * 2, PACK(DSIZE, 1)); // 프롤로그 푸터
    // 여기다가 initial predecessor랑 successor가 추가되어야 함.
    PUT(heap_startp + WSIZE * 3, PACK(0, 1)); // 에필로그

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
    {
        return -1;
    }
    return 0;
}

static void *find_fit(size_t asize)
{
    char *start_p = mem_heap_lo() + DSIZE; // root(init할 때의 successor)

    // root가 가리키는 곳에 가서 할당여부&크기 확인.
    // 할당할 수 있다면 predecessor와 successor 포인터를 업데이트 해준 뒤 포인터 리턴.
    // 할당할 수 없으면 현재 포인터를 *successor로 업데이트 해서 윗줄로 돌아가기.
    // 링크드리스트 끝(처음 init할 때의 initial block)까지 다 봤는데도 없으면 return NULL.

    // while (1)
    // {
    //     if (find_p > (char *)mem_heap_hi())
    //     {
    //         return NULL;
    //     }
    //     if (!GET_ALLOC(HDRP(find_p)) && GET_SIZE(HDRP(find_p)) >= asize)
    //     {
    //         return (void *)find_p;
    //     }
    //     find_p = NEXT_BLKP(find_p);
    // }

    // while (1)
    // {
    //     if (find_p > (char *)mem_heap_hi())
    //     {
    //         return NULL;
    //     }
    //     if (!GET_ALLOC(HDRP(find_p)) && GET_SIZE(HDRP(find_p)) >= asize)
    //     {
    //         return (void *)find_p;
    //     }
    //     find_p = NEXT_BLKP(find_p);
    // }
}

static void place(void *bp, size_t asize)
{
    size_t free_size = GET_SIZE(HDRP(bp));
    if ((free_size - asize) >= (2 * DSIZE))
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        PUT(GET_NEXT_HDR(bp), PACK(free_size - asize, 0));
        PUT(GET_NEXT_FTR(bp), PACK(free_size - asize, 0));
    }
    else
    {
        PUT(HDRP(bp), PACK(free_size, 1));
        PUT(FTRP(bp), PACK(free_size, 1));
    }
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */

void *mm_malloc(size_t size)
{
    size_t asize;
    char *bp;

    if (size == 0)
    {
        return NULL;
    }
    if (size <= DSIZE)
    {
        asize = 2 * DSIZE;
    }
    else
    {
        asize = ALIGN(size + DSIZE);
    }
    if ((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }
    size_t extend_size = asize > CHUNKSIZE ? asize : CHUNKSIZE;
    if ((bp = (char *)extend_heap(extend_size / WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    // predessor랑 successor 업데이트
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
    copySize = GET_SIZE(HDRP(oldptr));
    if (size < copySize)
        copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}
