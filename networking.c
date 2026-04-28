#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <sys/socket.h>
#include <string.h>

#include "networking.h"

int request_read(int *clientfd, request_buffer* b) {
    ssize_t bytes_read = recv(
            *clientfd, 
            b->buf + b->total_read, 
            b->buf_size - b->total_read - 1, 
            0
        );
    
    if (bytes_read < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return ERR_RECV;
        return ERR_RECV;
    }
    
    b->total_read = (size_t)bytes_read;
    b->buf[b->total_read] = '\0';

    return OK;
}

int request_parse(int *clientfd, request_buffer* b, request* r) {

    char *p = b->buf;
    const char *end = b->buf + b->total_read;
    int state = s_start;
    int curr_header = 0;
    char* start = NULL;
    ssize_t content_length = 0;

    while (p < end) {
        char *c = p++;

        switch (state) {
        case s_start:
            state = s_method;
            r->method = c;
            break;

        case s_method:
            if (*c == ' ') {
                state = s_path;
                r->field_name = c+1;
                r->method_len = c - r->method;
            }
            break;
            
        case s_path:
            if (*c == ' ') {
                state = s_protocol;
                r->protocol = c+1;
                r->field_name_len = c - r->field_name;
            }
            break;
        
        case s_protocol:
            if (*c == '\r') {
                state = s_header_key;
                r->protocol_len = c - r->protocol;

                // обрабатываем \n
                start = ++c;
            }
            break;

        case s_header_key:
            if (*c == ' ' || *c == '\n') {
                start = ++c;
                continue;
            }

            if (*c == '\r') {
                state = s_headers_done;
                r->headers_count = curr_header++; 
                continue;
            }

            if (*c == ':') {
                state = s_header_value;
                r->headers[curr_header].key = start;
                r->headers[curr_header].key_len = c - start;
                start = ++c;
            }

            break;

        case s_header_value:
            if (*c == ' ' || *c == '\n') {
                start = ++c;
                continue;
            }

            if (*c == '\r') {
                state = s_header_key;
                r->headers[curr_header].value = start;
                r->headers[curr_header].value_len = c - start;
                start = ++c;
                if (curr_header < MAX_HEADER_COUNT) ++curr_header;
                else state = s_headers_done;
            }

            break;

        case s_headers_done:
            for (size_t i = 0; i < r->headers_count; ++i) {
                if (r->headers[i].key_len == 14 && strncmp(r->headers[i].key, "Content-Length", r->headers[i].key_len) == 0) {
                    content_length = strtoll(r->headers[i].value, NULL, 10);
                    if (content_length < 0) content_length = 0;
                }
            }

            if (content_length <= 0) {
                state = s_done;
                continue;
            }

            state = s_read_body;
            break;

        case s_read_body: ;
            char *hdr = strstr(b->buf, "\r\n\r\n");
            if (!hdr) {
                return ERR_BAD_REQUEST;
            }

            // copy existing bytes of body to the payload
            size_t body_offset = (size_t)(hdr - b->buf) + 4;
            size_t avail = b->total_read > body_offset ? b->total_read - body_offset : 0;
            size_t to_copy = avail < (size_t)content_length ? avail : (size_t)content_length;

            // if payload bytes already exist in the buffer
            if (to_copy < (size_t)content_length) {
                if (content_length + 1 > MAX_PAYLOAD_SIZE) {
                    return ERR_BAD_REQUEST;
                }

                r->payload = malloc((size_t)content_length + 1);
                if (!r->payload) {
                    return ERR_MALLOC;
                }
            } else {
                r->payload = b->buf + body_offset;
            }
            

            if (to_copy > 0 && r->payload != b->buf + body_offset) {
                memcpy(r->payload, b->buf + body_offset, to_copy);
            }


            size_t copied = to_copy;

            // if there are some remaining bytes, read it
            while (copied < (size_t)content_length) {
                ssize_t bytes_read = recv(*clientfd, r->payload + copied, (size_t)content_length - copied, 0);
                if (bytes_read < 0) {
                    if (errno == EINTR) continue;
                    if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                    free(r->payload);
                    return ERR_RECV;
                }
                if (bytes_read == 0) break;
                copied += (size_t)bytes_read;
            }

            r->payload_len = (ssize_t)copied;

            printf("payload: %.*s\n", (int)r->payload_len, r->payload);

            state = s_done;
            break;
        
        case s_done:
            break;
        }
    }

    return 0;
}
