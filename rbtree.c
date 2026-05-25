#include "macros.h"


// NOTE: Red-Black tree algorithms taken mostly from here: https://ru.wikipedia.org/wiki/%D0%9A%D1%80%D0%B0%D1%81%D0%BD%D0%BE-%D1%87%D1%91%D1%80%D0%BD%D0%BE%D0%B5_%D0%B4%D0%B5%D1%80%D0%B5%D0%B2%D0%BE

#define SZ(x) ((x) & MEOWLLOC_SIZEMASK)

// GENERATIONS (needed because nodes don't have a parent pointer)
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

Meowlloc_RbtreeGeneration meowlloc_rbtree_getGeneration(Meowlloc_HeaderBlockFree *root, Meowlloc_HeaderBlockFree *needle, Meowlloc_RbtreeGeneration gen) {
    if(root == null) return (Meowlloc_RbtreeGeneration){0};
    gen = meowlloc_rbtree_rotateGeneration(gen, root);
    if(root == needle) return gen;

    if(SZ(needle->header.size) == SZ(root->header.size)) {
        Meowlloc_RbtreeGeneration result = meowlloc_rbtree_getGeneration(root->lhs, needle, gen);
        if(result.this == null)   result = meowlloc_rbtree_getGeneration(root->rhs, needle, gen);
        return result;
    }

    if(SZ(needle->header.size) > SZ(root->header.size)) {
        return meowlloc_rbtree_getGeneration(root->rhs, needle, gen);
    }
    else {
        return meowlloc_rbtree_getGeneration(root->lhs, needle, gen);
    }
}








// UTILITY
Meowlloc_HeaderBlockFree **Meowlloc_rbtree_getChildPointer(Meowlloc_HeaderBlockFree *root, Meowlloc_HeaderBlockFree *child, Meowlloc_HeaderBlockFree **tree) {
    return (root != null) ? (root->lhs == child ? &root->lhs : &root->rhs) : tree;
}

void meowlloc_rbtree_printNode(Meowlloc_HeaderBlockFree *node, bool newLine) {
    if(node == null) {
        printf("null");
    }
    else {
        printf("%ld%c (", SZ(node->header.size), MEOWLLOC_RBTREE_ISRED(node) ? 'r' : 'b');

        if(node->lhs == node) {
            printf("rec");
        }
        else {
            meowlloc_rbtree_printNode(node->lhs, false);
        }

        printf(", ");

        if(node->rhs == node) {
            printf("rec");
        }
        else {
            meowlloc_rbtree_printNode(node->rhs, false);
        }

        printf(")");
    }

    if(newLine) {
        printf("\n");
    }
}

// NOTE: if tree has only 1 block (i.e. only 1 arena) we don't want to free it
bool meowlloc_rbtree_isTreeMinimal(Meowlloc_HeaderBlockFree *root) {
    if(root == null) return true;
    if(root->lhs == null && root->rhs == null) return true;
    return false;
}



// ROTATING
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


// SEARCHING
Meowlloc_RbtreeGeneration meowlloc_rbtree_findSmallest(Meowlloc_HeaderBlockFree *root, Meowlloc_RbtreeGeneration gen) {
    gen = meowlloc_rbtree_rotateGeneration(gen, root);
    if(root == null || root->lhs == null) return gen;
    return meowlloc_rbtree_findSmallest(root->lhs, gen);
}

Meowlloc_RbtreeGeneration meowlloc_rbtree_findBiggest(Meowlloc_HeaderBlockFree *root, Meowlloc_RbtreeGeneration gen) {
    gen = meowlloc_rbtree_rotateGeneration(gen, root);
    if(root == null || root->rhs == null) return gen;
    return meowlloc_rbtree_findBiggest(root->rhs, gen);
}

// NOTE: this searches for the smallest fitting block, but this might not be optimal wrt fragmentation idk
Meowlloc_RbtreeGeneration meowlloc_rbtree_findBestBlockRec(Meowlloc_RbtreeGeneration gen, size_t requestedSize) {
    if(gen.this == null) return gen;

    if(gen.this->lhs != null && SZ(gen.this->lhs->header.size) >= requestedSize) {
        return meowlloc_rbtree_findBestBlockRec(meowlloc_rbtree_rotateGeneration(gen, gen.this->lhs), requestedSize);
    }

    if(SZ(gen.this->header.size) >= requestedSize) {
        return gen;
    }

    if(gen.this->rhs != null && SZ(gen.this->header.size) < requestedSize) {
        return meowlloc_rbtree_findBestBlockRec(meowlloc_rbtree_rotateGeneration(gen, gen.this->rhs), requestedSize);
    }

    return (Meowlloc_RbtreeGeneration){0};
}

Meowlloc_RbtreeGeneration meowlloc_rbtree_findBestBlock(Meowlloc_HeaderBlockFree *root, size_t requestedSize) {
    return meowlloc_rbtree_findBestBlockRec((Meowlloc_RbtreeGeneration){ .this = root }, requestedSize);
}



