/**********************************************************
*   Author  :   wmx
*   Date    :   2019/
*   comment : 模拟微内核 采用将请求通过IPC发送到专门的服务进程，代码如下：
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

long long end, start;

/*!
 * \brief 使能定时器标志
 *  timer = 0 停止
 *  timer = 1 启动
 */
int timer = 0;


int timer_start = 0;

//--删除的node计数
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


/*!
 * \brief 链表节点
 */
struct node {
    struct node *next;
    void *data;
};

void print_result()
{
    printf("定时器超时 total = %d\n", total);
    exit(0);
}

struct node *head = NULL;
struct node *current = NULL;

/*!
 * \brief 插入node
 *        node->next指向之前的头
 *        头指针指向当前node
 * \param node
 */
void insert(struct node *node)
{
    node->data = NULL;
    node->next = head;
    head = node;
}

/*!
 * \brief 返回删除的node，头指针指向下一位
 * \return
 */
struct node* delete()
{
    struct node *tempLink = head;

    head = head->next;

    return tempLink;
}

/*!
 * \brief 链表头指针为空
 * \return
 */
int empty()
{
    return head == NULL;
}

//--mutex
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

//--spinlock
static pthread_spinlock_t spin;


int add_task()
{
    //--动态内存分配
    struct node *tsk = (struct node*) malloc(sizeof(struct node));

    pthread_spin_lock(&spin);

    //--向链表插入node
    if (timer || curr < count) {
        curr ++;
        insert(tsk);
    }

    pthread_spin_unlock(&spin);

    return curr;
}

/*!
 * \brief 模拟耗时任务
 *        强度可以调整，比如0xff->0xffff，CPU比较猛比较多的机器上做测试，
 *        将其调强些，否则队列开销会淹没模拟任务的开销。
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

    //--添加node完成或者超时,退出
    while (1) {
        ret = add_task();
        if (!timer && ret == count) {
            break;
        }
    }
}


//struct itimerval {
//    struct timeval it_interval; /* 计时器重启动的间歇值 */
//    struct timeval it_value;    /* 计时器安装后首先启动的初始值 */
//};

//struct timeval {
//    long tv_sec;                /* 秒 */
//    long tv_usec;               /* 微妙(1/1000000) */
//};


/*!
 * \brief timer==1 ,运行10秒后结束
 *        timer==0 ,完全清除链表的node后结束
 * \param arg
 * \return
 */
void* server_func(void *arg)
{
    //--等待定时器超时或者完全清除链表的node
    while (timer || total != count) {
        struct node *tsk;

        pthread_spin_lock(&spin);

        //--链表为空，解锁,跳过此次循环
        if (empty()) {
            pthread_spin_unlock(&spin);
            continue;
        }

        if (timer && timer_start == 0) {
            struct itimerval tick = {0};
            timer_start = 1;

            //--宏	信号
            //SIGABRT 	（信号中止）异常终止，例如由...发起 退出 功能。
            //SIGFPE 	（信号浮点异常）错误的算术运算，例如零分频或导致溢出的运算（不一定是浮点运算）。
            //SIGILL 	（信号非法指令）无效的功能图像，例如非法指令。这通常是由于代码中的损坏或尝试执行数据。
            //SIGINT 	（信号中断）交互式注意信号。通常由应用程序用户生成。
            //SIGSEGV 	（信号分段违规）对存储的无效访问：当程序试图在已分配的内存之外读取或写入时。
            //SIGTERM 	（信号终止）发送到程序的终止请求。

            //--定时器超时触发,终止程序
            signal(SIGALRM, print_result);

            //--10秒后启动定时器
            tick.it_value.tv_sec = 10;
            tick.it_value.tv_usec = 0;
            setitimer(ITIMER_REAL, &tick, NULL);
        }

        tsk = delete();

        //--锁内,模拟耗时任务
        do_task();

        pthread_spin_unlock(&spin);

        //--锁外,模拟耗时任务
//        do_task();

        free(tsk);

        //--打印
        if(timer&&total%count==0)
            printf("%d ",total);

        //--删除的node计数
        total++;
    }

    //--记录结束时间,毫秒
    end = gettime();
    printf("耗时（毫秒）:   end - start = %lld   清除链表的节点数total = %d\n", end - start, total);
    exit(0);
}


/*!
 * \brief 输入命令，启动程序
 *        time ./arbitration 100000 100 1 //4个参数,使能定时器
 *        time ./arbitration 100000 100   //3个参数,未使能定时器
 * \param argc
 * \param argv
 * \return
 */
int main(int argc, char **argv)
{

    printf("模拟微内核采用 将请求通过IPC发送到专门的服务进程\n");

    int err, i;
    int threadCounts;
    pthread_t tid, stid;

    //--参数１　链表node数量
    count = atoi(argv[1]);
    //--参数２  线程数量
    threadCounts = atoi(argv[2]);

    printf("链表node数量 count = %d   线程数量 threadCounts = %d\n", count, threadCounts);

    //--正确输入命令，启动程序（4个参数)
    //-- time ./arbitration 100000 100
    //--使能定时器
    if (argc == 4) {
        timer = 1;
        printf("使能定时器,timer==1 ,运行10秒后结束\n");
    }else{
        printf("未使能定时器,timer==0 ,完全清除链表的node后结束\n");
    }

    pthread_spin_init(&spin, PTHREAD_PROCESS_PRIVATE);

    // 创建服务线程,清除链表的node
    err = pthread_create(&stid, NULL, server_func, NULL);
    if (err != 0) {
        exit(1);
    }

    //--开始时间戳
    start = gettime();

    // 创建工作线程,添加链表node
    for (i = 0; i < threadCounts; i++) {
        err = pthread_create(&tid, NULL, func, NULL);
        if (err != 0) {
            exit(1);
        }
    }

    sleep(3600);

    return 0;
}
