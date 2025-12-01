// server_main.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "socket_common.h"
#include "socket_util.h"
#include "helper_util.h"
#include "vowel_pipeline.h"

static int process_vowel(const char* input) {
    if(vowel_pipeline_run(input) != 0) {
        fprintf(stderr, "[server] vowel_pipeline_run failed\n");
        return -1;
    }

    return 0;
}

int main(void) {
  
    /* socket() -- for helper */
    int hsockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in haddr;
    haddr.sin_family = AF_INET;
    haddr.sin_port = htons(HELPER_PORT);
    if(inet_pton(AF_INET, "127.0.0.1", &haddr.sin_addr) <= 0) {
        perror("server_helper_inet_pton");
        close(hsockfd);
        exit(1);
    }
    if(connect(hsockfd, (struct sockaddr*)&haddr, sizeof(haddr)) < 0) {
        perror("server_helper_connect");
        close(hsockfd);
        exit(1);
    }

    /* listen() -- for client */
    int ssockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in saddr = {0};
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(SERVER_PORT);
    saddr.sin_addr.s_addr = INADDR_ANY;

    if(bind(ssockfd,(struct sockaddr*)&saddr, sizeof(saddr)) < 0) {
        perror("server_bind");
        close(ssockfd);
        exit(1);
    }
    if(listen(ssockfd, 1) < 0) {
        perror("server_listen");
        close(ssockfd);
        exit(1);
    }
    printf("[server] waiting client...\n");
    while(1) {
        int csock = accept(ssockfd, NULL, NULL);
        printf("[server] client connected!\n");

        // Buffer to accumulate all decoded data
        char* accumulated_text = NULL;
        size_t total_len = 0;

        while(1) {
            // 1) client -> server [DATA] or [END]
            char buf[MAX_LINE];
            int r = read_line(csock, buf, sizeof(buf)-1);
            if(r < 0) {
                perror("server_read_header");
                free(accumulated_text);
                close(csock);
                close(ssockfd);
                exit(1);
            }

            if(r == 0) {
                printf("[server] client disconnected.\n");
                free(accumulated_text);
                close(csock);
                break;
            }

            char cmd[16];
            size_t flen;
            if(sscanf(buf, "%s %zu", cmd, &flen) < 1) {
                fprintf(stderr, "[server] invalid cmd from client: %s\n", buf);
                free(accumulated_text);
                close(csock);
                close(ssockfd);
                exit(1);
            }

            // Check if END command
            if(strcmp(cmd, "END") == 0) {
                printf("[server] received END, processing vowel count\n");
                
                // 3) Count Vowel on accumulated text
                if(accumulated_text && total_len > 0) {
                    accumulated_text[total_len] = '\0';
                    printf("[server] total decoded input= %s\n", accumulated_text);
                    
                    if(process_vowel(accumulated_text) != 0) {
                        fprintf(stderr, "[server] process_vowel failed\n");
                        free(accumulated_text);
                        close(csock);
                        close(ssockfd);
                        exit(1);
                    }
                }

                //4) server -> helper [ENCODE] and send to client in chunks
                FILE* fin = fopen("vowelCount.txt", "r");
                if(fin == NULL) {
                    perror("fopen vowelCount.txt");
                    free(accumulated_text);
                    close(csock);
                    close(ssockfd);
                    exit(1);
                }
                
                char chunk[65];
                size_t chunk_len;
                
                // Send all chunks
                while((chunk_len = fread(chunk, 1, 64, fin)) > 0) {
                    unsigned char *vframe = NULL;
                    size_t vflen = 0;
                    helper_encode(hsockfd, (unsigned char*)chunk, chunk_len, &vframe, &vflen);
                    
                    //5) server ->client [VOCOUNT] : VCOUNT frame chunk
                    int n = snprintf(buf, sizeof(buf), "VOCOUNT %zu\n", vflen);
                    if(write_n(csock, (unsigned char*)buf, n) < 0 ||
                       write_n(csock, vframe, vflen) < 0) {
                        perror("server_write_data");
                        free(accumulated_text);
                        free(vframe);
                        fclose(fin);
                        close(csock);
                        close(ssockfd);
                        exit(1);
                    }
                    free(vframe);
                }
                fclose(fin);
                
                // Send END signal
                char end_signal[] = "END\n";
                if(write_n(csock, (unsigned char*)end_signal, strlen(end_signal)) < 0) {
                    perror("server_write_end");
                    free(accumulated_text);
                    close(csock);
                    close(ssockfd);
                    exit(1);
                }
                printf("[server] sent VOCOUNT chunks and END to client\n");

                free(accumulated_text);
                accumulated_text = NULL;
                total_len = 0;
                continue;
            }

            // Process DATA command
            unsigned char* frame_in = (unsigned char*)malloc(flen);
            if(read_n(csock, frame_in, flen) < 0) {
                perror("server_read_frame");
                free(frame_in);
                free(accumulated_text);
                close(csock);
                close(ssockfd);
                exit(1);
            }

            // 2) server -> helper [DECODE] 
            unsigned char *plain = NULL;
            size_t plain_len = 0;
            helper_decode(hsockfd, frame_in, flen, &plain, &plain_len);
            
            // Accumulate decoded text
            accumulated_text = (char*)realloc(accumulated_text, total_len + plain_len + 1);
            memcpy(accumulated_text + total_len, plain, plain_len);
            total_len += plain_len;

            free(frame_in);
            free(plain);
        }
    }

    close(ssockfd);
    close(hsockfd);

    return 0;

}