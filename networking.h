#pragma once

#include <stdlib.h>

#define MAX_HEADER_COUNT 64
#define MAX_PAYLOAD_SIZE 1024*6


enum {
    OK,
    ERR_FULL_BUFFER,
    ERR_RECV,
    ERR_READ_NULL,
    ERR_BAD_REQUEST,
    ERR_MALLOC,
};


enum {
    s_start,
    s_path,
    s_method,
    s_protocol,
    s_header_key,
    s_header_value,
    s_headers_done,
    s_read_body,
    s_done
};

typedef struct {
    char* key;              size_t key_len;
    char* value;            size_t value_len;
} header;

typedef struct {
    char *method;                           size_t method_len;
    char *field_name;                       size_t field_name_len;
    char *protocol;                         size_t protocol_len;

    char *payload;                          size_t payload_len;
    header headers[MAX_HEADER_COUNT];       size_t headers_count;
} request;

typedef struct {
    char* buf;
    size_t buf_size;
    size_t total_read;
} request_buffer;

typedef struct {
    char* write_buf;       size_t available_buf_size;
    char status_code[32];
    size_t content_length;
} response_buffer;


int request_read(int*, request_buffer*);
int request_parse(int*, request_buffer*, request*);
