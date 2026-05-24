#include "macros.h"


// NOTE: Red-Black tree algorithms taken mostly from here: https://ru.wikipedia.org/wiki/%D0%9A%D1%80%D0%B0%D1%81%D0%BD%D0%BE-%D1%87%D1%91%D1%80%D0%BD%D0%BE%D0%B5_%D0%B4%D0%B5%D1%80%D0%B5%D0%B2%D0%BE



typedef struct {
    Meowlloc_HeaderBlockFree *grandgrandfather;
    Meowlloc_HeaderBlockFree *grandfather;
    Meowlloc_HeaderBlockFree *uncle;
    Meowlloc_HeaderBlockFree *parent;
    Meowlloc_HeaderBlockFree *this;
} Meowlloc_RbtreeGeneration;

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




// NOTE: this searches for the smallest fitting block, but this might not be optimal wrt fragmentation idk
Meowlloc_RbtreeGeneration meowlloc_rbtree_findBestBlockRec(Meowlloc_RbtreeGeneration gen, size_t requestedSize) {
    if(gen.this == null) return gen;

    if(gen.this->lhs != null && gen.this->lhs->header.size >= requestedSize) {
        return meowlloc_rbtree_findBestBlockRec(meowlloc_rbtree_rotateGeneration(gen, gen.this->lhs), requestedSize);
    }

    if(gen.this->header.size >= requestedSize) {
        return gen;
    }

    if(gen.this->rhs != null && gen.this->header.size < requestedSize) {
        return meowlloc_rbtree_findBestBlockRec(meowlloc_rbtree_rotateGeneration(gen, gen.this->rhs), requestedSize);
    }

    return (Meowlloc_RbtreeGeneration){0};
}

Meowlloc_RbtreeGeneration meowlloc_rbtree_findBestBlock(Meowlloc_HeaderBlockFree *root, size_t requestedSize) {
    return meowlloc_rbtree_findBestBlockRec((Meowlloc_RbtreeGeneration){ .this = root }, requestedSize);
}





Meowlloc_HeaderBlockFree **Meowlloc_rbtree_getChildPointer(Meowlloc_HeaderBlockFree *root, Meowlloc_HeaderBlockFree *child, Meowlloc_HeaderBlockFree **tree) {
    return (root != null) ? (root->lhs == child ? &root->lhs : &root->rhs) : tree;
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

void meowlloc_rbtree_printNode(Meowlloc_HeaderBlockFree *node, bool newLine) {
    if(node == null) {
        printf("null");
    }
    else {
        printf("%ld%c (", node->header.size & ~MEOWLLOC_RBTREE_RED, MEOWLLOC_RBTREE_ISRED(node) ? 'r' : 'b');
        meowlloc_rbtree_printNode(node->lhs, false);
        printf(", ");
        meowlloc_rbtree_printNode(node->rhs, false);
        printf(")");
    }

    if(newLine) {
        printf("\n");
    }
}

void meowlloc_rbtree_rotateLeft(Meowlloc_RbtreeGeneration gen, bool rotate, Meowlloc_HeaderBlockFree **tree) {
    if(rotate) gen = meowlloc_rbtree_rotateGeneration(gen, null);

    Meowlloc_HeaderBlockFree *pivot = gen.grandfather->rhs;
    gen.grandfather->rhs = pivot->lhs;
    pivot->lhs = gen.grandfather;
    *Meowlloc_rbtree_getChildPointer(gen.grandgrandfather, gen.grandfather, tree) = pivot;
}

void meowlloc_rbtree_rotateRight(Meowlloc_RbtreeGeneration gen, bool rotate, Meowlloc_HeaderBlockFree **tree) {
    if(rotate) gen = meowlloc_rbtree_rotateGeneration(gen, null);

    Meowlloc_HeaderBlockFree *pivot = gen.grandfather->lhs;
    gen.grandfather->lhs = pivot->rhs;
    pivot->rhs = gen.grandfather;
    *Meowlloc_rbtree_getChildPointer(gen.grandgrandfather, gen.grandfather, tree) = pivot;
}

void meowlloc_rbtree_insertBlock(Meowlloc_HeaderBlockFree **tree, Meowlloc_HeaderBlockFree *block) {
    block->lhs = null;
    block->rhs = null;

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
            meowlloc_rbtree_rotateLeft(gen, true, tree);

            gen.this = gen.this->lhs;
            gen.parent = gen.grandfather->lhs;
        }
        else if(gen.this == gen.parent->lhs && gen.parent == gen.grandfather->rhs) {
            meowlloc_rbtree_rotateRight(gen, true, tree);

            gen.this = gen.this->rhs;
            gen.parent = gen.grandfather->rhs;
        }


        gen.parent->header.size &= ~MEOWLLOC_RBTREE_RED;
        gen.grandfather->header.size |= MEOWLLOC_RBTREE_RED;

        if(gen.this == gen.parent->lhs && gen.parent == gen.grandfather->lhs) {
            meowlloc_rbtree_rotateRight(gen, false, tree);
        }
        else {
            meowlloc_rbtree_rotateLeft(gen, false, tree);
        }


        break;
    } while(true);
}



