#include <stdio.h>
#include <stddef.h>

#define MEOWLLOC_CONFIG_ALIGNMENT 16
#include "meowlloc.c"




/*

we are required to make mem_allocate, mem_reallocate and mem_free,
all O(log(n))

mem_allocate needs to find an appropriate block in *some* data structure,
potentially split it, mark it as reserved

mem_free needs to unreserve the block, merge with other blocks if possible

mem_reallocate needs to check whether there is an adjacent block to be
merged with, or just perform a (mem_allocate > mem_copy > mem_free)



so really the main bottleneck here, algorithm complexity-wise, is managing
the blocks. Task suggests using a red black tree based on block's size to
manage it all (although im not exactly sure how would we merge the blocks
back, but i reckon it's possible). 

okay after thinking a bit i think i have an idea of what to do - basically
we embed the tree into block headers, so that the tree doesn't need any
additional memory to be managed, other than the one used for allocations

The rough idea is that we mmap a page and treat it as an array of block
headers, that are the size of `max_align_t` to ensure alignment. I really
don't like that it wastes like (32 + (0 .. 15)) bytes per allocation, but
guess that has to do?

okay i asked chatgpt about how much overhead does malloc have, and it gave
the following idea - since the allocated block is removed from the tree, it
does not need tree's pointers, so it can have a much reduced header. I'm
pretty sure it only needs a size and a status in this case

In order to merge block we would need to get their metadata somehow. We can
get the next block's address by incrementing by block's size (unless it is
the last block, but it's solveable). I'm not exactly sure how one would get
the previous block - there can be a scenario where blocks get freed left to
right, and they dont have an opportunity to merge with a free rhs block.

I feel like a genius, okay - so each free block gets created at some point.
If, upon its creation, it sees that the next block is allocated, it assigns
its previous pointer - if this pointer is equal to null it means that the
block is free. When that block gets freed it can retrieve the pointer, check
if its still free (the pointer will always point to a valid header, but it
might become allocated in the meanwhile), and possibly merge. To prevent the
merge of the rightmost block with the right end of the arena, we put a block
of size zero at the end of the arena. To prevent the merge of the leftmost
block with the left end of the arena, it assigns the previous pointer its own
address, and stores a flag when it's free. Thus, we don't even need an arena
header really. I actually feel proud wow, this seems solid


*/





int main() {
    // printf("Hello, World! %d\n", alignof(max_align_t));


    Meowlloc_HeaderBlockFree *tree = null;


#define BLOCK(x) \
    Meowlloc_HeaderBlockFree block##x = { .header = { .size = (x) } }; \
    meowlloc_rbtree_insertBlock(&tree, &block##x)

    BLOCK(2);
    BLOCK(4);
    BLOCK(6);
    BLOCK(8);
    BLOCK(16);
    BLOCK(32);
    BLOCK(64);
    BLOCK(128);

    meowlloc_rbtree_printNode(tree, true);

    meowlloc_rbtree_removeBlock(&tree, &block2,  meowlloc_rbtree_getGeneration(tree, &block2,  (Meowlloc_RbtreeGeneration){0}));
    meowlloc_rbtree_removeBlock(&tree, &block32, meowlloc_rbtree_getGeneration(tree, &block32, (Meowlloc_RbtreeGeneration){0}));
    meowlloc_rbtree_printNode(tree, true);



#define print \
    meowlloc_internal_visualizeArena(&MEOWLLOC_TREE->header); \
    meowlloc_rbtree_printNode(MEOWLLOC_TREE, true); \
    printf("\n")

    printf("TREE\n\n");

    print;
    int *allocation_0 = meowlloc_allocate(16); print;
    int *allocation_1 = meowlloc_allocate(16); print;
    allocation_1 = meowlloc_reallocate(allocation_1, 17); print;
    allocation_0 = meowlloc_reallocate(allocation_0, 17); print;
    meowlloc_free(allocation_0); print;
    meowlloc_free(allocation_1); print;


    return 0;
}
