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

#define HDRP(bp) ((char *)(bp)-WSIZE)                             // 헤더 주소
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)      // 푸터 주소
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)))         // (물리적) 다음 블록 주소
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE((char *)(bp)-DSIZE)) // (물리적) 이전 블록 주소
#define GET_NEXT_HDR(bp) (HDRP(NEXT_BLKP(bp)))                    // 다음 블록 헤더값 가져오기
#define GET_PREV_HDR(bp) (HDRP(PREV_BLKP(bp)))                    // 다음 블록 헤더값 가져오기
#define GET_NEXT_FTR(bp) (FTRP(NEXT_BLKP(bp)))                    // 다음 블록 헤더값 가져오기
#define GET_PREV_FTR(bp) (FTRP(PREV_BLKP(bp)))                    // 다음 블록 헤더값 가져오기
#define PRED(bp) (bp)                                             // predecessor의 위치는 bp와 일치함
#define SUCC(bp) (bp + WSIZE)
#define PRED_PRED(bp) (*GET(bp))         // predecessor의 predecessor가 가지고 있는 값
#define SUCC_SUCC(bp) (*GET(bp + WSIZE)) // successor의 successor가 가지고 있는 값

// allocate할 때는 사용하는 블록은 링크드리스트에서 빼버리고 pred의 succ->succ 가리키고 succ->pred->pred 가리켜야 함
// free할 때는 coalesce가 알아서 해줄거임

static void *find_linked_prev(void *ptr)
{
    // ptr의 prev에서 시작.
    // prev가 free면 그 주소 리턴.
    // alloc이면 하나 더 prev로 업데이트.
    // mem_heap_lo까지 갔는데 없으면 root 리턴.
}

static void *find_linked_next(void *ptr)
{
    // ptr의 next에서 시작.
    // next가 free면 그 주소 리턴.
    // alloc이면 하나 더 next로 업데이트.
    // mem_heap_hi까지 갔는데 없으면 null 리턴.
}

static void *coalesce(void *ptr)
{
    // 합치고 난 뒤에 predecessor랑 successor업데이트 해줘야 함.
    // coalesce는 물리적으로 앞뒤로 합치는거고 링크드리스트도 앞뒤로 합침 필요.
    // 넣을 위치 찾을 함수 필요. ptr의 앞쪽에 있는 free block이랑 뒤쪽에 있는 free block 찾기.
    // = list_prev, list_next

    size_t prev_allocated = GET_ALLOC(GET_PREV_FTR(ptr));
    size_t next_allocated = GET_ALLOC(GET_NEXT_HDR(ptr));
    size_t now_size = GET_SIZE(HDRP(ptr));

    if (prev_allocated && next_allocated)
    {
        // 합칠게 없으면 SUCC(list_prev) = ptr, PRED(ptr) = list_prev , PREV(next) = ptr, SUCC(ptr) = list_next
    }
    else if (prev_allocated && !next_allocated)
    {
        // 다음거랑 합치면 SUCC(list_prev) = ptr, PRED(ptr) = list_prev, SUCC(ptr) = SUCC(list_next), PRED(SUCC(list_next)) = ptr
        now_size += GET_SIZE(GET_NEXT_HDR(ptr));
        PUT(HDRP(ptr), PACK(now_size, 0));
        PUT(FTRP(ptr), PACK(now_size, 0));
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
        PUT(GET_PREV_HDR(ptr), PACK(now_size, 0));
        PUT(GET_NEXT_FTR(ptr), PACK(now_size, 0));
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
    // succ = null이고 pred 링크드리스트 마지막 블록
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
    char *heap_startp = mem_sbrk(4 * WSIZE);
    if (heap_startp == (void *)-1)
    {
        return -1;
    }
    PUT(heap_startp, 0);                              // 패딩
    PUT(heap_startp + WSIZE, PACK(2 * DSIZE, 1));     // 프롤로그 헤더
    PUT(heap_startp + WSIZE * 2, NULL);               // 힙의 첫번째 predecessor = root 역할
    PUT(heap_startp + WSIZE * 3, NULL);               // successor
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
    char *start_p; // root가 가리키는 곳에 가서 할당여부&크기 확인.

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
