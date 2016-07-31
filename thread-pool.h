#ifndef THREAD_POOL_H
#define THREAD_POOL_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>

#define MANAGE_SPACE 3	//manager_thread sleep time
#define MANAGE_PNUM 10	//manager_thread operating number of threads

typedef void *(*taskfun)(void *);

typedef struct
{
	void *para;
	taskfun fun;
}task_description;

typedef struct
{
	pthread_mutex_t p_mut;
	pthread_cond_t p_queue_fill;
	pthread_cond_t p_queue_empty;

	int p_thread_max;
	int p_thread_min;
	int p_thread_live;
	int p_thread_work;
	int p_exit_num;
	//pthread_t *p_work_tid;
	pthread_t p_manage_tid;

	task_description *p_task_queue;
	int p_queue_max;
	int p_queue_size;
	int p_queue_front;
	int p_queue_back;

	int shutdown;
	
	pthread_attr_t p_attr;
}thread_p;

thread_p* threadpl_create(int min_pthread, int max_pthread, int queue_size);
int threadpl_add(thread_p *pool, taskfun fun, void *para);
void* threadpl_work(void *para);
void* threadpl_manage(void *para);
int threadpl_distroy(thread_p **pool);

#endif
