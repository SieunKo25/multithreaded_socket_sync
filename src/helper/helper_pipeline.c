//helper_pipeline_test.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "frameq.h"
#include "thread_encdec.h"

#define FRAME_DATA_SIZE 64 //bytes

typedef struct {
    unsigned char ch;
} EncChar;

int helper_encode_only(const unsigned char *in, size_t in_len,
                                unsigned char *out, size_t *out_len) {
    //1) Initialize encoding frame queues
    //Encoding pipeline queues
    frameq_t q_input;      //(input characters)
    frameq_t q_frame_enc;   //(createFrame output)
    frameq_t q_bin_enc;     //(charToBin -> addParity)
    frameq_t q_parity_enc;  //(addParity -> createFrame))

    if(frameq_init(&q_input)      != 0 || 
        frameq_init(&q_frame_enc)   != 0 ||
        frameq_init(&q_bin_enc)     != 0 ||
        frameq_init(&q_parity_enc)  != 0) {
        return -1;
    }

    framePipe_arg_t a_frame     = {.in = &q_input, .out = &q_frame_enc};
    framePipe_arg_t a_encode    = {.in = &q_frame_enc, .out = &q_bin_enc};
    framePipe_arg_t a_parity    = {.in = &q_bin_enc, .out = &q_parity_enc};

    //2) create encoding threads for each stage
    pthread_t th_frame, th_encode, th_parity;
    //Encoding pipeline threads
    pthread_create(&th_frame,  NULL, createFrame,   &a_frame);
    pthread_create(&th_encode, NULL, charToBin,     &a_encode);
    pthread_create(&th_parity, NULL, addParity,     &a_parity);

    //3) Push test data into input queue
    // temporary : Test String
    size_t len = in_len;
    if (len > FRAME_DATA_SIZE) {
        printf("WARNING: input too long, truncated to 64.\n");
        len = FRAME_DATA_SIZE;
    }

    //push paylod q_input
    for(size_t i=0; i < len; i++) {
        EncChar* enc_ch = (EncChar*)malloc(sizeof(EncChar));
        enc_ch->ch = (unsigned char)in[i];
        frameq_push(&q_input, enc_ch);
    }

    //4) Push NULL to indicate end of input
    frameq_push(&q_input, NULL);

    //4) output encoded msg
    size_t idx = 0;
    while(1) {
        EncChar* enc_ch = (EncChar*)frameq_pop(&q_parity_enc);
        if(enc_ch == NULL) break;
        out[idx++] = (char)(enc_ch->ch);
        free(enc_ch);
    }
    *out_len = idx;

    pthread_join(th_encode, NULL);
    pthread_join(th_parity, NULL);
    pthread_join(th_frame, NULL);

    //5) Cleanup
    frameq_destroy(&q_input);
    frameq_destroy(&q_frame_enc);
    frameq_destroy(&q_bin_enc);
    frameq_destroy(&q_parity_enc);

    return 0;
}

int helper_decode_only(const unsigned char *in, size_t in_len,
                                unsigned char *out, size_t *out_len) {
    //1)  Initialize decoding frame queues
    //decoding pipeline queues
    frameq_t q_input;  //(removeParity -> binToChar)
    frameq_t q_parity_dec;  //(removeParity -> binToChar)
    frameq_t q_bin_dec;     //(binToChar -> Deframe)
    frameq_t q_payload_out; //(decoded payload characters)

    if(frameq_init(&q_input)       != 0 ||
        frameq_init(&q_parity_dec)   != 0 ||
        frameq_init(&q_bin_dec)      != 0 ||
        frameq_init(&q_payload_out)  != 0) {
        return -1;
    }

    framePipe_arg_t a_parity_dec    = {.in = &q_input,      .out = &q_parity_dec};
    framePipe_arg_t a_decode        = {.in = &q_parity_dec, .out = &q_bin_dec};
    framePipe_arg_t a_deframe       = {.in = &q_bin_dec,    .out = &q_payload_out};

    //2) create decoding threads for each stage
    //Decoding pipeline threads
    pthread_t th_varify, th_decode, th_deframe;
    pthread_create(&th_varify,  NULL, removeParity, &a_parity_dec);
    pthread_create(&th_decode,  NULL, binToChar,    &a_decode);
    pthread_create(&th_deframe, NULL, Deframe,      &a_deframe);

    //3) Push encoded data into input queue
    for(size_t i=0; i < in_len; i++) {
        EncChar* enc_ch = (EncChar*)malloc(sizeof(EncChar));
        enc_ch->ch = (unsigned char)in[i];
        frameq_push(&q_input, enc_ch);
    }
    frameq_push(&q_input, NULL); //end of input

    //4) output decoded msg
    size_t idx = 0;
    while(1) {
        EncChar* enc_ch = (EncChar*)frameq_pop(&q_payload_out);
        if(enc_ch == NULL) break;
        out[idx++] = (char)(enc_ch->ch);
        free(enc_ch);
    }

    *out_len = idx;
    printf("Output %zu :%s\n", *out_len, out);

    pthread_join(th_deframe, NULL);
    pthread_join(th_varify, NULL);  
    pthread_join(th_decode, NULL);

    //5) Cleanup
    frameq_destroy(&q_input);
    frameq_destroy(&q_parity_dec);
    frameq_destroy(&q_bin_dec);
    frameq_destroy(&q_payload_out);

    return 0;
}


