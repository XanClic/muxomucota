/************************************************************************
 * Copyright (C) 2013 by Max Reitz                                      *
 *                                                                      *
 * This file is part of µxoµcota.                                       *
 *                                                                      *
 * µxoµcota  is free  software:  you can  redistribute it and/or modify *
 * it under the terms of the GNU General Public License as published by *
 * the  Free Software Foundation,  either version 3 of the License,  or *
 * (at your option) any later version.                                  *
 *                                                                      *
 * µxoµcota  is  distributed  in the  hope  that  it  will  be  useful, *
 * but  WITHOUT  ANY  WARRANTY;  without even the  implied warranty  of *
 * MERCHANTABILITY  or  FITNESS  FOR  A  PARTICULAR  PURPOSE.  See  the *
 * GNU General Public License for more details.                         *
 *                                                                      *
 * You should have  received a copy of the  GNU  General Public License *
 * along with µxoµcota.  If not, see <http://www.gnu.org/licenses/>.    *
 ************************************************************************/

#include <compiler.h>
#include <digest.h>
#include <kassert.h>
#include <kmalloc.h>
#include <lock.h>
#include <pmm.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <vmem.h>


// FIXME
static lock_t alloc_lock = LOCK_INITIALIZER;


#define CHECK_INTEGRITY

#ifdef CHECK_INTEGRITY
#define check(x) kassert_print(check_integrity(*(x)), "%p corrupted", x)
#define update(x) write_checksum(*(x))

#define from_next(x) ((typeof(*x))((uintptr_t)(x) - offsetof(typeof(**x), next)))
#else
#define check(x)
#define update(x)
#endif


struct pool;

struct block
{
    struct block *next;
    struct pool *pool;
    size_t size; // size of comple block (including metadata)
#ifdef CHECK_INTEGRITY
    DIGESTIFY
#endif
};

#define BLOCK_META_SIZE ((sizeof(struct block) + 0xf) & ~0xf)

struct pool
{
    struct pool *next;
    struct block *free_blocks;
    size_t max_free_size;
    size_t size;
#ifdef CHECK_INTEGRITY
    DIGESTIFY
#endif
};

#define POOL_META_SIZE ((sizeof(struct pool) + 0xf) & ~0xf)

static struct pool *pools;


static void join_memory(struct pool *pool)
{
    check(pool);

    struct block *block = pool->free_blocks;

    if (unlikely(!block))
        return;

    while (block->next)
    {
        check(block);
        kassert_print(block->pool == pool, "Block %p does not belong to pool %p (but to %p)", block, pool, block->pool);

        kassert_print((uintptr_t)block + block->size > (uintptr_t)block->next, "Overlapping heap blocks (%p + %p > %p; in block %p)", block, block->size, block->next, block->pool);

        if (unlikely((uintptr_t)block + block->size == (uintptr_t)block->next))
        {
            check(block->next);
            kassert_print(block->next->pool == pool, "Block %p does not belong to pool %p (but to %p)", block->next, pool, block->next->pool);

            block->size += block->next->size;
            block->next  = block->next->next;

            update(block);

            if (block->size > pool->max_free_size)
            {
                pool->max_free_size = block->size;
                update(pool);
            }
        }
        else
            block = block->next;
    }
}


