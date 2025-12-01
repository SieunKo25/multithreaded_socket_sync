//thread_encdec.c
#include "thread_encdec.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define FRAME_DATA_MAX 64 //bytes
#define SYN 22

//input: ByteItem* (byte of payload data)
//output: ByteItem* ([SYN][SYN][LEN][DATA...] stream)
void* createFrame(void* arg) {
    framePipe_arg_t* p = (framePipe_arg_t*)arg;

    ByteItem *buf[FRAME_DATA_MAX];
    int count = 0;

    while(1) {
        ByteItem* item = (ByteItem*)frameq_pop(p->in);
        if(item == NULL) { 
            if(count > 0) {
                ByteItem* h1 = malloc(sizeof(ByteItem));
                ByteItem* h2 = malloc(sizeof(ByteItem));
                ByteItem* h3 = malloc(sizeof(ByteItem));
                h1->b = SYN;
                h2->b = SYN;
                h3->b = (unsigned char) count;
                frameq_push(p->out, h1);
                frameq_push(p->out, h2);
                frameq_push(p->out, h3);
                for(int i=0; i < count; i++) {
                    frameq_push(p->out, buf[i]); // push data bytes
                }
            }
            frameq_push(p->out, NULL); //indicate end of stream
            break;
        }

        buf[count++] = item;

        //if buffer is full, create frame
        if(count == FRAME_DATA_MAX) {
            ByteItem* h1 = malloc(sizeof(ByteItem));
            ByteItem* h2 = malloc(sizeof(ByteItem));
            ByteItem* h3 = malloc(sizeof(ByteItem));
            h1->b = SYN;
            h2->b = SYN;
            h3->b = (unsigned char) count;
            frameq_push(p->out, h1);
            frameq_push(p->out, h2);
            frameq_push(p->out, h3);
            for(int i=0; i < count; i++) {
                frameq_push(p->out, buf[i]); // push data bytes
            }
            count = 0; //reset count
        }
    }

    return NULL;
}

//input : ByteItem* ([SYN][SYN][LEN][DATA...] stream)
//output: BitItem* ('0' or '1' bits 1byte => 7bit)
void* charToBin(void* arg) {
    framePipe_arg_t* p = (framePipe_arg_t*)arg;

    while(1) {
        ByteItem* item = (ByteItem*)frameq_pop(p->in);
        if(item == NULL) {
            frameq_push(p->out, NULL); //end of stream
            break;
        }

        unsigned char b = item->b;
        free(item);

        //convert byte to 7 bits
        for(int i=6; i>=0; i--) {
            BitItem* bit_item = (BitItem*)malloc(sizeof(BitItem));
            bit_item->bit = ((b >> i) & 1) ? '1' : '0';
            frameq_push(p->out, bit_item);
        }
    }

    return NULL;
}

//input : BitItem* ('0' or '1' bits 1byte => 7bit)
//output : BitItem* ('0' or '1' bits 1byte => 7bit + parity bit)
void* addParity(void* arg) {
    framePipe_arg_t* p = (framePipe_arg_t*)arg;

    char bits[8]; //'0' or '1'(7bit + 1bit parity)
    int count = 1; 
    int one_count = 0;

    while(1) {
        BitItem *bit = (BitItem*)frameq_pop(p->in);
        if (bit == NULL){
            frameq_push(p->out, NULL); //end of stream
            break;
        }

        bits[count] = bit->bit;
        if(bit->bit == '1') one_count++;
        free(bit);

        count++;

        if(count == 8) {
            char parity = (one_count % 2 == 0) ? '1' : '0'; //odd parity
            bits[0] = parity;

            for(int i=0; i<8; i++) {
                BitItem* out = (BitItem*)malloc(sizeof(BitItem));
                out->bit = bits[i];
                frameq_push(p->out, out);
            }
            count = 1;
            one_count = 0;
        }
        
    }

    return NULL;
}

//input : BitItem* ('0' or '1' bits 1byte => 7bit + parity bit)
//output : BitItem* ('0' or '1' bits 1byte => 7bit)
void* removeParity(void* arg) {
    framePipe_arg_t* p = (framePipe_arg_t*)arg;

    char bits[8]; //'0' or '1'(7bit + 1bit parity)
    int count = 0;

    while(1) {
        BitItem* bit = (BitItem*)frameq_pop(p->in);
        if(bit == NULL) {
            frameq_push(p->out, NULL); //end of stream
            break;
        }

        bits[count++] = bit->bit;
        free(bit);

        if(count == 8) {
            //check parity
            int one_count = 0;
            for(int i=0; i<8; i++) {
                if(bits[i] == '1') one_count++;
            }
           
            if(one_count % 2 == 1) {
               //odd parity OK -> data bits[1..7] pass
               for(int i=1; i<8; i++) {
                    BitItem *out = (BitItem*)malloc(sizeof(BitItem));
                    out->bit = bits[i];
                    frameq_push(p->out, out);
               }
            } else {
                //parity error, skip this byte
                fprintf(stderr, "removeParity: parity error, byte skipped\n");
                exit(1);
            }
            count = 0;
        }
    }

    return NULL;
}

//input : BitItem* ('0' or '1' bits 1byte => 7bit)
//output: ByteItem* (byte of payload data)
void* binToChar(void* arg) {
    framePipe_arg_t* p = (framePipe_arg_t*)arg;

    char bits[7]; //'0' or '1'
    int count = 0;

    while(1) {
        BitItem* bit = (BitItem*)frameq_pop(p->in);
        if(bit == NULL) {
            frameq_push(p->out, NULL); //end of stream
            break;
        }

        bits[count++] = bit->bit;
        free(bit);

        if(count == 7) {
            unsigned char b = 0;
            for(int i=0; i<7; i++) {
                b = (b << 1) | (bits[i] - '0'); // '0','1' -> 0,1
            }
            ByteItem* out = (ByteItem*)malloc(sizeof(ByteItem));
            out->b = b;
            frameq_push(p->out, out);
            count = 0;
        }
    }

    return NULL;
}

//input : ByteItem* ([SYN][SYN][LEN][DATA...] stream)
//output: ByteItem* (datapayload bytes)
void* Deframe(void* arg) {
    framePipe_arg_t* p = (framePipe_arg_t*)arg;
    const unsigned char syn = SYN;

    while(1) {
        //read header 3 bytes
        ByteItem* h1 = (ByteItem*)frameq_pop(p->in);
        if(h1 == NULL) {
            frameq_push(p->out, NULL); //end of stream
            break;
        }
        ByteItem* h2 = (ByteItem*)frameq_pop(p->in);
        ByteItem* h3 = (ByteItem*)frameq_pop(p->in);

        if(h1->b != SYN || h2->b != SYN) {
            //invalid frame, skip
            free(h1);
            if(h2) free(h2);
            if(h3) free(h3);
            frameq_push(p->out, NULL); //indicate end of stream
            break;
        }

        unsigned char b0 = h1->b;
        unsigned char b1 = h2->b;
        unsigned char len = h3->b;
        free(h1);
        free(h2);
        free(h3);

        if(b0 == syn && b1 == syn && len > 0 && len <= FRAME_DATA_MAX) {
            for(int i= 0; i<len; i++) {
                ByteItem* d = (ByteItem*)frameq_pop(p->in);
                if(d == NULL) {
                    frameq_push(p->out, NULL);
                    return NULL;
                }
                frameq_push(p->out, d);
            }
        } else {
            //deframe error
            fprintf(stderr, "Deframe: SYN error or invalid length %d %d len = %d\n", b0, b1, len);
            exit(1);
        }
    }

    return NULL;
}