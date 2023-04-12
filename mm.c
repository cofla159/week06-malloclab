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
#define __POINTER_TYPE__ unsigned int
typedef __POINTER_TYPE__ pointer_t;

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (DSIZE - 1)) & ~0x7)
#define PACK(size, alloc) ((size) | (alloc))
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define GET(p) (*(pointer_t *)(p))
#define PUT(p, val) (*(pointer_t *)(p) = (val))
#define PUT_ADD(bp, add) (*(pointer_t **)(bp) = (pointer_t *)(add))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(bp) ((char *)(bp)-WSIZE)                             // 헤더 주소
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)      // 푸터 주소
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)))         // (물리적) 다음 블록 주소
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE((char *)(bp)-DSIZE)) // (물리적) 이전 블록 주소
#define GET_NEXT_HDR(bp) (HDRP(NEXT_BLKP(bp)))                    // 다음 블록 헤더 주소 가져오기
#define GET_PREV_HDR(bp) (HDRP(PREV_BLKP(bp)))                    // 이전 블록 헤더 주소 가져오기
#define GET_NEXT_FTR(bp) (FTRP(NEXT_BLKP(bp)))                    // 다음 블록 푸터 주소 가져오기
#define GET_PREV_FTR(bp) (FTRP(PREV_BLKP(bp)))                    // 이전 블록 푸터 주소 가져오기
#define PRED(bp) ((char *)bp)                                     // pred 주소
#define SUCC(bp) ((char *)bp + WSIZE)                             // succ 주소
#define GOTO_PRED(bp) (pointer_t *)(GET(PRED(bp)))                // p의 predecessor가 가리키는 곳(pred block bp값)
#define GOTO_SUCC(bp) (pointer_t *)(GET(SUCC(bp)))                // p의 successor가 가리키는 곳(succ block bp값)

static void *find_linked(void *ptr)
{
    pointer_t *now_element = (pointer_t *)(mem_heap_lo() + DSIZE); // initial block에서 시작(prodecessor)
    while (1)
    {
        if (!GOTO_SUCC(now_element) || GOTO_SUCC(now_element) > (pointer_t *)ptr)
        {
            return now_element;
        }
        now_element = (pointer_t *)GOTO_SUCC(now_element);
    }
    return NULL; // 끝까지 갔는데 없으면 이 블록이 링크드리스트의 끝에 붙는거
}

static void *coalesce(void *ptr)
{
    size_t prev_allocated = GET_ALLOC(GET_PREV_FTR(ptr));
    size_t next_allocated = GET_ALLOC(GET_NEXT_HDR(ptr));
    size_t now_size = GET_SIZE(HDRP(ptr));

    if (prev_allocated && next_allocated)
    {
        // 합칠게 없으면 SUCC(list_prev) = ptr, PRED(ptr) = list_prev , PREV(next) = ptr, SUCC(ptr) = list_next
        pointer_t *list_prev = find_linked(ptr);
        pointer_t *list_next = GOTO_SUCC(list_prev);

        PUT_ADD(SUCC(list_prev), ptr);
        PUT_ADD(PRED(ptr), list_prev);
        if (list_next)
        {
            PUT_ADD(PRED(list_next), ptr);
        }
        PUT_ADD(SUCC(ptr), list_next);
    }
    else if (prev_allocated && !next_allocated)
    {
        // 다음거랑 합치면 list_next가 bp랑 합쳐지기 때문에 SUCC(ptr)에 GOTO_SUCC(list_next) 넣어야 함
        now_size += GET_SIZE(GET_NEXT_HDR(ptr));
        pointer_t *list_prev = GOTO_PRED(NEXT_BLKP(ptr));
        pointer_t *list_next = (pointer_t *)NEXT_BLKP(ptr);

        PUT(HDRP(ptr), PACK(now_size, 0));
        PUT(FTRP(ptr), PACK(now_size, 0));
        PUT_ADD(SUCC(list_prev), ptr);
        PUT_ADD(PRED(ptr), list_prev);
        PUT_ADD(SUCC(ptr), GOTO_SUCC(list_next));
        if (GOTO_SUCC(list_next))
        {
            PUT_ADD(PRED(GOTO_SUCC(list_next)), ptr);
        }
    }
    else if (!prev_allocated && next_allocated)
    {
        // 이전꺼랑 합치면 링크드리스트는 업데이트 없음.
        now_size += GET_SIZE(GET_PREV_HDR(ptr));
        PUT(FTRP(ptr), PACK(now_size, 0));
        PUT(GET_PREV_HDR(ptr), PACK(now_size, 0));
        ptr = PREV_BLKP(ptr);
    }
    else if (!prev_allocated && !next_allocated)
    {
        // 양쪽 다 합치면 SUCC(list_prev) = SUCC(list_next), PREV(SUCC(list_next)) = list_prev
        now_size += GET_SIZE(GET_PREV_FTR(ptr)) + GET_SIZE(GET_NEXT_HDR(ptr));
        pointer_t *list_prev = (pointer_t *)PREV_BLKP(ptr);
        pointer_t *list_next = (pointer_t *)NEXT_BLKP(ptr);
        PUT(GET_PREV_HDR(ptr), PACK(now_size, 0));
        PUT(GET_NEXT_FTR(ptr), PACK(now_size, 0));
        PUT_ADD(SUCC(list_prev), GOTO_SUCC(list_next));
        if (GOTO_SUCC(list_next))
        {
            PUT_ADD(PRED(GOTO_SUCC(list_next)), list_prev);
        }
        ptr = PREV_BLKP(ptr);
    }
    return ptr;
}

