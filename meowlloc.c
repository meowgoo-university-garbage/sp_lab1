#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>

#include "macros.c"

// NOTE: for now ill be assuming that 16 bytes (== sizeof(Meowlloc_HeaderBlock)) for alignment will always be sufficient


#define MEOWLLOC_SIZEMASK (~(0b1111u))

#define MEOWLLOC_RBTREE_RED (0b1u)
#define MEOWLLOC_RBTREE_BLACK (0b0u)

#define MEOWLLOC_RBTREE_ISRED(block)   ((block) != null && ((block)->header.size & MEOWLLOC_RBTREE_RED) == (MEOWLLOC_RBTREE_RED))
#define MEOWLLOC_RBTREE_ISBLACK(block) ((block) == null || ((block)->header.size & MEOWLLOC_RBTREE_RED) == (MEOWLLOC_RBTREE_BLACK))


typedef struct Meowlloc_HeaderBlock Meowlloc_HeaderBlock;
struct Meowlloc_HeaderBlock {
    union {
        Meowlloc_HeaderBlock *previous;
        bool isReserved;
    };
    size_t size; // NOTE: bit 0 stores 0 == black, 1 == red for the rbtree
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
            .size = MEOWLLOC_SIZEMASK, // TODO: calculate
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






// NOTE: Red-Black tree algorithms taken mostly from here: https://ru.wikipedia.org/wiki/%D0%9A%D1%80%D0%B0%D1%81%D0%BD%D0%BE-%D1%87%D1%91%D1%80%D0%BD%D0%BE%D0%B5_%D0%B4%D0%B5%D1%80%D0%B5%D0%B2%D0%BE

// NOTE: this searches for the smallest fitting block, but this might not be optimal wrt fragmentation idk
Meowlloc_HeaderBlockFree *meowlloc_rbtree_findBestBlock(Meowlloc_HeaderBlockFree *node, size_t requestedSize) {
    if(node == null) return null;

    if(node->lhs != null && node->lhs->header.size >= requestedSize) {
        return meowlloc_rbtree_findBestBlock(node->lhs, requestedSize);
    }

    if(node->header.size >= requestedSize) {
        return node;
    }

    if(node->rhs != null && node->header.size < requestedSize) {
        return meowlloc_rbtree_findBestBlock(node->rhs, requestedSize);
    }

    return null;
}




typedef struct {
    Meowlloc_HeaderBlockFree *grandgrandfather;
    Meowlloc_HeaderBlockFree *grandfather;
    Meowlloc_HeaderBlockFree *uncle;
    Meowlloc_HeaderBlockFree *parent;
    Meowlloc_HeaderBlockFree *this;
} Meowlloc_RbtreeGeneration;

Meowlloc_HeaderBlockFree **Meowlloc_rbtree_getChildPointer(Meowlloc_HeaderBlockFree *root, Meowlloc_HeaderBlockFree *child) {
    return root->lhs == child ? &root->lhs : &root->rhs;
}

Meowlloc_RbtreeGeneration meowlloc_rbtree_rotateGeneration(Meowlloc_RbtreeGeneration gen, Meowlloc_HeaderBlockFree *new) {
    gen.grandgrandfather = gen.grandfather;
    gen.grandfather = gen.parent;
    gen.parent = gen.this;
    gen.this = new;

    gen.uncle = null;

    if(gen.grandfather != null) {
        gen.uncle = (gen.grandfather->lhs == gen.parent) ? gen.grandfather->rhs : gen.grandfather->lhs;
    }

    return gen;
}

Meowlloc_RbtreeGeneration meowlloc_rbtree_insertBlockRec(Meowlloc_HeaderBlockFree *root, Meowlloc_HeaderBlockFree *block, Meowlloc_RbtreeGeneration gen) {
    if(root == null) {
        gen = meowlloc_rbtree_rotateGeneration(gen, block);
        return gen;
    }

    Meowlloc_HeaderBlockFree **candidatePointer = (block->header.size > root->header.size) ? &root->rhs : &root->lhs;
    Meowlloc_HeaderBlockFree *candidate = *candidatePointer;

    if(candidate == null) {
        *candidatePointer = block;
        gen = meowlloc_rbtree_rotateGeneration(gen, block);
        return gen;
    }
    else {
        gen = meowlloc_rbtree_rotateGeneration(gen, candidate);
        return meowlloc_rbtree_insertBlockRec(candidate, block, gen);
    }
}

Meowlloc_RbtreeGeneration meowlloc_rbtree_getGeneration(Meowlloc_HeaderBlockFree *root, Meowlloc_HeaderBlockFree *needle, Meowlloc_RbtreeGeneration gen) {
    gen = meowlloc_rbtree_rotateGeneration(gen, root);
    if(root == needle) return gen;

    if(needle->header.size > root->header.size) {
        return meowlloc_rbtree_getGeneration(root->rhs, needle, gen);
    }
    else {
        return meowlloc_rbtree_getGeneration(root->lhs, needle, gen);
    }
}

void meowlloc_rbtree_printNode(Meowlloc_HeaderBlockFree *node) {
    if(node == null) {
        printf("null");
        return;
    }

    printf("%d%c (", node->header.size & ~MEOWLLOC_RBTREE_RED, MEOWLLOC_RBTREE_ISRED(node) ? 'r' : 'b');
    meowlloc_rbtree_printNode(node->lhs);
    printf(", ");
    meowlloc_rbtree_printNode(node->rhs);
    printf(")");
}

void meowlloc_rbtree_insertBlock(Meowlloc_HeaderBlockFree **tree, Meowlloc_HeaderBlockFree *block) {
    Meowlloc_RbtreeGeneration gen = meowlloc_rbtree_insertBlockRec(*tree, block, (Meowlloc_RbtreeGeneration){ .this = *tree });
    gen.this->header.size |= MEOWLLOC_RBTREE_RED;

    do {
        if(gen.parent == null) {
            *tree = gen.this;
            gen.this->header.size &= ~MEOWLLOC_RBTREE_RED;
            break;
        }

        if(MEOWLLOC_RBTREE_ISBLACK(gen.parent)) {
            break;
        }

        if(MEOWLLOC_RBTREE_ISRED(gen.uncle)) {
            gen.parent->header.size &= ~MEOWLLOC_RBTREE_RED;
            gen.uncle->header.size  &= ~MEOWLLOC_RBTREE_RED;
            gen.grandfather->header.size |= MEOWLLOC_RBTREE_RED;

            // TODO: calculate grandfather gen
            gen = meowlloc_rbtree_getGeneration(*tree, gen.grandfather, (Meowlloc_RbtreeGeneration){0});

            continue;
        }

        if(0) {}
        else if(gen.this == gen.parent->rhs && gen.parent == gen.grandfather->lhs) {
            // ROTATE LEFT
            gen.parent->rhs = gen.this->rhs;
            gen.this->lhs = gen.parent;
            gen.grandfather->lhs = gen.this;

            gen.this = gen.this->lhs;
            gen.parent = gen.grandfather->lhs;
        }
        else if(gen.this == gen.parent->lhs && gen.parent == gen.grandfather->rhs) {
            // ROTATE RIGHT
            gen.parent->lhs = gen.this->lhs;
            gen.this->rhs = gen.parent;
            gen.grandfather->rhs = gen.this;

            gen.this = gen.this->rhs;
            gen.parent = gen.grandfather->rhs;
        }


        gen.parent->header.size &= ~MEOWLLOC_RBTREE_RED;
        gen.grandfather->header.size |= MEOWLLOC_RBTREE_RED;

        if(gen.this == gen.parent->lhs && gen.parent == gen.grandfather->lhs) {
            // ROTATE RIGHT
            gen.grandfather->lhs = gen.parent->rhs;
            gen.parent->rhs = gen.grandfather;

            Meowlloc_HeaderBlockFree **dst = gen.grandgrandfather == null ? tree : Meowlloc_rbtree_getChildPointer(gen.grandgrandfather, gen.grandfather);
            *dst = gen.parent;
        }
        else {
            // ROTATE LEFT
            gen.grandfather->rhs = gen.parent->lhs;
            gen.parent->lhs = gen.grandfather;

            Meowlloc_HeaderBlockFree **dst = gen.grandgrandfather == null ? tree : Meowlloc_rbtree_getChildPointer(gen.grandgrandfather, gen.grandfather);
            *dst = gen.parent;
        }


        break;
    } while(true);
}
