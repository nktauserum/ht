#pragma once

#include <stdlib.h>
#include <string.h>

typedef struct {
    char* data;
    size_t size;
    size_t capacity;
} string;

string string_init(const char*, size_t);
void string_clean(string*);
int string_cmp(string, string);
