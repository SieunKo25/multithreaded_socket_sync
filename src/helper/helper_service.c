//helper_service.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "socket_common.h"
#include "thread_encdec.h"
#include "socket_util.h"


static int run_encode(const unsigned char *in, size_t in_len, unsigned char *out, size_t *out_len) {
    return helper_encode_only(in, in_len, out, out_len);
} 

static int run_decode(const unsigned char *in, size_t in_len, unsigned char *out, size_t *out_len) {
    return helper_decode_only(in, in_len, out, out_len);
}

// Thread function to handle each client connection
void* handle_client(void* arg) {
    int conn_fd = *(int*)arg;
    free(arg);
    
    char buf[MAX_LINE];
    
    fprintf(stderr, "[helper] client connected (fd=%d)!\n", conn_fd);

    // communication loop
    while(1) {
        //read command line
        ssize_t n = read_line(conn_fd, buf, sizeof(buf)-1);
        if(n <= 0) {
            if(n == 0) {
                fprintf(stderr, "[helper] client (fd=%d) disconnected\n", conn_fd);
            } else {
                perror("helper_read");
            }
            break;
        }
        buf[n] = '\0'; // ensure null termination

        fprintf(stderr, "[helper] (fd=%d) received line: '%s'\n", conn_fd, buf);

        // Encode len/ Decode len / Quit
        char cmd[16];
        size_t len = 0;
        int parsed = sscanf(buf,"%15s %zu", cmd, &len);
        
        if(parsed < 2) {
            fprintf(stderr, "[helper] (fd=%d) invalid command: %s\n", conn_fd, buf);
            continue;
        }

        if(strcmp(cmd, "QUIT") == 0) {
            fprintf(stderr, "[helper] (fd=%d) received QUIT command. Closing connection...\n", conn_fd);
            break;
        }

        //read payload
        unsigned char *inbuf = malloc(len);
        unsigned char outbuf[1024];
        size_t out_len = 0;

        if(read_n(conn_fd, inbuf, len) < 0) {
            perror("helper_read_payload");
            free(inbuf);
            break;
        }
        fprintf(stderr, "[helper] (fd=%d) received cmd %s, %zu bytes of payload.\n", conn_fd, cmd, len);

        int rc = -1;
        if(strcmp(cmd, "ENCODE") == 0) {
            rc = run_encode(inbuf, len, outbuf, &out_len);
        } else if(strcmp(cmd, "DECODE") == 0) {
            rc = run_decode(inbuf, len, outbuf, &out_len);
        } else {
            fprintf(stderr, "[helper] (fd=%d) unknown command: %s\n", conn_fd, cmd);
            free(inbuf);
            continue;
        }
        free(inbuf);

        if(rc != 0) {
            const char* err_msg = "ERROR\n";
            write_n(conn_fd, (unsigned char*)err_msg, strlen(err_msg));
            continue;
        }

        // send response, payload
        char header[64];
        int hdr_len = snprintf(header, sizeof(header), "OK %zu\n", out_len);
        if(write_n(conn_fd, (unsigned char*)header, hdr_len) < 0 ||
           write_n(conn_fd, outbuf, out_len) < 0) {
            perror("helper_write_response");
            break;
        }
        fprintf(stderr, "[helper] (fd=%d) sent response: OK %zu bytes\n", conn_fd, out_len);
    }

    close(conn_fd);
    fprintf(stderr, "[helper] (fd=%d) connection closed.\n", conn_fd);
    return NULL;
}


int main(void) {
    int listen_fd;
    struct sockaddr_in addr;

    // 1) socket()
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(listen_fd < 0) {
        perror("helper_socket");
        exit(1);
    }

    // 2) bind()
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(HELPER_PORT);

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if(bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("helper_bind");
        close(listen_fd);
        exit(1);
    }

    // 3) listen()
    if(listen(listen_fd, 5) < 0) {  // increased backlog to 5
        perror("helper_listen");
        close(listen_fd);
        exit(1);
    }

    fprintf(stderr, "[helper] listening on port %d...\n", HELPER_PORT);

    // 4) accept loop - create thread for each connection
    while(1) { 
        int* conn_fd = (int*)malloc(sizeof(int));
        *conn_fd = accept(listen_fd, NULL, NULL);
        if(*conn_fd < 0) {
            perror("helper_accept");
            free(conn_fd);
            continue;  // don't exit, just continue accepting
        }

        // Create detached thread to handle this connection
        pthread_t tid;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        
        if(pthread_create(&tid, &attr, handle_client, conn_fd) != 0) {
            perror("pthread_create");
            close(*conn_fd);
            free(conn_fd);
        }
        
        pthread_attr_destroy(&attr);
    }

    close(listen_fd);
    return 0;
}
