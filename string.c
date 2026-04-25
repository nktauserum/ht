#include <string.h>

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
    if (s->capacity != 0 && s->data != NULL) {
        free(s->data);
    }
}

int string_cmp(string f, string s) {
    return strncmp(f.data, s.data, f.size) != 0;
}