// INSERT
Meowlloc_RbtreeGeneration meowlloc_rbtree_insertBlockRec(Meowlloc_HeaderBlockFree *root, Meowlloc_HeaderBlockFree *block, Meowlloc_RbtreeGeneration gen) {
    if(root == null) {
        gen = meowlloc_rbtree_rotateGeneration(gen, block);
        return gen;
    }

    Meowlloc_HeaderBlockFree **candidatePointer = (SZ(block->header.size) > SZ(root->header.size)) ? &root->rhs : &root->lhs;
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

void meowlloc_rbtree_insertBlock(Meowlloc_HeaderBlockFree **tree, Meowlloc_HeaderBlockFree *block) {
    block->lhs = null;
    block->rhs = null;
    block->header.size &= ~MEOWLLOC_RBTREE_RED;

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




// REMOVE
void meowlloc_rbtree_removeBlock(Meowlloc_HeaderBlockFree **tree, Meowlloc_HeaderBlockFree *node, Meowlloc_RbtreeGeneration gen) {
    if(node->lhs == node) node->lhs = null;
    if(node->rhs == node) node->rhs = null;


    if(node->lhs != null && node->rhs != null) {
        Meowlloc_RbtreeGeneration clhsGen = meowlloc_rbtree_findBiggest (node->lhs, (Meowlloc_RbtreeGeneration){ .this = node });
        Meowlloc_RbtreeGeneration crhsGen = meowlloc_rbtree_findSmallest(node->rhs, (Meowlloc_RbtreeGeneration){ .this = node });

        // NOTE: it's not clear yet if there are other preferences
        Meowlloc_RbtreeGeneration swapGen = clhsGen;
        if(swapGen.this == null)  swapGen = crhsGen;
        swapGen = meowlloc_rbtree_getGeneration(*tree, swapGen.this, (Meowlloc_RbtreeGeneration){0});

        bool swapGenClose = (node == swapGen.parent);

        Meowlloc_HeaderBlockFree preserve = *node;



        *Meowlloc_rbtree_getChildPointer(gen.parent, node, tree) = swapGen.this;
        *Meowlloc_rbtree_getChildPointer(swapGen.parent, swapGen.this, null) = node;

        node->lhs = swapGen.this->lhs;
        node->rhs = swapGen.this->rhs;

        swapGen.this->lhs = preserve.lhs;
        swapGen.this->rhs = preserve.rhs;

        if(preserve.lhs == swapGen.this) {
            swapGen.this->lhs = node;
        }
        else {
            swapGen.this->lhs = preserve.lhs;
        }

        if(preserve.rhs == swapGen.this) {
            swapGen.this->rhs = node;
        }
        else {
            swapGen.this->rhs = preserve.rhs;
        }


        bool isNodeRed = MEOWLLOC_RBTREE_ISRED(node);
        bool isSwapRed = MEOWLLOC_RBTREE_ISRED(swapGen.this);

        node->header.size &= ~MEOWLLOC_RBTREE_RED;
        node->header.size |= (isSwapRed * MEOWLLOC_RBTREE_RED);

        swapGen.this->header.size &= ~MEOWLLOC_RBTREE_RED;
        swapGen.this->header.size |= (isNodeRed * MEOWLLOC_RBTREE_RED);

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

    Meowlloc_HeaderBlockFree *child = (node->lhs == null) ? node->rhs : node->lhs;

    *Meowlloc_rbtree_getChildPointer(gen.parent, node, tree) = child;

    if(MEOWLLOC_RBTREE_ISRED(node)) {
        return;
    }



    if(MEOWLLOC_RBTREE_ISBLACK(node) && MEOWLLOC_RBTREE_ISRED(child)) {
        child->header.size &= ~MEOWLLOC_RBTREE_RED;
        return;
    }

    gen.this = child;

    // return;

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
        
        gen = meowlloc_rbtree_getGeneration(*tree, gen.parent, (Meowlloc_RbtreeGeneration){0});
        gen = meowlloc_rbtree_rotateGeneration(gen, child);

        brother = (gen.parent->lhs == gen.this) ? gen.parent->rhs : gen.parent->lhs;



        if(brother == null) break;

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

                if(brother->lhs != null)
                    brother->lhs->header.size &= ~MEOWLLOC_RBTREE_RED;

                Meowlloc_RbtreeGeneration ggen = gen;
                ggen.this = brother;
                ggen = meowlloc_rbtree_rotateGeneration(ggen, null);

                meowlloc_rbtree_rotateRight(ggen, true, tree);
            }
            else if(gen.this == gen.parent->rhs && MEOWLLOC_RBTREE_ISBLACK(brother->lhs) && MEOWLLOC_RBTREE_ISRED(brother->rhs)) {
                brother->header.size |= MEOWLLOC_RBTREE_RED;

                if(brother->rhs != null)
                    brother->rhs->header.size &= ~MEOWLLOC_RBTREE_RED;

                Meowlloc_RbtreeGeneration ggen = gen;
                ggen.this = brother;
                ggen = meowlloc_rbtree_rotateGeneration(ggen, null);

                meowlloc_rbtree_rotateLeft(ggen, true, tree);
            }
        }
        
        gen = meowlloc_rbtree_getGeneration(*tree, gen.parent, (Meowlloc_RbtreeGeneration){0});
        gen = meowlloc_rbtree_rotateGeneration(gen, child);

        brother = (gen.parent->lhs == gen.this) ? gen.parent->rhs : gen.parent->lhs;

        if(MEOWLLOC_RBTREE_ISBLACK(gen.parent)) {
            brother->header.size &= ~MEOWLLOC_RBTREE_RED;
        }
        else {
            brother->header.size |= MEOWLLOC_RBTREE_RED;
        }

        gen.parent->header.size &= ~MEOWLLOC_RBTREE_RED;

        if(gen.this == gen.parent->lhs) {
            if(brother->rhs != null)
                brother->rhs->header.size &= ~MEOWLLOC_RBTREE_RED;
            meowlloc_rbtree_rotateLeft(gen, true, tree);
        }
        else {
            if(brother->lhs != null)
                brother->lhs->header.size &= ~MEOWLLOC_RBTREE_RED;
            meowlloc_rbtree_rotateRight(gen, true, tree);
        }

        break;
    } while(0);
}