Meowlloc_RbtreeGeneration meowlloc_rbtree_findSmallest(Meowlloc_HeaderBlockFree *root, Meowlloc_RbtreeGeneration gen) {
    gen = meowlloc_rbtree_rotateGeneration(gen, root);
    if(root == null || root->lhs == null) return gen;
    return meowlloc_rbtree_findSmallest(root->lhs, gen);
}

Meowlloc_RbtreeGeneration meowlloc_rbtree_findBiggest(Meowlloc_HeaderBlockFree *root, Meowlloc_RbtreeGeneration gen) {
    gen = meowlloc_rbtree_rotateGeneration(gen, root);
    if(root == null || root->rhs == null) return gen;
    return meowlloc_rbtree_findSmallest(root->rhs, gen);
}

void meowlloc_rbtree_removeBlock(Meowlloc_HeaderBlockFree **tree, Meowlloc_HeaderBlockFree *node, Meowlloc_RbtreeGeneration gen) {
    if(node->lhs != null && node->rhs != null) {
        Meowlloc_RbtreeGeneration clhsGen = meowlloc_rbtree_findBiggest(node->lhs, (Meowlloc_RbtreeGeneration){ .this = node });
        Meowlloc_RbtreeGeneration crhsGen = meowlloc_rbtree_findSmallest(node->rhs, (Meowlloc_RbtreeGeneration){ .this = node });

        // NOTE: it's not clear yet if there are other preferences
        Meowlloc_RbtreeGeneration swapGen = clhsGen;
        if(swapGen.this == null)  swapGen = crhsGen;

        bool swapGenClose = (node == swapGen.parent);

        // NOTE: i checked with pen and paper and it should work, but looks a bit scary
        *Meowlloc_rbtree_getChildPointer(gen.parent, node, tree) = swapGen.this;
        *Meowlloc_rbtree_getChildPointer(swapGen.parent, swapGen.this, null) = node;

        Meowlloc_HeaderBlockFree preserve = *node;

        node->lhs = swapGen.this->lhs;
        node->rhs = swapGen.this->rhs;

        swapGen.this->lhs = preserve.lhs;
        swapGen.this->rhs = preserve.rhs;

        bool isNodeRed = MEOWLLOC_RBTREE_ISRED(node);
        bool isSwapRed = MEOWLLOC_RBTREE_ISRED(swapGen.this);

        node->header.size &= ~MEOWLLOC_RBTREE_RED;
        node->header.size |= (isSwapRed * 0b1);

        swapGen.this->header.size &= ~MEOWLLOC_RBTREE_RED;
        swapGen.this->header.size |= (isNodeRed * 0b1);

        // NOTE: since we have exchanged blocks, the to-be-removed block is violating
        // the sorting conditions and we are unable to locate it normally
        if(swapGenClose) {
            swapGen = meowlloc_rbtree_getGeneration(*tree, swapGen.this, (Meowlloc_RbtreeGeneration){0});
        }
        else {
            swapGen = meowlloc_rbtree_getGeneration(*tree, swapGen.parent, (Meowlloc_RbtreeGeneration){0});
        }

        swapGen = meowlloc_rbtree_rotateGeneration(swapGen, node);

        meowlloc_rbtree_removeBlock(tree, node, swapGen);
        return;
    }


    if(MEOWLLOC_RBTREE_ISRED(node)) {
        *Meowlloc_rbtree_getChildPointer(gen.parent, node, tree) = null;
        return;
    }

    Meowlloc_HeaderBlockFree *child = (node->lhs == null) ? node->rhs : node->lhs;
    if(MEOWLLOC_RBTREE_ISBLACK(node) && MEOWLLOC_RBTREE_ISRED(child)) {
        *Meowlloc_rbtree_getChildPointer(gen.parent, node, tree) = child;
        child->header.size &= ~MEOWLLOC_RBTREE_RED;
        return;
    }

    *Meowlloc_rbtree_getChildPointer(gen.parent, node, tree) = child;
    gen.this = child;

    do {
        // case 1
        if(gen.parent == null) {
            break;
        }

        // case 2
        Meowlloc_HeaderBlockFree *brother = (gen.parent->lhs == gen.this) ? gen.parent->rhs : gen.parent->lhs;
        if(MEOWLLOC_RBTREE_ISRED(brother)) {
            gen.parent->header.size |= MEOWLLOC_RBTREE_RED;
            brother->header.size &= ~MEOWLLOC_RBTREE_RED;

            if(gen.this == gen.parent->lhs) {
                meowlloc_rbtree_rotateLeft(gen, true, tree);
            }
            else {
                meowlloc_rbtree_rotateRight(gen, true, tree);
            }
        }

        // case 3
        bool brotherFullyBlack = MEOWLLOC_RBTREE_ISBLACK(brother) &&
                                 MEOWLLOC_RBTREE_ISBLACK(brother->lhs) &&
                                 MEOWLLOC_RBTREE_ISBLACK(brother->rhs);
        if(MEOWLLOC_RBTREE_ISBLACK(gen.parent) && brotherFullyBlack) {
            brother->header.size |= MEOWLLOC_RBTREE_RED;
            gen = meowlloc_rbtree_getGeneration(*tree, gen.parent, (Meowlloc_RbtreeGeneration){0});
            continue;
        }

        // case 4
        if(MEOWLLOC_RBTREE_ISRED(gen.parent) && brotherFullyBlack) {
            brother->header.size |= MEOWLLOC_RBTREE_RED;
            gen.parent->header.size &= ~MEOWLLOC_RBTREE_RED;
            break;
        }


        // case 5
        if(MEOWLLOC_RBTREE_ISBLACK(brother)) {
            if(gen.this == gen.parent->lhs && MEOWLLOC_RBTREE_ISRED(brother->lhs) && MEOWLLOC_RBTREE_ISBLACK(brother->rhs)) {
                brother->header.size |= MEOWLLOC_RBTREE_RED;
                brother->lhs->header.size &= ~MEOWLLOC_RBTREE_RED;

                Meowlloc_RbtreeGeneration ggen = gen;
                ggen.this = brother;
                ggen = meowlloc_rbtree_rotateGeneration(ggen, null);

                meowlloc_rbtree_rotateRight(ggen, true, tree);
            }
            else if(gen.this == gen.parent->rhs && MEOWLLOC_RBTREE_ISBLACK(brother->lhs) && MEOWLLOC_RBTREE_ISRED(brother->rhs)) {
                brother->header.size |= MEOWLLOC_RBTREE_RED;
                brother->rhs->header.size &= ~MEOWLLOC_RBTREE_RED;

                Meowlloc_RbtreeGeneration ggen = gen;
                ggen.this = brother;
                ggen = meowlloc_rbtree_rotateGeneration(ggen, null);

                meowlloc_rbtree_rotateLeft(ggen, true, tree);
            }
        }

        if(MEOWLLOC_RBTREE_ISBLACK(gen.parent)) {
            brother->header.size &= ~MEOWLLOC_RBTREE_RED;
        }
        else {
            brother->header.size |= MEOWLLOC_RBTREE_RED;
        }

        gen.parent->header.size &= ~MEOWLLOC_RBTREE_RED;

        Meowlloc_HeaderBlockFree *preserve = gen.this;
        if(gen.this == gen.parent->lhs) {
            brother->rhs->header.size &= ~MEOWLLOC_RBTREE_RED;
            meowlloc_rbtree_rotateLeft(gen, true, tree);
        }
        else {
            brother->lhs->header.size &= ~MEOWLLOC_RBTREE_RED;
            meowlloc_rbtree_rotateRight(gen, true, tree);
        }

        gen.this = preserve;

        break;
    } while(0);
}
