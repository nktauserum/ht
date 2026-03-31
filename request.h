#pragma once

#include <stdlib.h>

#define GET '0'
#define SET '1'

/*
    1 (SET) name_of_field

    {"data": "hello world"}
*/

typedef struct {
    char method;
    char *field_name;   size_t field_name_len;
    char *payload;      size_t payload_len;
} request;

typedef struct {
    char *buf;
    size_t buf_size;
    size_t total_read;
} request_buffer;


int request_read(int*, request_buffer*);
void request_parse(request_buffer*, request*);