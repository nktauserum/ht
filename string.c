#include "string.h"

string string_init(const char *s, size_t len) {
    string r;
    r.data = malloc(len);
    memcpy(r.data, s, len);
    r.size = len;
    return r;
}