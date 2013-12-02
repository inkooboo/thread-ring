#include <sched.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <atomic>

enum {
   THREAD_COUNT = 503,
   STACK_SIZE = 256
};

typedef char cache_pad_t[64];

char stacks[THREAD_COUNT][STACK_SIZE];

struct thread_t {
    pthread_t pthread;
    cache_pad_t pad0;
    int index;
    cache_pad_t pad1;
    thread_t *next;
    cache_pad_t pad3;
    std::atomic<int> mailbox;
    cache_pad_t pad2;
} threads[THREAD_COUNT];

int max_token = 0;
int num_cores = 1;

void * thread_func(void *param)
{
    thread_t &this_thread = *(thread_t *)param;
    const int index = this_thread.index;
    std::atomic<int> &mailbox  = this_thread.mailbox;
    std::atomic<int> &next_mailbox = this_thread.next->mailbox;

    const int cpu = index * num_cores / THREAD_COUNT;
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    for (;;) {
        const int token = mailbox.load(std::memory_order_relaxed);
        if (!token) {
            sched_yield();
            continue;
        }

        next_mailbox.store(token + 1, std::memory_order_relaxed);
        sched_yield();
        
        mailbox.store(0, std::memory_order_relaxed);

        
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
    }
    threads[0].mailbox.store(1);

    // start
    pthread_attr_t stack_attr;
    pthread_attr_init(&stack_attr);
    for (int i = 0; i < THREAD_COUNT; ++i) {
        pthread_attr_setstack(&stack_attr, &stacks[i], STACK_SIZE);
        pthread_create(&(threads[i].pthread), &stack_attr, &thread_func, (void *)(threads + i));
    }

    // wait
    pthread_join(threads[(max_token - 1) % THREAD_COUNT].pthread, NULL);

    return 0;
}

