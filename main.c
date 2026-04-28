#include <stdio.h>
#include <pthread.h>
#include <threads.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

#include "ht.h"
#include "networking.h"

#define PORT 5000
#define QUEUE_SIZE 2048
#define WORKERS_COUNT 32
#define REQUEST_BUFFER_SIZE 1024*6
#define CHECK_SLEEP_TIME 5

struct {
    ht table;
    pthread_rwlock_t mu;
} data;

struct {
    int port;
 int server_fd;
    struct sockaddr_in server_addr;
} server;

typedef struct {
    int* queue[QUEUE_SIZE];
    int head, tail, size;

    pthread_mutex_t mu;
    pthread_cond_t cond;
} incoming_connections_buf;

void* worker(void* arg) {
    incoming_connections_buf* buf = arg;
    char* rb_buf = malloc(REQUEST_BUFFER_SIZE);
    char* wb_buf = malloc(REQUEST_BUFFER_SIZE);
    char* header_buf = malloc(REQUEST_BUFFER_SIZE);

    request_buffer rb = {.buf = rb_buf, .buf_size = REQUEST_BUFFER_SIZE, .total_read = 0};
    response_buffer wb = {.write_buf = wb_buf, .available_buf_size = REQUEST_BUFFER_SIZE, .content_length = 0};

    while (1) {
        pthread_mutex_lock(&buf->mu);
        while (buf->size == 0) pthread_cond_wait(&buf->cond, &buf->mu);

        int* clientfd = buf->queue[buf->tail];
        buf->tail = (buf->tail + 1) % QUEUE_SIZE;
        --buf->size;
        pthread_mutex_unlock(&buf->mu);

        do {
            request r = {0};
            int err;
            if ((err = request_read(clientfd, &rb)) != OK) {
                printf("ERROR: request_read(): ");
                switch (err)
                {
                case ERR_FULL_BUFFER:
                    printf("request buffer is full. TODO: make it dynamic.\n");
                    memcpy(wb.status_code, "413 Content Too Large", 22);
                    break;

                    
                case ERR_RECV:
                    printf("recv(): some kind of network error.\n");
                    memcpy(wb.status_code, "500 Internal Server Error", 26);
                    break;

                case ERR_READ_NULL:
                    printf("recv() has read zero bytes.\n");
                    memcpy(wb.status_code, "400 Bad Request", 16);
                    break;
                };
                continue;
            }

            if ((err = request_parse(clientfd, &rb, &r)) != OK) {
                printf("ERROR: request_parse: ");
                switch (err) {    
                case ERR_RECV:
                    printf("recv(): some kind of network error.\n");
                    memcpy(wb.status_code, "500 Internal Server Error", 26);
                    break;

                case ERR_BAD_REQUEST:
                    printf("bad request.\n");
                    memcpy(wb.status_code, "400 Bad Request", 16);
                    break;

                case ERR_MALLOC:
                    printf("malloc()\n");
                    memcpy(wb.status_code, "413 Content Too Large", 22);
                    break;
                
                default:
                    printf("unknown error.\n");
                    break;
                }

                continue;
            }

            // init the string & also strip the leading /
            string field_name = string_init(r.field_name+1, r.field_name_len-1);

            if (r.field_name_len <= 1) {
                memcpy(wb.status_code, "404 Not Found", 14);
                string_clean(&field_name);
                break;
            }

            if (strncmp(r.method, "GET", r.method_len) == 0) {

                pthread_rwlock_rdlock(&data.mu);
                item* it = ht_derive(&data.table, field_name);
                pthread_rwlock_unlock(&data.mu);

                if (!it) {
                    memcpy(wb.status_code, "204 No Content", 15);
                    break;
                }

                memcpy(wb.write_buf, it->value.data, it->value.size);
                wb.content_length = it->value.size;
                memcpy(wb.status_code, "200 OK", 7);

                string_clean(&field_name);
            } else if (strncmp(r.method, "POST", r.method_len) == 0 || strncmp(r.method, "PUT", r.method_len) == 0) {
                if (r.payload_len == 0) {
                    memcpy(wb.status_code, "400 Bad Request", 16);
                    break;
                }

                string value = string_init(r.payload, r.payload_len);

                int64_t ttl = 0;
                for (size_t i = 0; i < r.headers_count; ++i) {
                    if (strncmp("TTL", r.headers[i].key, r.headers[i].key_len) == 0) {
                        int64_t timestamp_s = time(NULL);
                        ttl = atoll(r.headers[i].value) + timestamp_s;
                    }
                }

                pthread_rwlock_wrlock(&data.mu);
                ht_insert(&data.table, field_name, value, ttl);
                pthread_rwlock_unlock(&data.mu);

                memcpy(wb.status_code, "200 OK", 7);

            } else {
                memcpy(wb.status_code, "405 Method Not Allowed", 23);

                string_clean(&field_name);
            }
        } while (0);
        
        snprintf(
            header_buf,
            REQUEST_BUFFER_SIZE, 
            "HTTP/1.1 %s\r\n"
            "Content-Type: text/plain\r\n"
            "Connection: close\r\n"
            "Content-Length: %lu\r\n\r\n",
            wb.status_code, wb.content_length
        );
 
        send(*clientfd, header_buf, strlen(header_buf), 0);
        if (wb.content_length != 0) {
            send(*clientfd, wb.write_buf, wb.content_length, 0);
        }

        close(*clientfd);
        free(clientfd);
        rb.total_read = 0;
    }

    // actually unnecessary
    free(rb_buf);
}

