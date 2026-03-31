#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct {
    char* key;
    uint64_t value;
    bool occupied;
} item;

typedef struct {
    item *bucket;
    size_t capacity;
    size_t size;
} ht;

void ht_insert(ht*, char*, uint64_t);
item* ht_derive(ht*, const char*);
void ht_init(ht*);
