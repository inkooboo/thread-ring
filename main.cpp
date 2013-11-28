#include <sched.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <atomic>

enum
{
   THREAD_COUNT = 503,
   STACK_SIZE = 256
};

pthread_t threads[THREAD_COUNT];
char stacks[THREAD_COUNT][STACK_SIZE];
int indexes[THREAD_COUNT];
std::atomic<int> mailboxes[THREAD_COUNT];

int max_token = 0;
int num_cores = 1;

void * thread_func(void *param)
{
    const int index = *(int *)param;
    std::atomic<int> *mailbox  = mailboxes + index;
    std::atomic<int> *next_mailbox = mailboxes + ((index + 1) % THREAD_COUNT);

    const int cpu = index * num_cores / THREAD_COUNT;
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    for (;;) {
        const int token = mailbox->load(std::memory_order_relaxed);
        if (!token) {
            pthread_yield();
            continue;
        }

        next_mailbox->store(token + 1, std::memory_order_relaxed);
        mailbox->store(0, std::memory_order_relaxed);

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

    for (int i = 0; i < THREAD_COUNT; ++i) {
        mailboxes[i].store(0);
    }
    mailboxes[0].store(1);

    pthread_attr_t stack_attr;
    pthread_attr_init(&stack_attr);

    for (int i = 0; i < THREAD_COUNT; ++i) {
	indexes[i] = i;
        pthread_attr_setstack(&stack_attr, &stacks[i], STACK_SIZE);
        pthread_create(&threads[i], &stack_attr, &thread_func, (void *)(indexes + i));
    }

    pthread_join(threads[(max_token - 1) % THREAD_COUNT], NULL);

    return 0;
}

