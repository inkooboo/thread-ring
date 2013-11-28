#define _GNU_SOURCE

#include <sched.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

enum
{
   THREAD_COUNT = 503,
   STACK_SIZE = 256
};

pthread_t threads[THREAD_COUNT];
char stacks[THREAD_COUNT][STACK_SIZE];
int mailboxes[THREAD_COUNT];

int max_token = 0;
int num_cores = 1;

void * thread_func(void *param)
{
    const int index = *(int *)param;
    int *mailbox  = mailboxes + index;
    int *next_mailbox = mailboxes + ((index + 1) % THREAD_COUNT);

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    for (;;) {
        const int token = *mailbox;
        if (!token) {
            usleep(1);
            continue;
        }

        *next_mailbox = token + 1;
        *mailbox = 0;

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

int main(int argc, char *argv[])
{
    int i = 0;

    (void)argc;

    max_token = strtol(argv[1], NULL, 10);

    num_cores = sysconf(_SC_NPROCESSORS_ONLN);

    for (i = 0; i < THREAD_COUNT; ++i) {
        mailboxes[i] = 0;
    }
    mailboxes[0] = 1;

    pthread_attr_t stack_attr;
    pthread_attr_init(&stack_attr);

    for (i = 0; i < THREAD_COUNT; ++i) {
        pthread_attr_setstack(&stack_attr, &stacks[i], STACK_SIZE);
        pthread_create(&threads[i], &stack_attr, &thread_func, (void *)i);
    }

    pthread_join(threads[max_token % THREAD_COUNT], NULL);

    return 0;
}