static void *extend_heap(size_t words)
{
    size_t size = words % 2 ? (words + 1) * WSIZE : words * WSIZE;
    char *start_p = mem_sbrk(size);
    if (start_p == (void *)-1)
    {
        return NULL;
    }
    // 새롭게 할당되는 블록도 free니까 predecessor랑 successor가 있음
    // coalesce가 다 해줄거임
    PUT(HDRP(start_p), PACK(size, 0));
    PUT(FTRP(start_p), PACK(size, 0));
    PUT(GET_NEXT_HDR(start_p), PACK(0, 1)); // 마지막 워드에 에필로그 넣어주기
    return coalesce(start_p);
}

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    char *heap_startp = mem_sbrk(6 * WSIZE);
    if (heap_startp == (void *)-1)
    {
        return -1;
    }
    PUT(heap_startp, 0);                              // 패딩
    PUT(heap_startp + WSIZE, PACK(2 * DSIZE, 1));     // 프롤로그 헤더
    PUT_ADD(heap_startp + WSIZE * 2, NULL);           // 힙의 첫번째 predecessor = root 역할
    PUT_ADD(heap_startp + WSIZE * 3, NULL);           // successor
    PUT(heap_startp + WSIZE * 4, PACK(2 * DSIZE, 1)); // 프롤로그 푸터
    PUT(heap_startp + WSIZE * 5, PACK(0, 1));         // 에필로그
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
    {
        return -1;
    }
    return 0;
}

static void *find_fit(size_t asize)
{
    pointer_t *find_p = (pointer_t *)(mem_heap_lo() + DSIZE);
    while (1)
    {
        find_p = (pointer_t *)GOTO_SUCC(find_p);
        if (!find_p)
        {
            return NULL;
        }
        if (!GET_ALLOC(HDRP(find_p)) && GET_SIZE(HDRP(find_p)) >= asize)
        {
            return (void *)find_p;
        }
    }
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
        // 쪼개진 뒷 블럭 링크드리스트 그 자리에 그대로 넣어주기
        // 링크드리스트에서 블록 빼고 작아진 주소값 그대로 그 자리에
        PUT_ADD(SUCC(GOTO_PRED(bp)), NEXT_BLKP(bp)); // 이전 리스트의 succ에 현재 블록 넣어주기
        PUT_ADD(PRED(NEXT_BLKP(bp)), GOTO_PRED(bp)); // 쪼개진 블록 pred에 원래의 pred
        if (!GOTO_SUCC(bp))
        {
            PUT_ADD(SUCC(NEXT_BLKP(bp)), NULL);
        }
        else
        {
            PUT_ADD(PRED(GOTO_SUCC(bp)), NEXT_BLKP(bp));
            PUT_ADD(SUCC(NEXT_BLKP(bp)), GOTO_SUCC(bp));
        }
    }
    else
    {
        PUT(HDRP(bp), PACK(free_size, 1));
        PUT(FTRP(bp), PACK(free_size, 1));
        // 링크드리스트에서 할당하는 블록 빼주기
        if (!GOTO_SUCC(bp))
        {
            PUT_ADD(SUCC(GOTO_PRED(bp)), NULL);
        }
        else
        {
            PUT_ADD(PRED(GOTO_SUCC(bp)), GOTO_PRED(bp));
            PUT_ADD(SUCC(GOTO_PRED(bp)), GOTO_SUCC(bp));
        }
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
