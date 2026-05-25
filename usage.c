#include <stdio.h>

#include "meowlloc.h"

void meowlloc_internal_visualizeArena(void *);
extern void *MEOWLLOC_TREE;

int main() {
    printf("Hello, World!\n");

    int *a = MEOWLLOC_ALLOCATE(int);
    *a = 5;


    printf("%d\n", *a);

    meowlloc_internal_visualizeArena(MEOWLLOC_TREE);
    int *numbers = MEOWLLOC_REALLOCATE_N(a, int, 16);
    meowlloc_internal_visualizeArena(MEOWLLOC_TREE);

    for(int i = 0; i < 16; i++) {
        numbers[i] = i;
    }

    for(int i = 0; i < 4; i++) {
        printf("%d %d %d %d\n", numbers[i*4 + 0], numbers[i*4 + 1], numbers[i*4 + 2], numbers[i*4 + 3]);
    }

    printf("%p %p\n", a, numbers);

    meowlloc_free(numbers);

    for(int i = 0; i < 4; i++) {
        printf("%d %d %d %d\n", numbers[i*4 + 0], numbers[i*4 + 1], numbers[i*4 + 2], numbers[i*4 + 3]);
    }


    return 0;
}
