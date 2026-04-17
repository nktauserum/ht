#include "string.h"

string string_init(const char *s, size_t len) {
    string r;
    r.data = malloc(len);
    memcpy(r.data, s, len);
    r.size = len;
    r.capacity = len;
    return r;
}

void string_clean(string *s) {
    if (s->capacity != 0 && s->data) {
        free(s->data);
    }
}