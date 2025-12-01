//server_vowel_pipeline_test.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "countq.h"
#include "strq.h"
#include "vowel_pipeline.h"

//totals[0] = aA, [1] = eE, [2] = iI, [3] = oO, [4] = uU
typedef struct {
    char lower;
    char upper;
    int idx;    //totals index
    strq_t *in_q;
    strq_t *out_q;
    countq_t * cnt_q;
} ch_worker_arg_t;

typedef struct {
    countq_t* cnt_q;
    int* totals;
} count_thread_arg_t;

void* count_thread(void *arg_void) {
    count_thread_arg_t* arg = (count_thread_arg_t*)arg_void;
    int* totals = arg->totals;

    while(1){
        count_req_t req = countq_pop(arg->cnt_q);
        if(req.idx == -1) {
            break;
        }

        if(req.idx >=0 && req.idx <5) {
            totals[req.idx] += req.amount;
        }
    }
    return NULL;
}

void* ch_worker(void* arg_void) {
    ch_worker_arg_t* arg = (ch_worker_arg_t*)arg_void;

    while(1) {
        char *s = strq_pop(arg->in_q);
        if(s == NULL) {
            if(arg->out_q) strq_push(arg->out_q, NULL);
            break;
        }

        //1) count vowels
        for(char*p = s; *p; ++p){
            if(*p == arg->lower || *p == arg->upper) {
                if(arg->cnt_q) {
                    count_req_t req;
                    req.idx = arg->idx;
                    req.amount = 1;
                    countq_push(arg->cnt_q, req);
                }    
            }
        }

        //temporary : pass through to next step
        if(arg->out_q) {
            strq_push(arg->out_q, s);
        } else {
            free(s);
        }
    }
    
    return NULL;
}


int vowel_pipeline_run(const char *input) {
    //1) prepare queues
    //string queues
    strq_t qA, qE, qI, qO, qU;
    if(strq_init(&qA) != 0 || strq_init(&qE) != 0 || strq_init(&qI) != 0 ||
       strq_init(&qO) != 0 || strq_init(&qU) != 0) {
        fprintf(stderr, "strq_init_failed\n");
        return 1;
    }

    //count queue
    countq_t count_q;
    if(countq_init(&count_q) != 0) {
        fprintf(stderr, "countq_init_failed\n");
        return 1;
    }


    //2) create count thread
    int totals[5] = {0};
    pthread_t thCount;
    count_thread_arg_t cnt_arg = { .cnt_q = &count_q, .totals = totals };
    pthread_create(&thCount, NULL, count_thread, &cnt_arg);

    //3) create worker threads for each vowel
    ch_worker_arg_t a_arg = {'a', 'A', 0, &qA, &qE, &count_q};
    ch_worker_arg_t e_arg = {'e', 'E', 1, &qE, &qI, &count_q};
    ch_worker_arg_t i_arg = {'i', 'I', 2, &qI, &qO, &count_q};
    ch_worker_arg_t o_arg = {'o', 'O', 3, &qO, &qU, &count_q};
    ch_worker_arg_t u_arg = {'u', 'U', 4, &qU, NULL, &count_q};  

    pthread_t thA, thE, thI, thO, thU;

    //4) create worker threads
    pthread_create(&thA, NULL, ch_worker, &a_arg);
    pthread_create(&thE, NULL, ch_worker, &e_arg);
    pthread_create(&thI, NULL, ch_worker, &i_arg);
    pthread_create(&thO, NULL, ch_worker, &o_arg);
    pthread_create(&thU, NULL, ch_worker, &u_arg);

    //5) push input lines into first queue
    char *s = strdup(input);
    if(!s) {
        perror ("[vowel_pipeline] strdup");
        strq_push(&qA, NULL);
    } else {
        strq_push(&qA, s);
        strq_push(&qA, NULL);
    }

    //6) push NULL to first queue to signal end
    strq_push(&qA, NULL);

    //7) wait for worker threads to finish
    pthread_join(thA, NULL);
    pthread_join(thE, NULL);
    pthread_join(thI, NULL);
    pthread_join(thO, NULL);
    pthread_join(thU, NULL);

    //send end signal to count thread
    count_req_t end_req = { .idx = -1, .amount = 0 };
    countq_push(&count_q, end_req);
    pthread_join(thCount, NULL);

    //8) print totals and create vowelCount.txt
    printf("\n[Totals]\n");
    printf("a/A: %d\n", totals[0]);
    printf("e/E: %d\n", totals[1]);
    printf("i/I: %d\n", totals[2]);
    printf("o/O: %d\n", totals[3]);
    printf("u/U: %d\n", totals[4]);

    FILE *f = fopen("vowelCount.txt", "w");
    if(!f) {
        perror("fopen vowelCount.txt");
        return 1;
    } else {
        fprintf(f, "Totals\n");
        fprintf(f, "Letter aA : %d\n", totals[0]);
        fprintf(f, "Letter eE : %d\n", totals[1]);
        fprintf(f, "Letter iI : %d\n", totals[2]);
        fprintf(f, "Letter oO : %d\n", totals[3]);
        fprintf(f, "Letter uU : %d\n", totals[4]);
        fclose(f);
        printf("[INFO] Wrote vowelCount.txt\n");
    }

    //9) cleanup
    strq_destroy(&qA);
    strq_destroy(&qE);
    strq_destroy(&qI);
    strq_destroy(&qO);
    strq_destroy(&qU);
    countq_destroy(&count_q);

    return 0;
}