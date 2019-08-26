/**********************************************************
*   Author  :   wmx
*   Date    :   2019/
*   comment :   模拟宏内核 访问共享资源时的自旋锁并发争抢模式：
*
*           Linux内核争抢式并发在SMP多核扩展上的不足
*           https://blog.csdn.net/dog250/article/details/99825071
**********************************************************/


#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/timeb.h>

static int count = 0;
static int curr = 0;

static pthread_spinlock_t spin;

long long end, start;
int timer_start = 0;
int timer = 0;

long long gettime()
{
    struct timeb t;
    ftime(&t);
    return 1000 * t.time + t.millitm;
}

void print_result()
{
    printf("定时器超时 curr = %d\n", curr);
    exit(0);
}

struct node {
    struct node *next;
    void *data;
};

void do_task()
{
    int i = 0, j = 2, k = 0;

    // 为了更加公平的对比，既然模拟微内核的代码使用了内存分配，这里也fake一个。
    struct node *tsk = (struct node*) malloc(sizeof(struct node));

    pthread_spin_lock(&spin); // 锁定整个访问计算区间

    if (timer && timer_start == 0) {
        struct itimerval tick = {0};
        timer_start = 1;

        //--定时器超时触发,终止程序
        signal(SIGALRM, print_result);

        //--10秒后启动定时器
        tick.it_value.tv_sec = 10;
        tick.it_value.tv_usec = 0;
        setitimer(ITIMER_REAL, &tick, NULL);
    }
    if (!timer && curr == count) {
        end = gettime();
        printf("耗时（毫秒）: end - start = %lld \n", end - start);
        exit(0);
    }
    curr ++;

    //--锁内,模拟耗时任务
    for (i = 0; i < 0xff; i++) {
        k += i/j;
    }

    pthread_spin_unlock(&spin);



    //--锁外,模拟耗时任务
//    for (i = 0; i < 0xff; i++) {
//        k += i/j;
//    }


    free(tsk);
}

void* func(void *arg)
{
    while (1) {
        do_task();
    }
}


/*!
 * \brief main
 * \param argc
 * \param argv
 * \return
 */
int main(int argc, char **argv)
{
    printf("模拟宏内核 访问共享资源时的自旋锁并发争抢模式\n");

    int err, i;
    int threadCounts;
    pthread_t tid;

    //--参数１　链表node数量
    count = atoi(argv[1]);
    //--参数２  线程数量
    threadCounts = atoi(argv[2]);

    printf("链表node数量 count = %d   线程数量 threadCounts = %d\n", count, threadCounts);

    if (argc == 4) {
        timer = 1;
    }

    pthread_spin_init(&spin, PTHREAD_PROCESS_PRIVATE);

    //--开始时间戳
    start = gettime();

    // 创建工作线程
    for (i = 0; i < threadCounts; i++) {
        err = pthread_create(&tid, NULL, func, NULL);
        if (err != 0) {
            exit(1);
        }
    }

    sleep(3600);

    return 0;
}
