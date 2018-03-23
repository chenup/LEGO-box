#include "sys_monitor.h"
#include "threadpool.h"
#include <stdio.h>
#include <stdlib.h>  
#include <unistd.h>  
#include <dirent.h>
#include <ctype.h> 
#include <sys/time.h> 
#include <string.h>

struct buffer manager;
struct cpu_info cur_cpu;
int main(int argc, char const *argv[])
{
	/*
	struct threadpool *tp = threadpool_init(INIT_NUM);
	if(tp == NULL)
	{
		printf("fail to create threadpool!\n");
		return -1;
	}
	*/
	//add_task(tp, collect_sys_info);
    //add_task(tp, write_log);
    init_buffer();
    //这个回调函数用于收集系统信息
	collect_sys_info();
	//这个回调函数用于写入log
	write_log();
	//threadpool_destroy();
	return 0;
}


void* collect_sys_info(void)
{
	int i = 20;
	while(i--)
	{
		printf("#%d\n", i);
		read_procs();  
		usleep(DELAY);
	}
}

void read_procs(void)
{
	DIR *proc_dir;  
    struct dirent *pid_dir;
    struct sys_info* si;
    pid_t pid;
    memset(manager.tmp, 0, sizeof(manager.tmp));
    proc_dir = opendir("/proc");  
    if(!proc_dir) 
    {
    	panic("fail to open /proc.\n");
    } 

    si = malloc(sizeof(struct sys_info));

  	read_cpuinfo(si);  
   	read_meminfo(si);

	while(pid_dir = readdir(proc_dir)) 
	{  
        if (!isdigit(pid_dir->d_name[0]))
        {
           continue; 
        }       
		pid = atoi(pid_dir->d_name);
		read_proc_info(si, pid);
    }
    closedir(proc_dir);
    read_timestamp(si);
	debug_sys_info(si);
	//更新系统信息
	update_sys_read(si);
}

void read_cpuinfo(struct sys_info* si)
{
	struct cpu_info* cur_cpu = malloc(sizeof(struct cpu_info));
	double cpu_usage = 0;
	u64 total_delta_time;
	u64 idle_delta_time;
	FILE* file = fopen("/proc/stat", "r");  
    if(!file) 
	{
		panic("fail to open /proc/stat.\n");
	}  
    fscanf(file, "cpu  %llu %llu %llu %llu %llu %llu %llu %llu %llu", &cur_cpu->utime, &cur_cpu->ntime, &cur_cpu->systime,  
            &cur_cpu->idltime, &cur_cpu->iotime, &cur_cpu->irqtime, &cur_cpu->sirqtime, &cur_cpu->sttime, &cur_cpu->gtime);  
   	fclose(file);

   	if(manager.old_cpu != NULL)
   	{
   		struct cpu_info* old_cpu = manager.old_cpu;
   		idle_delta_time = cur_cpu->idltime - old_cpu->idltime;
   		total_delta_time = (cur_cpu->utime + cur_cpu->ntime + cur_cpu->systime + cur_cpu->idltime + cur_cpu->iotime 
   							+ cur_cpu->irqtime + cur_cpu->sirqtime + cur_cpu->sttime + cur_cpu->gtime)  
                     		- (old_cpu->utime + old_cpu->ntime + old_cpu->systime + old_cpu->idltime + old_cpu->iotime 
   							+ old_cpu->irqtime + old_cpu->sirqtime + old_cpu->sttime + old_cpu->gtime); 
   		cpu_usage = (total_delta_time - idle_delta_time) / (total_delta_time * 1.0) * 100;
   		free(old_cpu);
   	}
   	manager.old_cpu = cur_cpu;

   	si->cpu_usage = cpu_usage;
}

void read_meminfo(struct sys_info* si)
{
	struct mem_info cur_mem;
	double mem_usage;
	FILE* file = fopen("/proc/meminfo", "r");
    if(!file) 
    {
    	panic("fail to open /proc/meminfo.\n");  
    }
    fscanf(file, "MemTotal: %llu kB MemFree: %llu kB MemAvailable: %*llu kB Buffers: %llu kB Cached: %llu kB", 
    		&cur_mem.memtotal, &cur_mem.memfree, &cur_mem.buffers, &cur_mem.cached);
    fclose(file);
    mem_usage = (cur_mem.memtotal - cur_mem.memfree - cur_mem.buffers - cur_mem.cached) / (1.0 * cur_mem.memtotal) * 100;
    si->mem_usage = mem_usage;
}

void read_proc_info(struct sys_info* si, pid_t pid)
{
	char filename[FN_LEN];
	struct proc_info* cur_proc;
	cur_proc = malloc(sizeof(struct proc_info));
	cur_proc->pid = pid;  
	sprintf(filename, "/proc/%d/stat", pid);  
	read_stat(filename, cur_proc);  
	sprintf(filename, "/proc/%d/cmdline", pid);  
	read_cmdline(filename, cur_proc);
	//debug_proc_info(cur_proc);
	//更新每个进程信息
	update_proc_read(si, cur_proc);
}

void update_proc_read(struct sys_info* si, struct proc_info* proc)
{
	pid_t pid = proc->pid;
	int i = pid / MAP_LEN;
	int j = pid % MAP_LEN;
	manager.tmp[i] |= 1 << j;
	//查看当前进程相比之前的读入是否是新增进程，如果是就加入insert链表
	if(!(manager.r_pidmap[i] & (1 << j)))
	{
		struct proc_info* p = si->insert;
		si->insert = proc;
		proc->next = p;
	}
}

