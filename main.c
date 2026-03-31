#include <stdio.h>

#include "ht.h"

int main(void) {
    ht table = {0};
    ht_init(&table);

    ht_insert(&table, "test", 196);
    printf("%s => %ld\n", "test", ht_derive(&table, "test")->value);

    return 0;
}