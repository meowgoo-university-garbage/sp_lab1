#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "internal.h"
#include "meowlloc.h"
#include "macros.h"
#include "rbtree.c"
#include "kernel.h"


#define SZ(x) ((x) & MEOWLLOC_SIZEMASK)


Meowlloc_HeaderBlockFree *MEOWLLOC_TREE = null;

Meowlloc_HeaderBlock *meowlloc_internal_getNextBlock(Meowlloc_HeaderBlock *block) {
    // NOTE: blocks are guaranteed to have size to hold exactly an integer amount of headers
    block += 1 + (SZ(block->size) / sizeof(Meowlloc_HeaderBlock));
    return block;
}

bool meowlloc_internal_isFinal(Meowlloc_HeaderBlock *block) {
    // NOTE: the only zero-sized block is the final block created in meowlloc_internal_initializeArena.
    // The only other time where a zero-sized block creation is allowed is during a meowlloc_reallocate,
    // which immediately merges the zero-sized block
    return SZ(block->size) == 0;
}

bool meowlloc_internal_isSingleBlock(Meowlloc_HeaderBlock *block) {
    Meowlloc_HeaderBlock *next = meowlloc_internal_getNextBlock(block);
    return meowlloc_internal_isFinal(next) && ((Meowlloc_HeaderBlockFinal *)next)->initial == block;
}

size_t meowlloc_internal_sizeToHeaders(size_t size) {
    size_t n = size / sizeof(Meowlloc_HeaderBlock);
    size_t r = size % sizeof(Meowlloc_HeaderBlock);
    if(r != 0) n += 1;
    return n;
}

size_t meowlloc_internal_sizeToPages(size_t size) {
    size_t n = size / MEOWLLOC_CONFIG_PAGESIZE;
    size_t r = size % MEOWLLOC_CONFIG_PAGESIZE;
    if(r != 0) n += 1;
    return n;
}

void meowlloc_internal_releasePages(Meowlloc_HeaderBlockFree *block) {
    // NOTE: casting uintptr_t to size_t is probably not the most consistent thing,
    // but it's not that bad, i just dont want to duplicate sizeToPages
    size_t lhs = (size_t)((uintptr_t)(block + 1));
    size_t rhs = (size_t)(lhs + SZ(block->header.size));

    // NOTE: we need page alignment
    size_t nlhs = meowlloc_internal_sizeToPages(lhs) * MEOWLLOC_CONFIG_PAGESIZE;
    size_t nrhs = meowlloc_internal_sizeToPages(rhs) * MEOWLLOC_CONFIG_PAGESIZE;

    if(nrhs > rhs) nrhs -= MEOWLLOC_CONFIG_PAGESIZE;
    if(nlhs >= nrhs) return;

    size_t difference = nrhs - nlhs;
    // madvise((void *)nlhs, difference, MADV_FREE);
    meowlloc_kernel_unuseMemory((void *)nlhs, difference);
}

void meowlloc_internal_initializeArena(char *arenaBytes, size_t len) {
    assert(len % sizeof(Meowlloc_HeaderBlock) == 0);
    assert(len >= (sizeof(Meowlloc_HeaderBlockFree) + sizeof(Meowlloc_HeaderBlockFinal)));

    Meowlloc_HeaderBlockFree *block_initial = (Meowlloc_HeaderBlockFree *)arenaBytes;
    *block_initial = (Meowlloc_HeaderBlockFree){
        .header = {
            .isReserved = false,
            .size = (len - (sizeof(Meowlloc_HeaderBlock) + sizeof(Meowlloc_HeaderBlockFinal))), // NOTE: HeaderBlock instead of HeaderBlockFree, since the byte difference will be provided to user upon allocation
        },
        .lhs = null,
        .rhs = null,
    };

    if(MEOWLLOC_CONFIG_RELEASEPAGES) {
        meowlloc_internal_releasePages(block_initial);
    }

    meowlloc_rbtree_insertBlock(&MEOWLLOC_TREE, block_initial);

    Meowlloc_HeaderBlockFinal *block_final = (Meowlloc_HeaderBlockFinal *)(arenaBytes + len - sizeof(Meowlloc_HeaderBlockFinal));
    *block_final = (Meowlloc_HeaderBlockFinal){
        .header = {
            .isReserved = false,
            .size = 0
        },
        .initial = &block_initial->header,
    };
}

// NOTE: returns MEOWLLOC_KERNEL_ERROR on error
char *meowlloc_internal_newArena(size_t desiredAllocationSize) {
    desiredAllocationSize += sizeof(Meowlloc_HeaderBlock) + sizeof(Meowlloc_HeaderBlockFinal);
    size_t size = meowlloc_internal_sizeToPages(desiredAllocationSize) * MEOWLLOC_CONFIG_PAGESIZE;
    if(size < MEOWLLOC_CONFIG_ARENASIZE) size = MEOWLLOC_CONFIG_ARENASIZE;
    assert(size % MEOWLLOC_CONFIG_PAGESIZE == 0);

    // char *arena = mmap(null, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    char *arena = meowlloc_kernel_acquireMemory(size);
    if(arena == MEOWLLOC_KERNEL_ERROR) return arena;

    meowlloc_internal_initializeArena(arena, size);

    return arena;
}

