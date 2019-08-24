/**********************************************************
*   Author  :   wmx
*   Date    :   2019/
*   comment : 模拟微内核采用 将请求通过IPC发送到专门的服务进程，代码如下：
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

long long end, start;
int timer = 0;
int timer_start = 0;
static int total = 0;


/*!
 * \brief 返回 1970-01-01至今 时间戳 毫秒
 * \return
 */
long long gettime()
{
    struct timeb t;
    ftime(&t);
    return 1000 * t.time + t.millitm; //毫秒
}

struct node {
    struct node *next;
    void *data;
};

void print_result()
{
    printf("%d\n", total);
    exit(0);
}

struct node *head = NULL;
struct node *current = NULL;

void insert(struct node *node)
{
    node->data = NULL;
    node->next = head;
    head = node;
}

struct node* delete()
{
    struct node *tempLink = head;

    head = head->next;

    return tempLink;
}

int empty()
{
    return head == NULL;
}

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_spinlock_t spin;

int add_task()
{
    struct node *tsk = (struct node*) malloc(sizeof(struct node));

    pthread_spin_lock(&spin);
    if (timer || curr < count) {
        curr ++;
        insert(tsk);
    }
    pthread_spin_unlock(&spin);

    return curr;
}

/*!
 * \brief 强度可以调整，比如0xff->0xffff，CPU比较猛比较多的机器上做测试，将其调强些，否则队列开销会淹没模拟任务的开销。
 */
void do_task()
{
    int i = 0, j = 2, k = 0;
    for (i = 0; i < 0xff; i++) {
        k += i/j;
    }
}

void* func(void *arg)
{
    int ret;

    while (1) {
        ret = add_task();
        if (!timer && ret == count) {
            break;
        }
    }
}


void* server_func(void *arg)
{
    while (timer || total != count) {
        struct node *tsk;
        pthread_spin_lock(&spin);
        if (empty()) {
            pthread_spin_unlock(&spin);
            continue;
        }
        if (timer && timer_start == 0) {
            struct itimerval tick = {0};
            timer_start = 1;
            signal(SIGALRM, print_result);
            tick.it_value.tv_sec = 10;
            tick.it_value.tv_usec = 0;
            setitimer(ITIMER_REAL, &tick, NULL);
        }
        tsk = delete();
        pthread_spin_unlock(&spin);
        do_task();
        free(tsk);
        total++;
    }
    end = gettime();
    printf("time:   end - start = %lld   total = %d\n", end - start, total);
    exit(0);
}

int main(int argc, char **argv)
{
    int err, i;
    int threadCounts;
    pthread_t tid, stid;

    count = atoi(argv[1]);//参数１　字符串转换成整型数
    threadCounts = atoi(argv[2]); //参数２  字符串转换成整型数

    if (argc == 4) {
        timer = 1;
    }

    pthread_spin_init(&spin, PTHREAD_PROCESS_PRIVATE);

    // 创建服务线程
    err = pthread_create(&stid, NULL, server_func, NULL);
    if (err != 0) {
        exit(1);
    }

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