void update_sys_read(struct sys_info* si)
{
	int m;
	int r;
	int shift;
	struct rm_pid* rp;
	struct sys_info* tmp_si;
	//这次的读入的pid和上次读入的pid进行比较，然后记录差别，这样下次写入log时直接更新不同的信息
	//可以记录每个位图的最大pid和最小pid，提高比较的效率
	for(int i = 0; i < MAP_NUM; i++)
	{
		m = manager.tmp[i];
		r = manager.r_pidmap[i];
		for(int j = 0; j < MAP_LEN; j++)
		{
			shift = 1 << j;
			if((m & shift) & !(r & shift))
			{
				manager.r_pidmap[i] |= shift;
			}
			else if(!(m & shift) & (r & shift))
			{
				struct rm_pid* p = malloc(sizeof(struct rm_pid));
				p->pid = i * MAP_LEN + j;
				rp = si->remove;
				si->remove = p;
				p->next = rp;
				manager.r_pidmap[i] &= ~shift;
			}
		}
	}
	manager.sys_head.num++;
	tmp_si = manager.sys_head.next;
	manager.sys_head.next = si;
	si->next = tmp_si;
}

void read_stat(char *filename, struct proc_info *proc) 
{    
   	char buf[BUF_LEN];
    char *open_paren;
    char *close_paren;
    FILE* file;
	file = fopen(filename, "r");  
    if(!file) 
    {
    	panic("fail to open %s.\n", filename);  
    }
   	fgets(buf, BUF_LEN, file);  
    fclose(file);
    open_paren = strchr(buf, '(');  
    close_paren = strrchr(buf, ')');    
  
   	*open_paren = '\0';
   	*close_paren = '\0';  
    strncpy(proc->name, open_paren + 1, NAME_LEN);  
    proc->name[NAME_LEN - 1] = '\0';  
    
    sscanf(close_paren + 1, " %*c %d ", &proc->ppid); 
} 

void read_cmdline(char *filename, struct proc_info *proc) 
{  
    FILE *file;  
    char buf[BUF_LEN]; 
    file = fopen(filename, "r");  
  	if(!file) 
    {
    	panic("fail to open %s.\n", filename);  
    }
   	fgets(buf, BUF_LEN, file);  
    fclose(file); 
     
    if (strlen(buf) > 0) 
    {  
        strncpy(proc->cmd, buf, CMD_LEN);  
        proc->cmd[CMD_LEN - 1] = '\0';  
    } 
    else
    {
        proc->cmd[0] = '\0';
    }        
}  

void read_timestamp(struct sys_info* si)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
    sprintf(si->timestamp, "%ld.%ld", tv.tv_sec, tv.tv_usec / 1000); 
}

void debug_sys_info(struct sys_info* si)
{
	printf("\ncpu_usage: %.2f%%\n", si->cpu_usage);
	printf("\nmem_usage: %.2f%%\n", si->mem_usage);
	printf("\ntimestamp: %s\n", si->timestamp);
}

void debug_proc_info(struct proc_info* proc)
{
	printf("[\n");
	printf("\npid: %lu\n", proc->pid);
	printf("\nppid: %lu\n", proc->ppid);
	printf("\nname: %s\n", proc->name);
	printf("\ncmd: %s\n", proc->cmd);
	printf("]\n");
}

void init_buffer(void)
{
	manager.proc_head = NULL;
	manager.proc_tail = NULL;
	manager.old_cpu = NULL;
	manager.sys_head.num = 0;
	manager.sys_head.next = NULL;
}

void* write_log(void)
{
	struct sys_info *cur_si;
	FILE* file;
	file = fopen("log.out", "w");
	if(!file) 
    {
    	panic("fail to open log.txt!\n");  
    }
    while(1)
    {
    	cur_si = get_sys_info();
		if(cur_si == NULL)
		{
			usleep(DELAY);
			continue;
		}
		//目标是只修改文件中不同的地方，减少文件的写入，时间原因，未实现
		write_cpuinfo(file, cur_si);
		write_meminfo(file, cur_si);
		write_timestamp(file, cur_si);
		write_procinfo(file, cur_si);
		fflush(file);
    }
	fclose(file);
}

void write_cpuinfo(FILE* file, struct sys_info* si)
{
	char cpu_usage[32];
	sprintf(cpu_usage, "cpu_usage: %.2f%%\n", si->cpu_usage); 
    fwrite(cpu_usage ,sizeof(char), strlen(cpu_usage), file);
}

void write_meminfo(FILE* file, struct sys_info* si)
{
	char mem_usage[32];
	sprintf(mem_usage, "mem_usage: %.2f%%\n", si->mem_usage); 
    fwrite(mem_usage ,sizeof(char), strlen(mem_usage), file);
}

void write_timestamp(FILE* file, struct sys_info* si)
{
	char timestamp[TS_LEN];
	sprintf(timestamp, "timestamp: %s\n", si->timestamp); 
    fwrite(timestamp ,sizeof(char), strlen(timestamp), file);
}

void write_procinfo(FILE* file, struct sys_info* si)
{

}

struct sys_info* get_sys_info(void)
{
	struct sys_info *si;
	if(manager.sys_head.num == 0 || manager.sys_head.next == NULL)
	{
		return NULL;
	}
	si = manager.sys_head.next;
	manager.sys_head.next = si->next;
	return si;
}