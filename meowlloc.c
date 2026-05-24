#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <sys/mman.h>

#include "internal.h"
#include "meowlloc.h"
#include "macros.h"
#include "rbtree.c"

// NOTE: for now ill be assuming that 16 bytes (== sizeof(Meowlloc_HeaderBlock)) for alignment will always be sufficient



Meowlloc_HeaderBlockFree *MEOWLLOC_TREE = null;

Meowlloc_HeaderBlock *meowlloc_internal_getNextBlock(Meowlloc_HeaderBlock *block) {
    block += block->size / sizeof(Meowlloc_HeaderBlock);
    return block;
}

bool meowlloc_internal_isFinal(Meowlloc_HeaderBlock *block) {
    return block->size == 0;
}

bool meowlloc_internal_isSingleBlock(Meowlloc_HeaderBlock *block) {
    Meowlloc_HeaderBlock *next = meowlloc_internal_getNextBlock(block);
    return meowlloc_internal_isFinal(next) && (Meowlloc_HeaderBlock *)((Meowlloc_HeaderBlockFinal *)next)->initial == block;
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

    meowlloc_rbtree_insertBlock(&MEOWLLOC_TREE, block_initial);

    Meowlloc_HeaderBlockFinal *block_final = (Meowlloc_HeaderBlockFinal *)(arenaBytes + len - sizeof(Meowlloc_HeaderBlockFinal));
    *block_final = (Meowlloc_HeaderBlockFinal){
        .header = {
            .isReserved = false,
            .size = 0
        },
        .initial = block_initial,
    };
}

void meowlloc_internal_newArena(size_t size) {
    // TODO: validate multiple of page size

    char *arena = mmap(null, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    printf("MMAP %p\n", arena);

    meowlloc_internal_initializeArena(arena, size);
}

size_t meowlloc_internal_sizeToHeaders(size_t size) {
    size_t n = size / sizeof(Meowlloc_HeaderBlock);
    size_t r = size % sizeof(Meowlloc_HeaderBlock);
    if(r != 0) n += 1;
    return n;
}







void *meowlloc_allocate(size_t size) {
    Meowlloc_RbtreeGeneration gen = meowlloc_rbtree_findBestBlock(MEOWLLOC_TREE, size);

    if(gen.this == null) {
        // TODO: error handling?

        // TODO: whole number of pages greater or equal to size, all things considered
        meowlloc_internal_newArena(4096);

        return meowlloc_allocate(size);
    }

    meowlloc_rbtree_removeBlock(&MEOWLLOC_TREE, gen.this, gen);

    // NOTE: we have enough space for an extra free header which can store 16 bytes
    if(gen.this->header.size >= (size + sizeof(Meowlloc_HeaderBlockFree))) {
        size_t lhsContentHeaderCount = meowlloc_internal_sizeToHeaders(size);
        Meowlloc_HeaderBlockFree *rhs = (Meowlloc_HeaderBlockFree *)(&gen.this->header + 1 + lhsContentHeaderCount);

        rhs->header.isReserved = false;
        rhs->header.size = gen.this->header.size - (lhsContentHeaderCount * sizeof(Meowlloc_HeaderBlock) + 1 * sizeof(Meowlloc_HeaderBlock));
        meowlloc_rbtree_insertBlock(&MEOWLLOC_TREE, rhs);

        gen.this->header.size = lhsContentHeaderCount * sizeof(Meowlloc_HeaderBlock);
    }

    gen.this->header.previous = (Meowlloc_HeaderBlock *)gen.this;

    Meowlloc_HeaderBlock *result = (Meowlloc_HeaderBlock *)gen.this;
    result += 1;
    memset(result, 0, gen.this->header.size);

    return result;
}

void *meowlloc_reallocate(void *old, size_t newSize) {
    Meowlloc_HeaderBlock *block = old;
    block -= 1; // NOTE: user data starts exactly after the end of the header

    Meowlloc_HeaderBlock *nextBlock = block + (block->size / sizeof(Meowlloc_HeaderBlock));
    bool canMergeWithNext = (nextBlock->size != 0) && !nextBlock->isReserved;

    if(canMergeWithNext) {
        // TODO: merge with next lol
        return null;
    }
    else {
        void *new = meowlloc_allocate(newSize);
        memcpy(new, old, newSize < block->size ? newSize : block->size);
        meowlloc_free(old);
        return new;
    }
}

void meowlloc_free(void *old) {
    Meowlloc_HeaderBlock *block = old;
    block -= 1; // NOTE: user data starts exactly after the end of the header

    Meowlloc_HeaderBlock *previousBlock = block->previous;
    bool canMergeWithPrevious = (previousBlock != block && !previousBlock->isReserved);

    Meowlloc_HeaderBlock *nextBlock = meowlloc_internal_getNextBlock(block);
    bool canMergeWithNext = !meowlloc_internal_isFinal(nextBlock) && !nextBlock->isReserved;

    // TODO: merge if possible, return to the tree

    if(meowlloc_internal_isSingleBlock(block)) {
        // TODO: if the tree has more than a single node, munmap the arena
        return;
    }

    return;
}