void* data_worker(void*) {
    while (1) { 
        for (size_t i = 0; i < data.table.capacity; ++i) {
            pthread_rwlock_rdlock(&data.mu);

            bool expired = false;

            if (data.table.bucket[i].occupied) {
                uint64_t ttl = data.table.bucket[i].ttl;
                expired = (ttl != 0 && ttl < (uint64_t)time(NULL));
            } 

            pthread_rwlock_unlock(&data.mu);

            if (expired) {
                pthread_rwlock_wrlock(&data.mu);
    
                uint64_t ttl = data.table.bucket[i].ttl;
                if (data.table.bucket[i].occupied &&
                    ttl != 0 && 
                    ttl < (uint64_t)time(NULL)) {

                    string_clean(&data.table.bucket[i].value); 
                    string_clean(&data.table.bucket[i].key); 

                    data.table.bucket[i].occupied = false;
                    data.table.bucket[i].ttl = 0;
                }
                pthread_rwlock_unlock(&data.mu);
            }
        }


        struct timespec ts = {.tv_sec = CHECK_SLEEP_TIME};
        thrd_sleep(&ts, NULL);
    }
}

int main(void) {
    if ((server.server_fd = socket(AF_INET, SOCK_STREAM,0)) < 0) {
        perror("socket failed");
        return 1;
    }

    int opt = 1;
    if (setsockopt(server.server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        close(server.server_fd);
        return 1;
    }

    server.server_addr.sin_family = AF_INET;
    server.server_addr.sin_addr.s_addr = INADDR_ANY;
    server.server_addr.sin_port = htons(PORT);

    if (bind(server.server_fd, (struct sockaddr *)&server.server_addr, sizeof(server.server_addr)) < 0) {
        perror("bind failed");
        close(server.server_fd);
        return 1;
    }

     if (listen(server.server_fd, SOMAXCONN) < 0) {
        perror("listen failed");
        close(server.server_fd);
        return 1;
    }

    if (fcntl(server.server_fd, F_SETFL, O_NONBLOCK) < 0) {
        perror("fcntl failed");
        close(server.server_fd);
        return 1;
    }

    printf("INFO: server successfully started on port %d\n", PORT);

    // initializing the connections buffer
    incoming_connections_buf buf;
    pthread_mutex_init(&buf.mu, NULL);
    pthread_cond_init(&buf.cond, NULL);
    buf.head = buf.tail = buf.size = 0;

    // initialising the data table
    ht_init(&data.table);
    pthread_rwlock_init(&data.mu, NULL);
    pthread_t w;
    pthread_create(&w, NULL, data_worker, NULL);

    pthread_t workers[WORKERS_COUNT];
    for (uint8_t i = 0; i < WORKERS_COUNT; ++i) 
        pthread_create(&workers[i], NULL, worker, &buf);

    int epfd = epoll_create1(0);
    struct epoll_event ev = {.events = EPOLLIN, .data.fd = server.server_fd};
    epoll_ctl(epfd, EPOLL_CTL_ADD, server.server_fd, &ev);

    while (1) {
        struct epoll_event events[64];
        int n = epoll_wait(epfd, events, 64, -1);

        for (int i = 0; i < n; ++i) {
            if (events[i].data.fd == server.server_fd) {
                while (1) {
                    int* clientfd = (int*)malloc(sizeof(int));
                    if (!clientfd) break;

                    struct timeval tv = { .tv_sec = 10, .tv_usec = 0 };  
                    setsockopt(*clientfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

                    if ((*clientfd = accept(server.server_fd, NULL, NULL)) < 0) {
                        if (errno != EAGAIN) perror("accept failed");
                        free(clientfd);
                        break;
                    }

                    pthread_mutex_lock(&buf.mu);

                    if (buf.size < QUEUE_SIZE) {
                        buf.queue[buf.head] = clientfd;
                        buf.head =  (buf.head + 1) % QUEUE_SIZE;
                        ++buf.size;
                        pthread_cond_signal(&buf.cond);
                    } else {
                        close(*clientfd);
                        free(clientfd);
                    }

                    pthread_mutex_unlock(&buf.mu);
                }
            }
        }
    }

    return 0;
}
