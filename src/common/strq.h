//strq.h
#ifndef STRQ_H
#define STRQ_H

#include <pthread.h>

//fixed size 5
#define STRQ_CAP 5

typedef struct {
    char* buf[STRQ_CAP];        //string pointers
    int head;                   //next pop index  
    int tail;                   //next push index
    int count;                  //number of items 

    pthread_mutex_t mtx;        //mutex for synchronization
    pthread_cond_t not_empty;   //condition variable for not empty
    pthread_cond_t not_full;    //condition variable for not full
} strq_t;

int strq_init(strq_t* q);
int strq_destroy(strq_t* q);
void strq_push(strq_t*q, char* str);
char* strq_pop(strq_t* q);

#endif