static void alloc_pool(size_t sz)
{
    kassert(sz >= POOL_META_SIZE + BLOCK_META_SIZE);

    sz = (sz + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    int frame_count = sz >> PAGE_SHIFT;
    uintptr_t pageframes[frame_count];

    // Avoid external fragmentation
    for (int i = 0; i < frame_count; i++)
        pageframes[i] = pmm_alloc();

    struct pool *pool = kernel_map_nc(pageframes, frame_count);
    pool->size = sz;
    pool->max_free_size = sz - POOL_META_SIZE;
    pool->free_blocks = (struct block *)((uintptr_t)pool + POOL_META_SIZE);
    pool->free_blocks->next = NULL;
    pool->free_blocks->pool = pool;
    pool->free_blocks->size = sz - POOL_META_SIZE;

    struct pool **fp;
    for (fp = &pools; *fp && (*fp < pool); fp = &(*fp)->next)
        check(*fp);

    pool->next = *fp;
    *fp = pool;
    if (fp != &pools)
        update(from_next(fp));

    update(pool);
    update(pool->free_blocks);


    bool current_pool_fragmented = false;

    pool = pools;
    while (pool->next)
    {
        check(pool);

        kassert_print((uintptr_t)pool + pool->size <= (uintptr_t)pool->next, "Overlapping heap pools (%p + %p > %p)", pool, pool->size, pool->next);

        if (unlikely((uintptr_t)pool + pool->size == (uintptr_t)pool->next))
        {
            struct pool *next = pool->next;
            check(next);
            pool->size += next->size;
            pool->next  = next->next;
            update(pool);

            struct block *hdr = (struct block *)next;
            hdr->next = next->free_blocks;
            hdr->size = POOL_META_SIZE;
            update(hdr);

            for (struct block *b = hdr; b; b = b->next)
            {
                check(b);
                b->pool = pool;
                update(b);
            }

            struct block **bp;
            for (bp = &pool->free_blocks; *bp; bp = &(*bp)->next)
                check(*bp);

            *bp = hdr;
            if (bp != &pool->free_blocks)
                update(from_next(bp));
            else
                update(pool);

            current_pool_fragmented = true;
        }
        else
        {
            if (unlikely(current_pool_fragmented))
            {
                join_memory(pool);
                current_pool_fragmented = false;
            }

            pool = pool->next;
        }
    }
}


static void update_max_free_size(struct pool *pool)
{
    size_t max = 0;

    check(pool);

    for (struct block *block = pool->free_blocks; block; block = block->next)
    {
        check(block);
        if (block->size > max)
            max = block->size;
    }

    pool->max_free_size = max;
    update(pool);
}


static struct block *alloc_block(struct pool *pool, size_t sz)
{
    struct block **fb = &pool->free_blocks;

    // There is always a block big enough (verified through max_free_size)

    while (*fb)
    {
        check(*fb);

        if ((*fb)->size >= sz)
        {
            /*          _         _
             *         |  HDR      |
             *         |  [block] _| - sz
             * osize - |  HDR
             *         |_ [fill]
             */

            struct block *t = *fb;

            bool up_mfs = t->size == pool->max_free_size;

            if (t->size - sz < BLOCK_META_SIZE + 16)
                *fb = t->next;
            else
            {
                size_t osize = t->size;
                *fb = (struct block *)((uintptr_t)t + sz);
                (*fb)->size = osize - sz;
                (*fb)->pool = pool;
                (*fb)->next = t->next;

                update(*fb);

                t->size = sz;
                update(t);
            }

            if (fb != &pool->free_blocks)
                update(from_next(fb));
            else
                update(pool);

            if (up_mfs)
                update_max_free_size(pool);

            return t;
        }

        fb = &(*fb)->next;
    }

    kassert_print(0, "No fitting block found in heap pool %p (%p searched, %p promised)", pool, sz, pool->max_free_size);
}


void *kmalloc(size_t size)
{
    if (!size)
        size = 1;

    size = ((size + 0xf) & ~0xf) + BLOCK_META_SIZE;

    struct pool *pool;

    lock(&alloc_lock);

    do
    {
        for (pool = pools; pool && (pool->max_free_size < size); pool = pool->next)
            check(pool);

        if (!pool)
            alloc_pool(size + POOL_META_SIZE);
    } while (!pool);

    struct block *block = alloc_block(pool, size);
    check(block);

    unlock(&alloc_lock);

    return (void *)((uintptr_t)block + BLOCK_META_SIZE);
}


void kfree(void *ptr)
{
    if (ptr == NULL)
        return;


    struct block *block = (struct block *)((uintptr_t)ptr - BLOCK_META_SIZE);
    check(block);

    lock(&alloc_lock);

    struct pool *pool = block->pool;
    check(pool);

    struct block **fb;
    for (fb = &pool->free_blocks; *fb && (*fb < block); fb = &(*fb)->next)
        check(*fb);

    kassert_print(*fb != block, "Double free (%p)", block);

    block->next = *fb;
    *fb = block;
    if (fb != &pool->free_blocks)
        update(from_next(fb));
    else
        update(pool);

    join_memory(pool);

    unlock(&alloc_lock);
}
