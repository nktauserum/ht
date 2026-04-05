#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "string.h"

typedef struct {
    string key;
    string value;
    bool occupied;
} item;

typedef struct {
    item *bucket;
    size_t capacity;
    size_t size;
} ht;

void ht_insert(ht*, string, string);
item* ht_derive(ht*, string);
void ht_init(ht*);
