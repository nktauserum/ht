#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>

#define HT_CAPACITY 4096

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

void ht_init(ht* t) {
    t->bucket = malloc(HT_CAPACITY * sizeof(item));
    memset(t->bucket, 0, HT_CAPACITY * sizeof(*t->bucket));
    t->capacity = HT_CAPACITY;
    t->size = 0;
}

uint32_t djb2(const char *key) {
    uint32_t hash = 5381;

    for (size_t i = 0; i < strlen(key); ++i) {
        hash = hash*33 + key[i];
    }

    return hash;
}

void ht_insert(ht* t, char *key, uint64_t value) {
    uint32_t pos = djb2(key)% t->capacity;

    printf("pos: %s => %d\n", key, pos);

    t->bucket[pos].value = value;
    t->bucket[pos].occupied = true;
    t->bucket[pos].key = key;

    ++t->size;
}

int ht_derive(ht* t, const char* key) {
    uint32_t pos = djb2(key)% t->capacity;

    // there is currently no collision handle
    if (strcmp(t->bucket[pos].key, key) < 0) 
        return -1;
        
    return t->bucket[pos].value;
}

int main(void) {
    ht table = {0};
    ht_init(&table);

    ht_insert(&table, "test", 196);
    printf("%s => %d\n", "test", ht_derive(&table, "test"));

    return 0;
}