// NOTE: MUST NOT merge blocks that are currently in the tree (red/black bit)
void meowlloc_internal_mergeBlocks(Meowlloc_HeaderBlock *lhs, Meowlloc_HeaderBlock *rhs) {
    Meowlloc_HeaderBlock *nextBlock = meowlloc_internal_getNextBlock(lhs);
    assert(nextBlock == rhs);

    lhs->size += (SZ(rhs->size) + sizeof(Meowlloc_HeaderBlock));
}

// NOTE: MUST NOT split block that is currently in the tree (red/black bit)
Meowlloc_HeaderBlock *meowlloc_internal_splitBlock(Meowlloc_HeaderBlock *lhs, size_t take, bool discardLhsHeader) {
    // NOTE: used for realloc
    if(discardLhsHeader) {
        if(take <= sizeof(Meowlloc_HeaderBlock)) take = 0;
        else take -= sizeof(Meowlloc_HeaderBlock);
    }

    size_t taken = meowlloc_internal_sizeToHeaders(take);

    Meowlloc_HeaderBlock *rhs = lhs + 1 + taken;
    rhs->isReserved = false;
    rhs->size = SZ(lhs->size) - (taken * sizeof(Meowlloc_HeaderBlock) + sizeof(Meowlloc_HeaderBlock));

    lhs->size = taken * sizeof(Meowlloc_HeaderBlock);

    return rhs;
}

Meowlloc_HeaderBlock *meowlloc_internal_findFirstBlock(Meowlloc_HeaderBlock *any) {
    if(meowlloc_internal_isFinal(any)) return ((Meowlloc_HeaderBlockFinal *)any)->initial;
    return meowlloc_internal_findFirstBlock(meowlloc_internal_getNextBlock(any));
}

void meowlloc_internal_visualizeArena(Meowlloc_HeaderBlock *any) {
    if(any == null) {
        printf("[ null ]\n");
        return;
    }

    Meowlloc_HeaderBlock *this = meowlloc_internal_findFirstBlock(any);

    while(!meowlloc_internal_isFinal(this)) {
        printf("[ %c %ld ] ", this->isReserved ? 'X' : 'O', SZ(this->size));
        this = meowlloc_internal_getNextBlock(this);
    }

    printf("\n");
}







void *meowlloc_allocate(size_t size) {
    if(size == 0) return null;

    Meowlloc_RbtreeGeneration gen = meowlloc_rbtree_findBestBlock(MEOWLLOC_TREE, size);

    if(gen.this == null) {
        char *mmapResult = meowlloc_internal_newArena(size);
        if(mmapResult == MEOWLLOC_KERNEL_ERROR) {
            fprintf(stderr, "mmap failed, errno: %d\n", errno);
            return null;
        }

        return meowlloc_allocate(size);
    }

    meowlloc_rbtree_removeBlock(&MEOWLLOC_TREE, gen.this, gen);

    // NOTE: we have enough space for an extra free header which can store 16 bytes
    if(SZ(gen.this->header.size) >= (size + sizeof(Meowlloc_HeaderBlockFree))) {
        Meowlloc_HeaderBlock *rhs = meowlloc_internal_splitBlock(&gen.this->header, size, false);

        Meowlloc_HeaderBlock *nextBlock = meowlloc_internal_getNextBlock(rhs);
        if(nextBlock->isReserved) {
            nextBlock->previous = rhs;
        }

        meowlloc_rbtree_insertBlock(&MEOWLLOC_TREE, (Meowlloc_HeaderBlockFree *)rhs);
    }

    gen.this->header.previous = &gen.this->header;

    Meowlloc_HeaderBlock *result = &gen.this->header;
    result += 1;
    memset(result, MEOWLLOC_SENTINEL_ALLOCATED, SZ(gen.this->header.size));

    assert((uintptr_t)result % MEOWLLOC_CONFIG_ALIGNMENT == 0);

    return result;
}

void *meowlloc_reallocate_shrink(Meowlloc_HeaderBlock *block, size_t newSize) {
    size_t remainingSize = SZ(block->size) - newSize;

    if(remainingSize >= sizeof(Meowlloc_HeaderBlockFree)) {
        Meowlloc_HeaderBlock *rhs = meowlloc_internal_splitBlock(block, newSize, false);
        meowlloc_rbtree_insertBlock(&MEOWLLOC_TREE, (Meowlloc_HeaderBlockFree *)rhs);
    }

    return (block + 1);
}