int helper_roundtrip_pipeline(const unsigned char *in, size_t in_len,
                                unsigned char *out, size_t *out_len) {
    //Encoding
    //1) Initialize encoding frame queues
    //Encoding pipeline queues
    frameq_t q_input;      //(input characters)
    frameq_t q_frame_enc;   //(createFrame output)
    frameq_t q_bin_enc;     //(charToBin -> addParity)
    frameq_t q_parity_enc;  //(addParity -> createFrame))

      if(frameq_init(&q_input)      != 0 || 
        frameq_init(&q_frame_enc)   != 0 ||
        frameq_init(&q_bin_enc)     != 0 ||
        frameq_init(&q_parity_enc)  != 0) {
        return -1;
    }

    framePipe_arg_t a_frame     = {.in = &q_input, .out = &q_frame_enc};
    framePipe_arg_t a_encode    = {.in = &q_frame_enc, .out = &q_bin_enc};
    framePipe_arg_t a_parity    = {.in = &q_bin_enc, .out = &q_parity_enc};

    //2) create encoding threads for each stage
    pthread_t th_frame, th_encode, th_parity;
    //Encoding pipeline threads
    pthread_create(&th_frame,  NULL, createFrame,   &a_frame);
    pthread_create(&th_encode, NULL, charToBin,     &a_encode);
    pthread_create(&th_parity, NULL, addParity,     &a_parity);

    //3) Push test data into input queue
    // temporary : Test String
    size_t len = in_len;
    if (len > FRAME_DATA_SIZE) {
        printf("WARNING: input too long, truncated to 64.\n");
        len = FRAME_DATA_SIZE;
    }

    //push paylod q_input
    for(size_t i=0; i < len; i++) {
        EncChar* enc_ch = (EncChar*)malloc(sizeof(EncChar));
        enc_ch->ch = (unsigned char)in[i];
        frameq_push(&q_input, enc_ch);
    }

    //4) Push NULL to indicate end of input
    frameq_push(&q_input, NULL);


    //Decoding
    //1)  Initialize decoding frame queues
    //decoding pipeline queues
    frameq_t q_parity_dec;  //(removeParity -> binToChar)
    frameq_t q_bin_dec;     //(binToChar -> Deframe)
    frameq_t q_payload_out; //(decoded payload characters)

    if(frameq_init(&q_parity_dec)   != 0 ||
       frameq_init(&q_bin_dec)      != 0 ||
       frameq_init(&q_payload_out)  != 0) {
        //clean up
        frameq_destroy(&q_input);
        frameq_destroy(&q_frame_enc);
        frameq_destroy(&q_bin_enc);
        frameq_destroy(&q_parity_enc);
        return -1;
    }

    framePipe_arg_t a_parity_dec    = {.in = &q_parity_enc, .out = &q_parity_dec};
    framePipe_arg_t a_decode        = {.in = &q_parity_dec, .out = &q_bin_dec};
    framePipe_arg_t a_deframe       = {.in = &q_bin_dec,    .out = &q_payload_out};

    //2) create decoding threads for each stage
    //Decoding pipeline threads
    pthread_t th_varify, th_decode, th_deframe;
    pthread_create(&th_varify,  NULL, removeParity, &a_parity_dec);
    pthread_create(&th_decode,  NULL, binToChar,    &a_decode);
    pthread_create(&th_deframe, NULL, Deframe,      &a_deframe);

    //4) output decoded msg
    size_t idx = 0;
    while(1) {
        EncChar* enc_ch = (EncChar*)frameq_pop(&q_payload_out);
        if(enc_ch == NULL) break;
        out[idx++] = (char)(enc_ch->ch);
        free(enc_ch);
    }

    *out_len = idx;
    printf("Output %zu :%s\n", *out_len, out);

    //5) Wait for all threads to finish
    pthread_join(th_encode, NULL);
    pthread_join(th_parity, NULL);  
    pthread_join(th_frame, NULL);
    pthread_join(th_deframe, NULL);
    pthread_join(th_varify, NULL);
    pthread_join(th_decode, NULL);

    //6) Cleanup
    frameq_destroy(&q_input);
    frameq_destroy(&q_frame_enc);
    frameq_destroy(&q_bin_enc);
    frameq_destroy(&q_parity_enc);
    frameq_destroy(&q_parity_dec);
    frameq_destroy(&q_bin_dec);
    frameq_destroy(&q_payload_out);

    return 0;    
}
