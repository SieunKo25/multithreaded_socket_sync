// helper/helper_util.c

#include "helper_util.h"
#include "socket_util.h"
#include "socket_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void helper_encode(int sock, const unsigned char *in, size_t len,
                    unsigned char **out, size_t *outlen) {
    char header[64];
    int hdr_len = snprintf(header, sizeof(header), "ENCODE %zu\n", len);

    if(write_n(sock, (unsigned char*)header, hdr_len) < 0 ||
       write_n(sock, in, len) < 0) {
        perror("server_helper_encode_write");
        exit(1);
    }

    //read response header
    char buf[MAX_LINE];
    ssize_t n = read_line(sock, buf, sizeof(buf)-1);
    if(n <= 0) {
        perror("server_helper_encode_read_header");
        exit(1);
    }
    buf[n] = '\0';

    char cmd[16];
    size_t rlen;
    if(sscanf(buf, "%s %zu", cmd, &rlen) < 2 || strcmp(cmd, "OK") != 0) {
        fprintf(stderr, "[server] invalid response from helper: %s\n", buf);
        exit(1);
    }

    *outlen = rlen;
    *out = (unsigned char*)malloc(rlen);
    
    if(read_n(sock, *out, rlen) < 0) {
        perror("server_helper_encode_read_payload");
        free(*out);
        exit(1);
    }
              
}

void helper_decode(int sock, const unsigned char *in, size_t len,
                    unsigned char **out, size_t *outlen) {
    char header[64];
    int hdr_len = snprintf(header, sizeof(header), "DECODE %zu\n", len);

    if(write_n(sock, (unsigned char*)header, hdr_len) < 0 ||
       write_n(sock, in, len) < 0) {
        perror("server_helper_decode_write");
        exit(1);
    }

    //read response header
    char buf[MAX_LINE];
    ssize_t n = read_line(sock, buf, sizeof(buf)-1);
    if(n <= 0) {
        perror("server_helper_decode_read_header");
        exit(1);
    }
    buf[n] = '\0';

    char cmd[16];
    size_t rlen;
    if(sscanf(buf, "%s %zu", cmd, &rlen) < 2 || strcmp(cmd, "OK") != 0) {
        fprintf(stderr, "[server] invalid response from helper: %s\n", buf);
        exit(1);
    }

    *outlen = rlen;
    *out = (unsigned char*)malloc(rlen+1);
    
    if(read_n(sock, *out, rlen) < 0) {
        perror("server_helper_decode_read_payload");
        free(*out);
        exit(1);
    }
    (*out)[rlen] = '\0';
}