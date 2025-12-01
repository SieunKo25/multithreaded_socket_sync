// helper_util.h
#ifndef HELPER_UTIL_H
#define HELPER_UTIL_H

#include <stddef.h>

void helper_encode(int sock, const unsigned char *in, size_t len,
                    unsigned char **out, size_t *outlen);

void helper_decode(int sock, const unsigned char *in, size_t len,
                    unsigned char **out, size_t *outlen);

#endif // HELPER_UTIL_H