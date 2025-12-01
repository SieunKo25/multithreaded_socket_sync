// socket_util.c
#include "socket_util.h"
#include <unistd.h> //read, write

int read_line(int fd, char* buf, size_t maxlen) {
    size_t n = 0;
    while(n < maxlen - 1) {
        char c;
        ssize_t r = read(fd, &c, 1);
        if(r == 0) {
            if(n == 0) return 0; //error or EOF
            break;
        }
        if(r < 0) {
            return -1;
        }
        if(c == '\n') {
            break;
        }
        buf[n++] = c;
    }
    buf[n] = '\0';
    return (int)n;
}

int read_n(int fd, unsigned char* buf, size_t n) {
    size_t total = 0;
    while(total < n) {
        ssize_t r = read(fd, buf + total, n - total);
        if(r <= 0) {
            return -1; //error or EOF
        }
        total += r;
    }
    return 0;
}

int write_n(int fd, const unsigned char* buf, size_t n) {
    size_t total = 0;
    while(total < n) {
        ssize_t w = write(fd, buf + total, n - total);
        if(w <= 0) {
            return -1; //error
        }
        total += w;
    }
    return 0;
}
