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

void ht_insert(ht* t, string key, string value, int64_t ttl) {
    uint32_t start = djb2(key)%t->capacity;

    for (uint32_t i = 0; i < t->capacity-start; ++i) {
        uint32_t pos = start + i;

        // empty slot
        if (!t->bucket[pos].occupied) {        
            t->bucket[pos].value = value;
            t->bucket[pos].occupied = true;
            t->bucket[pos].key = key;
            t->bucket[pos].ttl = ttl;
            t->size += 1;
            break;
        }

        // the same key
        if (string_cmp(t->bucket[pos].key, key) == 0) { 
            string_clean(&t->bucket[pos].value);
            t->bucket[pos].value = value;
            t->bucket[pos].ttl = ttl;
            break;
        }
    }
}

item* ht_derive(ht* t, string key) {
    uint32_t start = djb2(key)%t->capacity;
    uint32_t pos = 0;

    for (uint32_t i = 0; i < t->capacity-start; ++i) {
        pos = start + i;

       // not found
        if (!t->bucket[pos].occupied) {        
            return NULL;
        }

        // the same key
        if (string_cmp(t->bucket[pos].key, key) == 0) { 
            break;
        }
    }

    return &t->bucket[pos];
}

void ht_delete(ht* t, string key) {
    uint32_t start = djb2(key)%t->capacity;

    for (uint32_t i = 0; i < t->capacity; ++i) {
        uint32_t pos = start + i;

        // empty slot
        if (!t->bucket[pos].occupied) {        
            break;
        }

        // the same key
        if (string_cmp(t->bucket[pos].key, key) == 0) { 
            string_clean(&t->bucket[pos].value);
            string_clean(&t->bucket[pos].key);
            t->bucket[pos].occupied = false;
            break;
        }
    }
}

