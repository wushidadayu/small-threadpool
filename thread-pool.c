#include "thread-pool.h"

thread_p* threadpl_create(int min_pthread, int max_pthread, int queue_size)
{
	if(min_pthread > max_pthread || min_pthread < 5 || queue_size < 5)
	{
		printf("thread create parameters error\n");
		return NULL;
	}
	thread_p *pool = NULL;
	pthread_t tid;
	int i;
	do{
		if(NULL == (pool = malloc(sizeof(thread_p))))
		{
			printf("P%d, malloc\n", __LINE__);
			break;
		}
		memset(pool, 0, sizeof(thread_p));
		if(pthread_mutex_init(&pool->p_mut, NULL)
				|| pthread_cond_init(&pool->p_queue_fill, NULL)
				|| pthread_cond_init(&pool->p_queue_empty,NULL))
		{
			printf("P%d, init mutex\n", __LINE__);
			break;
		}
		pool->p_thread_max = max_pthread;
		pool->p_thread_min = min_pthread;
		pool->p_queue_max = queue_size;
		pool->shutdown = 1;
		//if(NULL == (pool->p_work_tid = malloc(sizeof(pthread_t)*pool->p_thread_max)))
		//{
			//printf("P%d, malloc p_work_tid\n", __LINE__);
			//break;
		//}
		if(NULL == (pool->p_task_queue = malloc(sizeof(task_description) * queue_size)))
		{
			printf("P%d malloc p_task_queue\n", __LINE__);
			break;
		}

		pthread_attr_init(&pool->p_attr);
		pthread_attr_setdetachstate(&pool->p_attr, PTHREAD_CREATE_DETACHED);
		pthread_create(&pool->p_manage_tid, &pool->p_attr, threadpl_manage, (void *)pool);
		for(pool->p_thread_live=0; pool->p_thread_live<min_pthread; ++pool->p_thread_live)
			pthread_create(&tid, &pool->p_attr, threadpl_work, (void *)pool);
		return pool;
	}while(0);
	threadpl_distroy(&pool);
	return NULL;
}

int threadpl_add(thread_p *pool, taskfun fun, void *para)
{
	if(NULL == pool)
		return -1;
	pthread_mutex_lock(&pool->p_mut);
	if(!pool->shutdown)
	{
		pthread_mutex_unlock(&pool->p_mut);
		return -2;
	}
	while(pool->p_queue_size >= pool->p_queue_max)
	{
		if(pthread_cond_wait(&pool->p_queue_fill, &pool->p_mut)!=0)
			return -3;
	}
	pool->p_task_queue[pool->p_queue_back].para = para;
	pool->p_task_queue[pool->p_queue_back].fun = fun;
	pool->p_queue_back = (pool->p_queue_back + 1)%pool->p_queue_max;
	pool->p_queue_size++;
	
	pthread_cond_signal(&pool->p_queue_empty);
	pthread_mutex_unlock(&pool->p_mut);
	return 0;
}

void* threadpl_work(void *para)
{
	thread_p *pool = (thread_p*)para;
	task_description tmp_task;
	while(1)
	{
		pthread_mutex_lock(&pool->p_mut);
		while((0 == pool->p_queue_size) && pool->shutdown)
		{
			pthread_cond_wait(&pool->p_queue_empty, &pool->p_mut);
			if(pool->p_exit_num > 0)
			{
				pool->p_exit_num--;
				if(pool->p_thread_live > pool->p_thread_min)
				{
					pool->p_thread_live--;
					pthread_mutex_unlock(&pool->p_mut);
					pthread_exit(NULL);
				}
			}
		}
		if(!pool->shutdown)
		{
			pool->p_thread_live--;
			pthread_mutex_unlock(&pool->p_mut);
			pthread_exit(NULL);
		}
		pool->p_queue_size--;
		memcpy(&tmp_task, &pool->p_task_queue[pool->p_queue_front],sizeof(task_description));
		pool->p_queue_front = (pool->p_queue_front + 1)%pool->p_queue_max;
		pthread_cond_signal(&pool->p_queue_fill);
		pool->p_thread_work++;
		pthread_mutex_unlock(&pool->p_mut);
		
		tmp_task.fun(tmp_task.para);

		pthread_mutex_lock(&pool->p_mut);
		pool->p_thread_work--;
		pthread_mutex_unlock(&pool->p_mut);
	}
	pthread_exit(NULL);
}

void* threadpl_manage(void *para)
{
	thread_p *pool = (thread_p*)para;
	int i, add;
	pthread_attr_t attr;
	pthread_t tid;
	
	while(1)
	{
		sleep(MANAGE_SPACE);
		pthread_mutex_lock(&pool->p_mut);
		if(pool->shutdown)
		{
			pthread_mutex_unlock(&pool->p_mut);
			pthread_exit(NULL);
		}
		//检查是否需要增加线程
		//i = -1;
		add = 0;
		while(pool->p_queue_size > pool->p_thread_min 
				&& pool->p_thread_live < pool->p_thread_max
				&& add < MANAGE_PNUM)
		{
			//if(NULL != pool->p_work_tid[++i])
			//	continue;
			pthread_create(&tid, &pool->p_attr, threadpl_work, para);
			pool->p_thread_live++;
			add++;
		}
		pthread_mutex_unlock(&pool->p_mut);

		//检查是否需要销毁空闲线程
		pthread_mutex_lock(&pool->p_mut);
		if(pool->p_thread_work*2 < pool->p_thread_live 
				&& pool->p_thread_work > pool->p_thread_min)
		{
			pool->p_exit_num = MANAGE_PNUM;
			pthread_mutex_unlock(&pool->p_mut);

			for(i = 0; i<MANAGE_PNUM; ++i)
				pthread_cond_signal(&pool->p_queue_empty);
		}else
			pthread_mutex_unlock(&pool->p_mut);
	}
	return NULL;
}

int threadpl_distroy(thread_p **p)
{
	if(NULL == p && NULL == *p)
		return 0;
	thread_p *pool = *p;

	pthread_mutex_lock(&pool->p_mut);
	pool->shutdown = 0;
	if(pool->p_thread_live > 0)
		pthread_cond_broadcast(&pool->p_queue_empty);
	pthread_mutex_unlock(&pool->p_mut);
	
	
	int i = 4;
	pthread_mutex_lock(&pool->p_mut);
	while(pool->p_thread_live > 0 && i--)		
	{
		pthread_mutex_unlock(&pool->p_mut);
		usleep(500000);
		pthread_mutex_lock(&pool->p_mut);
		
	}
	if(NULL != pool->p_task_queue)
		free(pool->p_task_queue);
	//if(NULL != pool->p_work_tid)
	//	free(pool->p_work_tid);
	printf("threadpool close , %d\n", pool->p_thread_live);
	free(pool);
	*p = NULL;
	return 0;
}

