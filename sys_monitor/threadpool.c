#include "threadpool.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>  
#include <assert.h>
#include <ctype.h> 

//初始化线程池
struct threadpool* threadpool_init(int thread_num)
{
	struct threadpool *tp = NULL;
	int i;
	int err;
	tp = malloc(sizeof(struct threadpool));
	if(tp == NULL)
	{
		printf("failed to malloc threadpool!\n");
		return NULL;
	}
	//设置开始时线程池的线程数量
	tp->thread_num = thread_num;
	tp->thead = NULL;
	tp->ttail = NULL;
	tp->pthreads = malloc(sizeof(pthread_t) * thread_num);
	if(tp->pthreads == NULL)
	{
		printf("failed to malloc pthreads!\n");
		free(tp);
		return NULL;
	}
	//创建初始线程
	for(i = 0; i < tp->thread_num; ++i)
	{
		err = pthread_create(&(tp->pthreads[i]), NULL, tp_func, (void*)tp);
		if(err == -1)
		{
			printf("failed to create all threads!\n");
			free(tp->pthreads);
			free(tp);
			return NULL;
		}
	}
	return tp;
}

//向线程池里面增加任务
int add_task(struct threadpool* tp, void* (*call)(void))
{
	assert(tp != NULL);
	assert(call != NULL);

    struct task *ptask =(struct task*) malloc(sizeof(struct task));
    ptask->func = call;    
    ptask->next = NULL;
    //加入管理任务的链表中
    if(tp->thead == NULL)
    {
        tp->thead = ptask; 
        tp->ttail = ptask;
    }
    else
    {
        tp->ttail->next = ptask;
        tp->ttail = ptask; 
    }
    return 0;
}

//线程池销毁
void threadpool_destroy(void) 
{
	printf("destroy threadpool!");
}

//从任务管理链表里取出任务，然后分配给线程完成
void* tp_func(void* arg)
{
    struct threadpool *tp = (struct threadpool*)arg;
    assert(tp);

    struct task *ptask = NULL;
    while (1)  
    {
        ptask = tp->thead;
        if(tp->thead == tp->ttail)
        {
        	if(tp->thead == NULL)
        	{
        		continue;
        	}
        	tp->thead = NULL;
        	tp->ttail = NULL;
        }
        else
        {
        	tp->thead = ptask->next;
        }
        (ptask->func)();
        free(ptask);
        ptask = NULL;    
    }
}