void *meowlloc_reallocate_expand(Meowlloc_HeaderBlock *block, size_t newSize) {
    Meowlloc_HeaderBlock *nextBlock = meowlloc_internal_getNextBlock(block);
    bool canMergeWithNext = !meowlloc_internal_isFinal(nextBlock) && !nextBlock->isReserved && newSize <= SZ(block->size) + sizeof(Meowlloc_HeaderBlock) + SZ(nextBlock->size);

    if(canMergeWithNext) {
        meowlloc_rbtree_removeBlock(&MEOWLLOC_TREE, (Meowlloc_HeaderBlockFree *)nextBlock, meowlloc_rbtree_getGeneration(MEOWLLOC_TREE, (Meowlloc_HeaderBlockFree *)nextBlock, (Meowlloc_RbtreeGeneration){0}));

        size_t expandSize = newSize - SZ(block->size);
        if(sizeof(Meowlloc_HeaderBlock) + SZ(nextBlock->size) - expandSize >= sizeof(Meowlloc_HeaderBlockFree)) {
            Meowlloc_HeaderBlock *rhs = meowlloc_internal_splitBlock(nextBlock, expandSize, true);
            meowlloc_rbtree_insertBlock(&MEOWLLOC_TREE, (Meowlloc_HeaderBlockFree *)rhs);
        }

        meowlloc_internal_mergeBlocks(block, nextBlock);

        return (block + 1);
    }
    else {
        void *new = meowlloc_allocate(newSize);
        memcpy(new, (block + 1), SZ(block->size));
        meowlloc_free(block + 1);
        return new;
    }
}

void *meowlloc_reallocate(void *old, size_t newSize) {
    if(old == null) {
        return meowlloc_allocate(newSize);
    }

    if(newSize == 0) {
        meowlloc_free(old);
        return null;
    }

    Meowlloc_HeaderBlock *block = old;
    block -= 1;

    assert(block->isReserved);

    newSize = meowlloc_internal_sizeToHeaders(newSize) * sizeof(Meowlloc_HeaderBlock);

    if(newSize > SZ(block->size)) {
        return meowlloc_reallocate_expand(block, newSize);
    }
    else {
        return meowlloc_reallocate_shrink(block, newSize);
    }
}

void meowlloc_free(void *old) {
    if(old == null) return;

    Meowlloc_HeaderBlock *block = old;
    block -= 1;

    assert(block->isReserved);

    Meowlloc_HeaderBlock *previousBlock = block->previous;
    bool canMergeWithPrevious = (previousBlock != block && !previousBlock->isReserved);

    if(canMergeWithPrevious) {
        meowlloc_rbtree_removeBlock(&MEOWLLOC_TREE, (Meowlloc_HeaderBlockFree *)previousBlock, meowlloc_rbtree_getGeneration(MEOWLLOC_TREE, (Meowlloc_HeaderBlockFree *)previousBlock, (Meowlloc_RbtreeGeneration){0}));
        meowlloc_internal_mergeBlocks(previousBlock, block);
        block = previousBlock;
    }

    Meowlloc_HeaderBlock *nextBlock = meowlloc_internal_getNextBlock(block);
    bool canMergeWithNext = !meowlloc_internal_isFinal(nextBlock) && !nextBlock->isReserved;

    if(canMergeWithNext) {
        meowlloc_rbtree_removeBlock(&MEOWLLOC_TREE, (Meowlloc_HeaderBlockFree *)nextBlock, meowlloc_rbtree_getGeneration(MEOWLLOC_TREE, (Meowlloc_HeaderBlockFree *)nextBlock, (Meowlloc_RbtreeGeneration){0}));
        meowlloc_internal_mergeBlocks(block, nextBlock);
    }

    nextBlock = meowlloc_internal_getNextBlock(block);
    // NOTE: the only condition when this can ever fail is if the next block is the final block which is always free (or rather stateless), but just in case
    if(nextBlock->isReserved) {
        nextBlock->previous = block;
    }

    if(MEOWLLOC_CONFIG_RELEASEPAGES) {
        meowlloc_internal_releasePages((Meowlloc_HeaderBlockFree *)block);
    }
    else {
        memset(block + 1, MEOWLLOC_SENTINEL_FREED, SZ(block->size));
    }

    Meowlloc_HeaderBlockFree *freeBlock = (Meowlloc_HeaderBlockFree *)block;
    freeBlock->header.isReserved = false;

    // NOTE: if this arena is the only arena remaining, we do NOT want to munmap it, because the very
    // next allocation will require doing a syscall which is bad. There is one edge case - if the next
    // allocation is bigger than max(this, MEOWLLOC_CONFIG_ARENASIZE), a new arena will be allocated
    // regardless, leaving one unused arena. This is not a problem for 2 reasons: first, since that
    // allocation is so big, we will need that dormant arena for smaller allocations; second, both of
    // these arenas are still munmappble, as long as another alloc/free is performed. So the worst case
    // scenario is having 1 unused arena.
    if(meowlloc_internal_isSingleBlock(block) && MEOWLLOC_TREE != null) {
        // munmap(block, sizeof(Meowlloc_HeaderBlock) + SZ(block->size) + sizeof(Meowlloc_HeaderBlockFinal));
        meowlloc_kernel_releaseMemory(block, sizeof(Meowlloc_HeaderBlock) + SZ(block->size) + sizeof(Meowlloc_HeaderBlockFinal));
    }
    else {
        meowlloc_rbtree_insertBlock(&MEOWLLOC_TREE, freeBlock);
    }
}

#undef SZ
