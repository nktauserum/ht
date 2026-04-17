#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>

#include "ht.h"

#define HT_CAPACITY 4096

void ht_init(ht* t) {
    t->bucket = malloc(HT_CAPACITY * sizeof(item));
    memset(t->bucket, 0, HT_CAPACITY * sizeof(*t->bucket));
    t->capacity = HT_CAPACITY;
    t->size = 0;
}

uint32_t djb2(string s) {
    uint32_t hash = 5381;

    for (size_t i = 0; i < s.size; ++i) {
        hash = hash*33 + s.data[i];
    }

    return hash;
}

void ht_insert(ht* t, string key, string value) {
    uint32_t pos = djb2(key)% t->capacity;
    
    printf("pos: %.*s => %d\n", (int)key.size, key.data, pos);
    
    int i = 1;
    if (t->bucket[pos].occupied) {
        string_clean(&t->bucket[pos].value);
        i = 0;
    }

    t->bucket[pos].value = value;
    t->bucket[pos].occupied = true;
    t->bucket[pos].key = key;
    t->size += i;
}

item* ht_derive(ht* t, string key) {
    uint32_t pos = djb2(key)% t->capacity;

    // there is currently no collision handle
    // if (strcmp(t->bucket[pos].key, key) < 0) 
    //     return NULL;
        
    return &t->bucket[pos];
}