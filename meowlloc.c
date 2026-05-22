#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

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
    return meowlloc_internal_isFinal(next) && ((Meowlloc_HeaderBlockFinal *)next)->initial == block;
}

void meowlloc_internal_initializeArena(char *arenaBytes, size_t len) {
    assert(len % sizeof(Meowlloc_HeaderBlock) == 0);

    assert(len >= (sizeof(Meowlloc_HeaderBlockFree) + sizeof(Meowlloc_HeaderBlockFinal)));

    Meowlloc_HeaderBlockFree *block_initial = (Meowlloc_HeaderBlockFree *)arenaBytes;
    *block_initial = (Meowlloc_HeaderBlockFree){
        .header = {
            .isReserved = false,
            .size = MEOWLLOC_SIZEMASK, // TODO: calculate
        },
        .lhs = null,
        .rhs = null,
    };

    meowlloc_rbtree_insertBlock(&MEOWLLOC_TREE, block_initial);

    Meowlloc_HeaderBlockFinal *block_final = (Meowlloc_HeaderBlockFinal *)(arenaBytes - sizeof(Meowlloc_HeaderBlockFinal));
    *block_final = (Meowlloc_HeaderBlockFinal){
        .header = {
            .isReserved = false,
            .size = 0
        },
        .initial = block_initial,
    };
}







void *meowlloc_allocate(size_t size) {
    return null;
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
