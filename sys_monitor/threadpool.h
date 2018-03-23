#ifndef THREADPOOL_H
#define THREADPOOL_H

typedef unsigned long int pthread_t;

//线程池
struct threadpool
{
    int thread_num; //线程数量
    pthread_t *pthreads; //线程pid
    struct task* thead; //管理任务
    struct task* ttail;
};

//线程具体执行的任务
struct task
{
	void* (*func)(void); //回调函数
	struct task *next;
};

struct threadpool* threadpool_init(int thread_num);
int add_task(struct threadpool* tp, void* (*call)(void));
void* tp_func(void* arg);
void threadpool_destroy(void);

#endif