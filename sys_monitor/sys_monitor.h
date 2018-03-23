#ifndef SYS_MONITOR_H
#define SYS_MONITOR_H

#include <sys/types.h>
#include <stdio.h> 

#define NAME_LEN 64
#define CMD_LEN 256
#define TS_LEN 64
#define DELAY 500000
#define INIT_NUM 2
#define MAP_NUM 512
#define FN_LEN 32
#define BUF_LEN 256
#define MAP_LEN 64

#define panic(...) { fprintf(stderr, __VA_ARGS__); exit(EXIT_FAILURE); } 

typedef unsigned long long u64;
typedef u64 map_t;

//cpu information
struct cpu_info 
{  
    u64 utime;
    u64 ntime; 
    u64 systime;
    u64 idltime;  //idle time
    u64 iotime;
    u64 irqtime;
    u64 sirqtime;
    u64 sttime;
   	u64 gtime;   
};

//memory information
struct mem_info
{
    u64 memtotal;
    u64 memfree;
    u64 buffers;
    u64 cached; 
}; 

//proc information
struct proc_info 
{   
    pid_t pid;  
    pid_t ppid;  
    char name[NAME_LEN];  
    char cmd[CMD_LEN];
    struct proc_info* next;
    struct proc_info* pre;
}; 

//用链表来记录进程的pid
//增量修改，这些是上次log里存在而这次写入不需要的进程信息
struct rm_pid
{
	pid_t pid;
	struct rm_pid* next;
};

struct sys_info
{
	double cpu_usage;
	double mem_usage;
	char timestamp[TS_LEN];
	
	struct proc_info *insert; //当前写入需要新增的进程信息
	
	struct rm_pid *remove;	//当前写入需要删除的进程信息
	struct sys_info* next;
};

struct sys_info_head
{

	int num;	//缓冲区里剩下的需要写入log的数量
	struct sys_info* next;
};

struct buffer
{
	struct proc_info* proc_head;	//管理最新写入log的进程信息，双向链表，而且可以用红黑树或者B+树来索引它们，加快效率
	struct proc_info* proc_tail;
	struct sys_info_head sys_head;
	map_t tmp[MAP_NUM];	//当前读入的进程信息的位图,一个bit代表一个pid
	map_t r_pidmap[MAP_NUM]; //上次读入的位图，两个位图可以实现增量修改，知道哪些进程需要保留，哪些进程需要删除，哪些进程是新增的
	struct cpu_info* old_cpu; //保存上次的cpu信息，通过delta来计算cpu占用率
};

//红黑树或者B+树(尚未实现)，如果是c++的话，可以直接使用map，pid作为key，对应的进程信息作为value

//collect system infomation
void* collect_sys_info(void);
static void read_procs(void);
static void read_cpuinfo(struct sys_info* si);
static void read_meminfo(struct sys_info* si);
static void read_proc_info(struct sys_info* si, pid_t pid);
static void read_stat(char *filename, struct proc_info *proc);
static void read_cmdline(char *filename, struct proc_info *proc);
static void read_timestamp(struct sys_info* si);
static void init_buffer(void);
static void read_proc_info(struct sys_info* si, pid_t pid);
static void update_proc_read(struct sys_info* si, struct proc_info* proc);
static void update_sys_read(struct sys_info* si);
static void debug_sys_info(struct sys_info* si);
static void debug_proc_info(struct proc_info* proc);

//write log
void* write_log(void);
static void write_cpuinfo(FILE* file, struct sys_info* si);
static struct sys_info* get_sys_info(void);
static void write_meminfo(FILE* file, struct sys_info* si);
static void write_timestamp(FILE* file, struct sys_info* si);
static void write_procinfo(FILE* file, struct sys_info* si);

#endif