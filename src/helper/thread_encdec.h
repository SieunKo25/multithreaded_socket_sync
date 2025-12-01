//thread_encdec.h
#ifndef THREAD_ENCDEC_H
#define THREAD_ENCDEC_H

#include "frameq.h"

typedef struct{
    unsigned char b;
}ByteItem;

typedef struct {
    char bit;
} BitItem;

typedef struct {
    frameq_t* in;
    frameq_t* out;
} framePipe_arg_t;

void* createFrame(void* arg);
void* addParity(void* arg);
void* charToBin(void* arg);
void* Deframe(void* arg);
void* removeParity(void* arg);
void* binToChar(void* arg);

int helper_roundtrip_pipeline(const unsigned char *in, size_t in_len,
                                unsigned char *out, size_t *out_len);    
int helper_encode_only(const unsigned char *in, size_t in_len,
                                unsigned char *out, size_t *out_len);
int helper_decode_only(const unsigned char *in, size_t in_len,
                                unsigned char *out, size_t *out_len);        

#endif