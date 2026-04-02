#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <sys/socket.h>
#include <string.h>

#include "request.h"

int request_read(int *clientfd, request_buffer* b, request* r) {
    while (1) {
        if (b->total_read >= b->buf_size - 1) {
            perror("buffer is full");
            return -1;
        }

        ssize_t bytes_read = recv(
                *clientfd, 
                b->buf + b->total_read, 
                b->buf_size - b->total_read - 1, 
                0
            );
        
        if (bytes_read < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) return -1;
            perror("recv");
            return -1;
        }
        
        if (bytes_read == 0) {
            if (b->total_read == 0) {
                perror("read null");
                return -1;
            }
            break;
        }
        
        b->total_read += (size_t)bytes_read;
    
        b->buf[b->total_read] = '\0';

        if (strstr(b->buf, "\r\n\r\n")) {
            break;
        }
    }

    const char *p = b->buf;
    const char *end = b->buf + b->total_read;
    int state = s_start;
    int curr_header = 0;
    char* start = NULL;

    while (p < end) {
        const char *c = p++;

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
                state = s_done;
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
                if (curr_header <= MAX_HEADER_COUNT) ++curr_header;
                else state = s_done;
            }

            break;
        
        case s_done:
            break;
        }
    }

    return 0;
}
