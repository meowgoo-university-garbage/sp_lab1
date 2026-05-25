#ifndef __MEOWLLOC_INTERNAL_H
#define __MEOWLLOC_INTERNAL_H

#include "config.h"

#define MEOWLLOC_SIZEMASK (~(0b1111u))

#define MEOWLLOC_RBTREE_RED (0b1u)
#define MEOWLLOC_RBTREE_BLACK (0b0u)

#define MEOWLLOC_RBTREE_ISRED(block)   ((block) != null && ((block)->header.size & MEOWLLOC_RBTREE_RED) == (MEOWLLOC_RBTREE_RED))
#define MEOWLLOC_RBTREE_ISBLACK(block) ((block) == null || ((block)->header.size & MEOWLLOC_RBTREE_RED) == (MEOWLLOC_RBTREE_BLACK))



#define MEOWLLOC_SENTINEL_ALLOCATED 0x00
#define MEOWLLOC_SENTINEL_FREED     0xAC


typedef struct Meowlloc_HeaderBlock Meowlloc_HeaderBlock;
struct Meowlloc_HeaderBlock {
    union {
        struct {
            union {
                Meowlloc_HeaderBlock *previous;
                uint64_t isReserved; // NOTE: this can't be `bool` because we need to compare all 64 bits of the pointer, while bool is only the lower 8 bits (or even implementation defined?) and might give false negatives
            };
            // NOTE: bit 0 stores 0 == black, 1 == red for the rbtree
            size_t size; 
        };
        char _alignmentPadding[MEOWLLOC_CONFIG_ALIGNMENT];
    };
};


// NOTE: if header.isReserved == false
typedef struct Meowlloc_HeaderBlockFree Meowlloc_HeaderBlockFree;
struct Meowlloc_HeaderBlockFree {
    Meowlloc_HeaderBlock header;

    union {
        struct {
            Meowlloc_HeaderBlockFree *lhs;
            Meowlloc_HeaderBlockFree *rhs;
        };
        char _alignmentPadding[MEOWLLOC_CONFIG_ALIGNMENT];
    };
};


// NOTE: if header.size == 0
typedef struct {
    Meowlloc_HeaderBlock header;

    union {
        struct {
            Meowlloc_HeaderBlock *initial;
            void *_padding;
        };
        char _alignmentPadding[MEOWLLOC_CONFIG_ALIGNMENT];
    };
} Meowlloc_HeaderBlockFinal;



// NOTE: assuming that sizeof(Meowlloc_HeaderBlock) == 16, alignment less than that (1, 2, 4, 8) is already supported,
// and any power of two greater than that is also trivially supported using _alignmentPadding
_Static_assert(sizeof(Meowlloc_HeaderBlock) % MEOWLLOC_CONFIG_ALIGNMENT == 0, "Alignment must be a power of two");

_Static_assert(sizeof(Meowlloc_HeaderBlock) * 2  == sizeof(Meowlloc_HeaderBlockFree), "Bad");
_Static_assert(sizeof(Meowlloc_HeaderBlockFinal) == sizeof(Meowlloc_HeaderBlockFree), "Bad");

#endif // __MEOWLLOC_INTERNAL_H
