
//-- 多核多线程自旋锁spinlock 与互斥量mutex性能分析
//-- https://blog.csdn.net/WMX843230304WMX/article/details/100052812

//Name: svm2.c
//Source: http://www.solarisinternals.com/wiki/index.php/DTrace_Topics_Locks
//Compile(spin lock version): gcc -o spin -DUSE_SPINLOCK svm2.c -lpthread
//Compile(mutex version): gcc -o mutex svm2.c -lpthread
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/syscall.h>

//--线程数量
#define        THREAD_NUM     2

pthread_t g_thread[THREAD_NUM];


//-- spinlock 或者 mutex
#ifdef USE_SPINLOCK
pthread_spinlock_t g_spin;
#else
pthread_mutex_t g_mutex;
#endif


__uint64_t g_count;

pid_t gettid()
{
    return syscall(SYS_gettid);
}

void *run_amuck(void *arg)
{
       int i, j;

       //--打印线程ＩＤ
       printf("Thread %lu started.\n", (unsigned long)gettid());

       //--10000次请求锁
       for (i = 0; i < 10000; i++) {
#ifdef USE_SPINLOCK
           pthread_spin_lock(&g_spin);
#else
               pthread_mutex_lock(&g_mutex);
#endif
               //--每获取一次锁，执行 100000次 累加 操作
               //--耗时比较长
               for (j = 0; j < 100000; j++) {

                    //--打印优先完成的线程ＩＤ
                   if (g_count++ == 123456789){
                           printf("Thread %lu wins!\n", (unsigned long)gettid());
                   }
               }
#ifdef USE_SPINLOCK
           pthread_spin_unlock(&g_spin);
#else
               pthread_mutex_unlock(&g_mutex);
#endif
       }

       //--打印线程ＩＤ
       printf("Thread %lu finished!\n", (unsigned long)gettid());

       return (NULL);
}

int main(int argc, char *argv[])
{
       int i, threads = THREAD_NUM;

       //--打印线程数量
       printf("Creating %d threads...\n", threads);

       //-- spinlock 或者 mutex
#ifdef USE_SPINLOCK
       pthread_spin_init(&g_spin, 0);
#else
       pthread_mutex_init(&g_mutex, NULL);
#endif

       for (i = 0; i < threads; i++)
               pthread_create(&g_thread[i], NULL, run_amuck, (void *) i);


        /*!
        * @brief pthread_join
        *   主线程会一直等待直到等待的线程结束自己才结束
        *   对线程的资源进行回收
        */
       for (i = 0; i < threads; i++)
               pthread_join(g_thread[i], NULL);

       printf("Done.\n");

       return (0);
}

