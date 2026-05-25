#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "meowlloc.c"

int random_between(int lo, int hi) {
    return (rand() % (hi - lo + 1)) + lo;
}

#define IT 128

int main() {
    int seed = time(null);
    // seed = 1779712503;
    printf("SEED: %d\n", seed);
    srand(seed);

    printf("Hello, World!\n");

    int *buffer[IT] = {0};

    for(int i = 0; i < IT; i++) {
        buffer[i] = MEOWLLOC_ALLOCATE(int);
    }

    for(int i = 0; i < IT; i++) {
        meowlloc_internal_visualizeArena((Meowlloc_HeaderBlock *)MEOWLLOC_TREE);
        meowlloc_rbtree_printNode(MEOWLLOC_TREE, true);

        int j = rand() % IT;
        int size = random_between(4, 256);

        if(rand() % 2 == 0) {
            meowlloc_free(buffer[j]);
            buffer[j] = meowlloc_allocate(size);
        }
        else {
            buffer[j] = meowlloc_reallocate(buffer[j], size);
        }
    }

    for(int i = 0; i < IT; i++) {
        meowlloc_free(buffer[i]);
    }

    meowlloc_internal_visualizeArena((Meowlloc_HeaderBlock *)MEOWLLOC_TREE);

    return 0;
}
