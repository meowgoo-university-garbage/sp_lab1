#include <stdio.h>
#include <stddef.h>

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
    printf("Hello, World! %d\n", alignof(max_align_t));


    Meowlloc_HeaderBlockFree *tree = null;


#define BLOCK(name, x) \
    Meowlloc_HeaderBlockFree name = { .header = { .size = (x) } }; \
    meowlloc_rbtree_insertBlock(&tree, &name)

    BLOCK(block1, 2);
    BLOCK(block2, 4);
    BLOCK(block3, 6);
    BLOCK(block4, 8);
    BLOCK(block5, 16);
    BLOCK(block6, 32);
    BLOCK(block7, 64);
    BLOCK(block8, 128);

    printf("%d\n", tree->header.size);
    printf("%d\n", tree->header.size);

    meowlloc_rbtree_printNode(tree);
    printf("\n");

    return 0;
}
