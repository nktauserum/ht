#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "string.h"

typedef struct {
    string key;
    string value;
    int64_t ttl;
    bool occupied;
} item;

typedef struct {
    item *bucket;
    size_t capacity;
    size_t size;
} ht;

void ht_insert(ht*, string, string, int64_t);
item* ht_derive(ht*, string);
void ht_init(ht*);
void ht_delete(ht*, string);
void* ht_worker(void*); 
