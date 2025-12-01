//strq.c
#include "strq.h"
#include <stdlib.h>
#include <stdio.h>

int strq_init(strq_t* q) {
    if(!q) return -1;
    q->head = q->tail = q->count = 0;
    if(pthread_mutex_init(&q->mtx, NULL) != 0) return -1;
    if(pthread_cond_init(&q->not_empty, NULL) != 0) {
        pthread_mutex_destroy(&q->mtx);
        return -1;
    }
    if(pthread_cond_init(&q->not_full, NULL) != 0) {
        pthread_cond_destroy(&q->not_empty);
        pthread_mutex_destroy(&q->mtx);
        return -1;
    }
    return 0;
}

int strq_destroy(strq_t* q) {
    if(!q) return -1;
    pthread_mutex_destroy(&q->mtx);
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
    return 0;
}

void strq_push(strq_t* q, char* s) {
    pthread_mutex_lock(&q->mtx);

    while(q->count == STRQ_CAP) {
        pthread_cond_wait(&q->not_full, &q->mtx);
    }

    q->buf[q->tail] = s;
    q->tail = (q->tail + 1) % STRQ_CAP;
    q->count++;

    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mtx);
}

char* strq_pop(strq_t* q) {
    pthread_mutex_lock(&q->mtx);

    while(q->count == 0) {
        pthread_cond_wait(&q->not_empty, &q->mtx);
    }

    char* s = q->buf[q->head];
    q->head = (q->head + 1) %STRQ_CAP;
    q->count--;

    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->mtx);

    return s;
}
