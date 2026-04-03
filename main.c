#include <stdio.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include "ht.h"
#include "request.h"

#define PORT 5000
#define QUEUE_SIZE 2048
#define WORKERS_COUNT 32
#define REQUEST_BUFFER_SIZE 1024*6

struct {
    ht table;
    pthread_mutex_t mu;
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

void handle(int *clientfd, request_buffer* b) {
    request r = {0};
    if ((request_read(clientfd, b, &r)) < 0) {
        perror("request_read");
        return;
    }

    // debug
    printf(
        "method: %.*s\n"
        "field: %.*s\n"
        "protocol: %.*s\n",
        (int)r.method_len, r.method,
        (int)r.field_name_len, r.field_name,
        (int)r.protocol_len, r.protocol
    );

    send(*clientfd, 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Connection: close\r\n"
        "Content-Length: 5\r\n\r\n"
        "read", 88, 0);
}

void* worker(void* arg) {
    incoming_connections_buf* buf = arg;
    char* rb_buf = malloc(REQUEST_BUFFER_SIZE * sizeof(char));
    request_buffer rb = {.buf = rb_buf, .buf_size = REQUEST_BUFFER_SIZE, .total_read = 0};

    while (1) {
        pthread_mutex_lock(&buf->mu);
        while (buf->size == 0) pthread_cond_wait(&buf->cond, &buf->mu);

        int* clientfd = buf->queue[buf->tail];
        buf->tail = (buf->tail + 1) % QUEUE_SIZE;
        --buf->size;
        pthread_mutex_unlock(&buf->mu);

        handle(clientfd, &rb);

        close(*clientfd);
        free(clientfd);
        rb.total_read = 0;
    }

    // actually unnecessary
    free(rb_buf);
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
    pthread_mutex_init(&data.mu, NULL);

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