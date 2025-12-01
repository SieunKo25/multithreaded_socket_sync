//countq.h
#ifndef COUNTQ_H
#define COUNTQ_H

#include <pthread.h>

typedef struct {
    int idx;    //0~4 for a,e,i,o,u ; -1 for end signal
    int amount; //number of vowels counted
} count_req_t;

#define COUNTQ_CAP 16

typedef struct {
    count_req_t buf[COUNTQ_CAP];
    int head;
    int tail;
    int count;
    pthread_mutex_t mtx;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} countq_t;

int countq_init(countq_t* q);
int countq_destroy(countq_t* q);
void countq_push(countq_t* q, count_req_t req);
count_req_t countq_pop(countq_t* q);

#endif // COUNTQ_H