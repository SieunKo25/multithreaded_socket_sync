// socket_util.h
#if SOCKET_UTIL_H
#define SOCKET_UTIL_H

#include <stddef.h>

int read_line(int fd, char* buf, size_t maxlen);
int read_n(int fd, unsigned char* buf, size_t n);
int write_n(int fd, const unsigned char* buf, size_t n);

#endif //SOCKET_UTIL_H