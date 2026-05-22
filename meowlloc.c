#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include "macros.c"

// NOTE: for now ill be assuming that 16 bytes (== sizeof(Meowlloc_HeaderBlock)) for alignment will always be sufficient


typedef struct Meowlloc_HeaderBlock Meowlloc_HeaderBlock;
struct Meowlloc_HeaderBlock {
    union {
        Meowlloc_HeaderBlock *previous;
        bool isReserved;
    };
    size_t size;
};


// NOTE: if header.isReserved == false
typedef struct Meowlloc_HeaderBlockFree Meowlloc_HeaderBlockFree;
struct Meowlloc_HeaderBlockFree {
    Meowlloc_HeaderBlock header;

    Meowlloc_HeaderBlockFree *lhs;
    Meowlloc_HeaderBlockFree *rhs;
};


// NOTE: if header.size == 0
typedef struct {
    Meowlloc_HeaderBlock header;

    Meowlloc_HeaderBlockFree *initial;
    Meowlloc_HeaderBlockFree *_padding; // NOTE: probably not necessary
} Meowlloc_HeaderBlockFinal;

bool meowlloc_internal_isFinal(Meowlloc_HeaderBlock *block) {
    return block->size == 0;
}




void meowlloc_internal_initializeArena(char *arenaBytes, size_t len) {
    assert(len % sizeof(Meowlloc_HeaderBlock) == 0);

    assert(len >= (sizeof(Meowlloc_HeaderBlockFree) + sizeof(Meowlloc_HeaderBlockFinal)));

    Meowlloc_HeaderBlockFree *block_initial = (Meowlloc_HeaderBlockFree *)arenaBytes;
    *block_initial = (Meowlloc_HeaderBlockFree){
        .header = {
            .isReserved = false,
            .size = 15, // TODO: calculate
        },
        .lhs = null,
        .rhs = null,
    };

    // TODO: insert block_initial into the tree

    Meowlloc_HeaderBlockFinal *block_final = (Meowlloc_HeaderBlockFinal *)(arenaBytes - sizeof(Meowlloc_HeaderBlockFinal));
    *block_final = (Meowlloc_HeaderBlockFinal){
        .header = {
            .isReserved = false,
            .size = 0
        },
        .initial = block_initial,
    };
}
