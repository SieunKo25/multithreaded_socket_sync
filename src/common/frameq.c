//frameq.c
#include "frameq.h"
#include <stdlib.h>

int frameq_init(frameq_t* q) {
    q->head = q->tail = q->count = 0;
    
    if(pthread_mutex_init(&q->mtx, NULL) != 0) return -1;
    if(sem_init(&q->items, 0, 0) != 0) {
        pthread_mutex_destroy(&q->mtx);
        return -1;
    }
    if(sem_init(&q->slots, 0, FRAMEQ_CAP) != 0) {
        sem_destroy(&q->items);
        pthread_mutex_destroy(&q->mtx);
        return -1;
    }
    return 0;
}

int frameq_destroy(frameq_t* q) {
    if(!q) return -1;
    pthread_mutex_destroy(&q->mtx);
    sem_destroy(&q->items);
    sem_destroy(&q->slots);
    return 0;
}

void frameq_push(frameq_t* q, void* item) {
    sem_wait(&q->slots);//wait for available slot

    pthread_mutex_lock(&q->mtx);

    q->buf[q->tail] = item;
    q->tail = (q->tail + 1) % FRAMEQ_CAP;
    q->count++;

    pthread_mutex_unlock(&q->mtx);

    sem_post(&q->items);//signal that a new item is available
}

void* frameq_pop(frameq_t* q) {
    sem_wait(&q->items);//wait for available item

    pthread_mutex_lock(&q->mtx);

    void* item = q->buf[q->head];
    q->head = (q->head + 1) % FRAMEQ_CAP;
    q->count--;

    pthread_mutex_unlock(&q->mtx);

    sem_post(&q->slots);//signal that a slot is now available

    return item;
}