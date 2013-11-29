#include <sched.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <atomic>

enum {
   THREAD_COUNT = 503,
   STACK_SIZE = 256,
};

typedef char[64] cache_pad_t;

char stacks[THREAD_COUNT][STACK_SIZE];

struct thread_t {
    int index;
    cache_pad_t pad1;
    std::atomic<int> mailbox;
    cache_pad_t pad2;
    thread_t *next;
    cache_pad_t pad3;
    std::atomic<pid_t> pid;
} threads[THREAD_COUNT];

int max_token = 0;
int num_cores = 1;

void * thread_func(void *param)
{
    thread_t &this_thread = *(thread_t *)param;
    const int index = this_thread.index;
    std::atomic<int> &mailbox  = this_thread.mailbox;
    std::atomic<int> &next_mailbox = this_thread.next->mailbox;

    const pid_t this_pid = gettid();
    this_thread.pid = this_pid;

    // wait for next thread became started
    pid_t next_pid =  0;
    while (!(next_pid = this_thread.next->pid)) {
    }
    
    const int cpu = index * num_cores / THREAD_COUNT;
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    sched_param normal;
    sched_getparam(this_pid, &normal);
    
    sched_param lower;
    lower.sched_priority = normal.sched_priority + 1;
    
    for (;;) {
        const int token = mailbox.load(std::memory_order_relaxed);
        if (!token) {
            sched_yield();
            continue;
        }

        next_mailbox.store(token + 1, std::memory_order_relaxed);
        mailbox.store(0, std::memory_order_relaxed);
        
        // reschedule threads
        sched_setscheduler(next_pid, SCHED_FIFO, &normal);
        sched_setscheduler(this_pid, SCHED_FIFO, &lower);
        
        if (token == max_token) {
            printf("%d\n", index + 1);
            break;
        }

        if (token + THREAD_COUNT > max_token) {
            break;
        }
    }
    return 0;
}

int main(int, char *argv[])
{
    max_token = strtol(argv[1], NULL, 10);
    num_cores = sysconf(_SC_NPROCESSORS_ONLN);

    // init
    for (int i = 0; i < THREAD_COUNT; ++i) {
        threads[i].index = i;
        threads[i].next = &threads[(i + 1) % THREAD_COUNT];
        threads[i].mailbox.store(0);
        threads[i].pid.store(0);
    }
    threads[0].mailbox.store(1);

    // start
    pthread_attr_t stack_attr;
    pthread_attr_init(&stack_attr);
    for (int i = 0; i < THREAD_COUNT; ++i) {
        pthread_attr_setstack(&stack_attr, &stacks[i], STACK_SIZE);
        pthread_create(&threads[i], &stack_attr, &thread_func, (void *)(indexes + i));
    }

    // wait
    pthread_join(threads[(max_token - 1) % THREAD_COUNT], NULL);

    return 0;
}

