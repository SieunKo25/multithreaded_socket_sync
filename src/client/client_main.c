//client/client_main.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "socket_common.h"
#include "socket_util.h"
#include "helper_util.h"
#include "filename.h"

int main(void) {
    /* socket() -- for helper */
    int hsockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in haddr;
    haddr.sin_family = AF_INET;
    haddr.sin_port = htons(HELPER_PORT);
    if(inet_pton(AF_INET, "127.0.0.1", &haddr.sin_addr) <= 0) {
        perror("client_helper_inet_pton");
        close(hsockfd);
        exit(1);
    }
    if(connect(hsockfd, (struct sockaddr*)&haddr, sizeof(haddr)) < 0) {
        perror("client_helper_connect");
        close(hsockfd);
        exit(1);
    }

    /* socket() -- for server */
    int ssockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(SERVER_PORT);
    if(inet_pton(AF_INET, "127.0.0.1", &saddr.sin_addr) <= 0) {
        perror("client_server_inet_pton");
        close(ssockfd);
        close(hsockfd);
        exit(1);
    }
    if(connect(ssockfd, (struct sockaddr*)&saddr, sizeof(saddr)) < 0) {
        perror("client_server_connect");
        close(ssockfd);
        close(hsockfd);
        exit(1);
    }

    // 1) read input file and send in 64-byte chunks
    FILE* fin = fopen(INPUT_FILE_1, "r");
    if(fin == NULL) {
        perror("client_fopen_input");
        close(ssockfd);
        close(hsockfd);
        exit(1);
    }
    
    char input[65]; // max size : 64 + null
    size_t len = 0;
    
    // Send all data chunks
    while(1) {
        len = fread(input, 1, sizeof(input)-1, fin);
        if(len <= 0) break;
        
        input[len] = '\0';
        
        // 2) client -> helper [ENCODE]
        unsigned char* frame = NULL;
        size_t frame_len = 0;
        helper_encode(hsockfd, (unsigned char*)input, len, &frame, &frame_len);
        
        // 3) client -> server [DATA]
        char header[64];
        int n = snprintf(header, sizeof(header), "DATA %zu\n", frame_len);
        if(write_n(ssockfd, (unsigned char*)header, n) < 0 ||
           write_n(ssockfd, frame, frame_len) < 0) {
            perror("client_server_write");
            free(frame);
            fclose(fin);
            close(ssockfd);
            close(hsockfd);
            exit(1);
        }
        free(frame);
    }
    fclose(fin);
    
    // 4) Send END signal
    char end_cmd[] = "END\n";
    if(write_n(ssockfd, (unsigned char*)end_cmd, strlen(end_cmd)) < 0) {
        perror("client_server_write_end");
        close(ssockfd);
        close(hsockfd);
        exit(1);
    }
    
    // 5) server -> client [VOCOUNT] chunks : receive and decode all chunks
    char* accumulated_result = NULL;
    size_t total_result_len = 0;
    
    while(1) {
        char line[MAX_LINE];
        if(read_line(ssockfd, line, sizeof(line)-1) < 0) {
            perror("client_server_read_header");
            free(accumulated_result);
            close(ssockfd);
            close(hsockfd);
            exit(1);
        }
        
        char cmd[16];
        size_t vflen;
        if(sscanf(line, "%s", cmd) < 1) {
            fprintf(stderr, "[client] invalid cmd from server: %s\n", line);
            free(accumulated_result);
            close(ssockfd);
            close(hsockfd);
            exit(1);
        }
        
        // Check for END signal
        if(strcmp(cmd, "END") == 0) {
            printf("[client] received END from server\n");
            break;
        }
        
        // Process VOCOUNT chunk
        if(sscanf(line, "%s %zu", cmd, &vflen) < 2 || strcmp(cmd, "VOCOUNT") != 0) {
            fprintf(stderr, "[client] invalid VOCOUNT cmd: %s\n", line);
            free(accumulated_result);
            close(ssockfd);
            close(hsockfd);
            exit(1);
        }
        
        unsigned char* vframe = (unsigned char*)malloc(vflen);
        if(read_n(ssockfd, vframe, vflen) < 0) {
            perror("client_server_read_vframe");
            free(vframe);
            free(accumulated_result);
            close(ssockfd);
            close(hsockfd);
            exit(1);
        }
        
        // 6) client -> helper [DECODE]
        unsigned char* vcount_msg = NULL;
        size_t vcount_len = 0;
        helper_decode(hsockfd, vframe, vflen, &vcount_msg, &vcount_len);
        
        // Accumulate decoded result
        accumulated_result = (char*)realloc(accumulated_result, total_result_len + vcount_len + 1);
        memcpy(accumulated_result + total_result_len, vcount_msg, vcount_len);
        total_result_len += vcount_len;
        
        free(vframe);
        free(vcount_msg);
    }
    
    if(accumulated_result) {
        accumulated_result[total_result_len] = '\0';
        printf("[client] Vowel Count Result:\n%s\n", accumulated_result);
        
        // 7) store receivedVowelCount.txt
        FILE *fw = fopen(OUTPUT_FILE_1, "w");
        if(fw == NULL) {
            perror("client_fopen_output");
        } else {
            fwrite(accumulated_result, 1, total_result_len, fw);
            fclose(fw);
        }
        
        free(accumulated_result);
    }

    close(ssockfd);
    close(hsockfd);


    return 0;
}