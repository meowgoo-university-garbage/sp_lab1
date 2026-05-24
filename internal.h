#ifndef __MEOWLLOC_INTERNAL_H
#define __MEOWLLOC_INTERNAL_H

#define MEOWLLOC_SIZEMASK (~(0b1111u))

#define MEOWLLOC_RBTREE_RED (0b1u)
#define MEOWLLOC_RBTREE_BLACK (0b0u)

#define MEOWLLOC_RBTREE_ISRED(block)   ((block) != null && ((block)->header.size & MEOWLLOC_RBTREE_RED) == (MEOWLLOC_RBTREE_RED))
#define MEOWLLOC_RBTREE_ISBLACK(block) ((block) == null || ((block)->header.size & MEOWLLOC_RBTREE_RED) == (MEOWLLOC_RBTREE_BLACK))


typedef struct Meowlloc_HeaderBlock Meowlloc_HeaderBlock;
struct Meowlloc_HeaderBlock {
    union {
        Meowlloc_HeaderBlock *previous;
        uint64_t isReserved; // NOTE: this can't be `bool` because we need to compare all 64 bits of the pointer, while bool is only the lower 8 bits and might give false negatives
    };
    // NOTE: bit 0 stores 0 == black, 1 == red for the rbtree
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

#endif // __MEOWLLOC_INTERNAL_H
