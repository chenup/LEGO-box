环境：系统环境：ubuntu 16.04 64bit
开发语言： C
运行命令：
gcc sys_monitor.c sys_monitor.h threadpool.h threadpool.c -o sys_monitor -lpthread

实现语言是用C实现的，目前没有实现从json的输入和输出
思路：
1、通过读取/proc/stat、/proc/meminfo、/proc/pid/stats 、 /proc/pid/cmdline文件以及系统提供gettimeofday()函数 来获取json要求的sys_info。
2、通过增量修改的方式来减少文件的写入，每次写入只修改与上次不同的地方，这就要求提供数据结构来保存此次写入需要新增的信息和需要删除的信息，这些可以用链表来实现，而且用位图保存前后两次读取的pid可以更快的比较出差异。
3、通过缓冲区链表来保存每次已经读的但尚未写入log的sys_info,每次写从缓冲区的首部开始，读是插入到缓冲区的尾部，通过双向链表来记录所有的proc_info,并且用红黑树或者b+树以pid作为索引，更快的访问到每个具体的proc_info。
4、线程间的共享信息通过全局变量来实现。
5、当写的速度快与读时，写需要wait一段时间等有新的读进来才可以继续写，但是可以实现一个通知机制，这样读一完成，写就可以继续进行。
6、 写log时，通过文件指针来直接跳转到差异的部分修改，上次写入不需要变动的部分尽量不去重新写，如果log文件内容中间有空余行的话，从文件末尾开始往前移动填充。
7、当系统高负载的情况下，判断各个cpu的占用率，然后选择占用率最小的cpu，将线程迁移过去。
8、线程间的同步和锁还在考虑。