#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <sys/socket.h>
#include <string.h>

#include "request.h"

int request_read(int *clientfd, request_buffer* b) {
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

    return 0;
}

void request_parse(request_buffer* buf, request* r) {

}
