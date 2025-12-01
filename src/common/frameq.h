//frameq.h
#ifndef FRAMEQ_H
#define FRAMEQ_H

#include <pthread.h>
#include <semaphore.h>

#define FRAMEQ_CAP 536 //capacity of frame queue (size of decode frame)

typedef struct {
    void* buf[FRAMEQ_CAP];      //frame pointers
    int head;                   //next pop index  
    int tail;                   //next push index
    int count;                  //number of items 

    pthread_mutex_t mtx;        //mutex for synchronization
    sem_t items;              //semaphore for counting items
    sem_t slots;              //semaphore for counting slots
} frameq_t;

int frameq_init(frameq_t* q);
int frameq_destroy(frameq_t* q);
void frameq_push(frameq_t* q, void* item);
void* frameq_pop(frameq_t* q);

#endif