
//-- 多核多线程自旋锁spinlock 与互斥量mutex性能分析
//-- https://blog.csdn.net/WMX843230304WMX/article/details/100052812

// Name: spinlockvsmutex1.cc
// Source: http://www.alexonlinux.com/pthread-mutex-vs-pthread-spinlock
// Compiler(spin lock version): g++ -o spin_version -DUSE_SPINLOCK spinlockvsmutex1.cc -lpthread
// Compiler(mutex version): g++ -o mutex_version spinlockvsmutex1.cc -lpthread

#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>
#include <sys/time.h>
#include <list>
#include <pthread.h>

#define LOOPS 50000000

using namespace std;

list<int> the_list;


//-- spinlock 或者 mutex
#ifdef USE_SPINLOCK
pthread_spinlock_t spinlock;
#else
pthread_mutex_t mutex;
#endif


//Get the thread id
pid_t gettid() { return syscall( __NR_gettid ); }

void *consumer(void *ptr)
{
    int i;

    //--打印线程ＩＤ
    printf("Consumer Thread ID %lu\n", (unsigned long)gettid());

    while (1)
    {
#ifdef USE_SPINLOCK
        pthread_spin_lock(&spinlock);
#else
        pthread_mutex_lock(&mutex);
#endif

        //--列表为空，结束
        if (the_list.empty())
        {
#ifdef USE_SPINLOCK
            pthread_spin_unlock(&spinlock);
#else
            pthread_mutex_unlock(&mutex);
#endif
            break;
        }

        //---取出列表第一个值
        //--每获取一次锁，执行 1次 赋值和 pop_front 操作
        //--耗时非常短
        i = the_list.front();
        the_list.pop_front();

#ifdef USE_SPINLOCK
        pthread_spin_unlock(&spinlock);
#else
        pthread_mutex_unlock(&mutex);
#endif
    }

    return NULL;
}

int main()
{
    int i;
    pthread_t thr1, thr2;
    struct timeval tv1, tv2;

#ifdef USE_SPINLOCK
    pthread_spin_init(&spinlock, 0);
#else
    pthread_mutex_init(&mutex, NULL);
#endif

    // Creating the list content...
    //--生产者,创建列表内容
    for (i = 0; i < LOOPS; i++)
        the_list.push_back(i);

    // Measuring time before starting the threads...
    //--启动线程前时间
    gettimeofday(&tv1, NULL);

    //--创建两个消费者线程
    pthread_create(&thr1, NULL, consumer, NULL);
    pthread_create(&thr2, NULL, consumer, NULL);

    //--主线程等待两个消费者线程结束
    pthread_join(thr1, NULL);
    pthread_join(thr2, NULL);

    // Measuring time after threads finished...
    //--线程结束时间
    gettimeofday(&tv2, NULL);

    if (tv1.tv_usec > tv2.tv_usec)
    {
        tv2.tv_sec--;
        tv2.tv_usec += 1000000;
    }

    //--打印耗时
    printf("Result - %ld.%ld\n", tv2.tv_sec - tv1.tv_sec,
        tv2.tv_usec - tv1.tv_usec);

#ifdef USE_SPINLOCK
    pthread_spin_destroy(&spinlock);
#else
    pthread_mutex_destroy(&mutex);
#endif

    return 0;
}
