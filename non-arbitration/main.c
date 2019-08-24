/**********************************************************
*   Author  :   wmx
*   Date    :   2019/
*   comment :   模拟宏内核中访问共享资源时的自旋锁并发争抢模式：
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
    printf("%d\n", curr);
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
        signal(SIGALRM, print_result);
        tick.it_value.tv_sec = 10;
        tick.it_value.tv_usec = 0;
        setitimer(ITIMER_REAL, &tick, NULL);
    }
    if (!timer && curr == count) {
        end = gettime();
        printf("%lld\n", end - start);
        exit(0);
    }
    curr ++;
    for (i = 0; i < 0xff; i++) { // 做一些稍微耗时的计算，模拟类似socket操作。强度可以调整，比如0xff->0xffff，CPU比较猛比较多的机器上做测试，将其调强些，否则队列开销会淹没模拟任务的开销。
        k += i/j;
    }
    pthread_spin_unlock(&spin);
    free(tsk);
}

void* func(void *arg)
{
    while (1) {
        do_task();
    }
}

int main(int argc, char **argv)
{
    int err, i;
    int tcnt;
    pthread_t tid;

    count = atoi(argv[1]);
    tcnt = atoi(argv[2]);
    if (argc == 4) {
        timer = 1;
    }

    pthread_spin_init(&spin, PTHREAD_PROCESS_PRIVATE);
    start = gettime();
    // 创建工作线程
    for (i = 0; i < tcnt; i++) {
        err = pthread_create(&tid, NULL, func, NULL);
        if (err != 0) {
            exit(1);
        }
    }

    sleep(3600);

    return 0;
}
