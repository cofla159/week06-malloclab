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
    "8team",
    /* First member's full name */
    "Hwang Chaerim",
    /* First member's email address */
    "chaerim@jungle.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};
/* Basic constants and macros */
#define WSIZE 4             /* Word and header/footer size (bytes) */
#define DSIZE 8             /* Double word size (bytes) */
#define CHUNKSIZE (1 << 12) /* Extend heap by this amount (bytes) */
#define __POINTER_TYPE__ unsigned int *
typedef __POINTER_TYPE__ pointer_t;

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p) (*(pointer_t)(p))
#define PUT(p, val) (*(pointer_t)(p) = (unsigned int)(val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp)-WSIZE)
#define NEXT(bp) ((char *)(bp)-WSIZE)
/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE)))
/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

static pointer_t segregated_free_list[30];

static int find_free_index(size_t adjust_size)
{
    for (int i = 0; i < 30; i++)
    {
        if (adjust_size <= (int)(1 << (i + 3)))
        {
            if (segregated_free_list[i] == NULL)
            {
                continue;
            }
            return i;
        }
    }
    return 0;
}

static int find_index(size_t adjust_size)
{
    for (int i = 0; i < 30; i++)
    {
        if (adjust_size == 1 << (i + 3))
        {
            return i;
        }
    }
}
static void insert_free_list(int index, void *bp)
{
    PUT(NEXT(bp), segregated_free_list[index]);
    segregated_free_list[index] = bp;
}

static void init_header(void *bp, size_t size)
{
    PUT(HDRP(bp), PACK(size, 1));
}

static pointer_t pop_free_list(int index)
{
    pointer_t allocated_bp = segregated_free_list[index];
    pointer_t next_bp = GET(NEXT(segregated_free_list[index]));
    init_header(allocated_bp, 1);
    segregated_free_list[index] = next_bp;
    return allocated_bp;
}

static void *coalesce(void *ptr, int idx, int size)
{
    // 링크드리스트 타면서 ptr-size나 ptr+size 발견하면
    // 삭제하고 idx+1 리스트에 넣고 break
    // 끝까지 봤는데 없으면 지금 블록만 idx에 넣기
    pointer_t linked_root = segregated_free_list[idx];
    while (1)
    {
        if (linked_root == NULL || (pointer_t)GET(NEXT(linked_root)) == NULL)
        {
            insert_free_list(idx, ptr);
            return ptr;
        }
        if ((pointer_t)GET(NEXT(linked_root)) == (pointer_t)(ptr - size))
        {
            GET(NEXT(linked_root)) = GET(NEXT(ptr));
            insert_free_list(idx + 1, ptr - size);
            return ptr;
        }
        if ((pointer_t)GET(NEXT(linked_root)) == (pointer_t)(ptr + size))
        {
            GET(NEXT(linked_root)) = GET(NEXT(ptr));
            insert_free_list(idx + 1, ptr);
            return ptr;
        }
    }
}

static void *extend_heap(size_t words)
{
    size_t size = words % 2 ? (words + 1) * WSIZE : words * WSIZE;
    pointer_t bp = mem_sbrk(size);
    int index = find_index(size);
    if (bp == (void *)-1)
    {
        return NULL;
    }
    PUT(mem_heap_hi() - 3, PACK(0, 1));
    return coalesce(bp, index, CHUNKSIZE);
}

int mm_init(void)
{
    pointer_t start_addr = mem_sbrk(2 * WSIZE);
    if (start_addr == (void *)-1)
    {
        return -1;
    }
    for (int i = 0; i < 30; i++)
    {
        segregated_free_list[i] = NULL;
    }
    PUT(start_addr, 0);
    PUT(start_addr + 1, PACK(1, 0));
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
    {
        return -1;
    }
    return 0;
}

size_t define_adjust_size(size_t size)
{
    int n = 0;
    int flag = 0;
    size += WSIZE;
    while (1)
    {
        if (size == 1)
        {
            if (flag == 0)
            {
                return 1 << n;
            }
            break;
        }
        flag += size % 2;
        size /= 2;
        n += 1;
    }
    return 1 << (n + 1);
}
void *divide_block(int index, int adjust_size)
{
    unsigned int *previous_block_bp;
    unsigned int *next_block_bp;
    while (1)
    {
        previous_block_bp = pop_free_list(index);
        next_block_bp = (unsigned int *)((char *)previous_block_bp + (int)(1 << (index + 3)));
        insert_free_list(index - 1, next_block_bp);
        if ((int)(1 << (index + 3)) == adjust_size)
        {
            return previous_block_bp;
        }
        insert_free_list(index - 1, previous_block_bp);
        index -= 1;
    }
    // while (1)
    // {
    //     char *popped_block = (char *)pop_free_list(idx);
    //     size_t size = GET_SIZE(HDRP(popped_block));
    //     PUT(popped_block + size / 2, PACK(size / 2, 0));
    //     insert_free_list(idx - 1, popped_block + size / 2);
    //     if (size / 2 == asize)
    //     {
    //         PUT(HDRP(bp), PACK(size, 1))
    //         return popped_block;
    //     }
    //     insert_free_list(idx - 1, popped_block);
    // }
}

void *mm_malloc(size_t size)
{
    if (size == 0)
    {
        return NULL;
    }
    size_t adjust_size = define_adjust_size(size);
    int index = find_free_index(adjust_size);
    if (!segregated_free_list[index])
    {
        size_t extend_size = MAX(adjust_size, CHUNKSIZE);
        pointer_t bp = extend_heap((extend_size / WSIZE));
        if (bp == (void *)-1)
        {
            return NULL;
        }
        return divide_block(index, adjust_size);
    }
    if (adjust_size == (int)(1 << (index + 3)))
    {
        return pop_free_list(index);
    }
    return divide_block(index, adjust_size);
}

void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));
    int index = find_index(size);
    coalesce(ptr, index, size);
}

void *mm_realloc(void *ptr, size_t size)
{
    if (size <= 0)
    {
        mm_free(ptr);
        return 0;
    }
    if (ptr == NULL)
    {
        return mm_malloc(size);
    }
    void *newp = mm_malloc(size);
    if (newp == NULL)
    {
        return 0;
    }
    size_t oldsize = GET_SIZE(HDRP(ptr));
    if (size < oldsize)
    {
        oldsize = size;
    }
    memcpy(newp, ptr, oldsize);
    mm_free(ptr);
    return newp;
}