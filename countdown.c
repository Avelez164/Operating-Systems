#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define NUM_PHILOSOPHERS 5

int loops = 3;

enum
{
    THINKING,
    HUNGRY,
    EATING
} state[NUM_PHILOSOPHERS];
pthread_cond_t conds[NUM_PHILOSOPHERS];
pthread_mutex_t state_mutex = PTHREAD_MUTEX_INITIALIZER;

int global_countdown;
pthread_mutex_t countdown_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t all_done = PTHREAD_COND_INITIALIZER;

void think(int id)
{
    printf("P#%d THINKING.\n", id);
    usleep(100000);
}

void eat(int id, int left)
{
    printf("P#%d EATING (%d left).\n", id, left);
    usleep(100000);
}

int left(int id) { return (id + NUM_PHILOSOPHERS - 1) % NUM_PHILOSOPHERS; }
int right(int id) { return (id + 1) % NUM_PHILOSOPHERS; }

void test(int id)
{
    if (state[id] == HUNGRY &&
        state[left(id)] != EATING &&
        state[right(id)] != EATING)
    {
        state[id] = EATING;
        pthread_cond_signal(&conds[id]);
    }
}

void pickup_forks(int id)
{
    pthread_mutex_lock(&state_mutex);
    state[id] = HUNGRY;
    test(id);
    while (state[id] != EATING)
        pthread_cond_wait(&conds[id], &state_mutex);
    pthread_mutex_unlock(&state_mutex);
}

void putdown_forks(int id)
{
    pthread_mutex_lock(&state_mutex);
    state[id] = THINKING;
    test(left(id));
    test(right(id));
    pthread_mutex_unlock(&state_mutex);
}

void *philosopher(void *arg)
{
    int id = *(int *)arg;
    free(arg);

    for (int i = loops; i > 0; i--)
    {
        think(id);
        pickup_forks(id);
        eat(id, i);
        putdown_forks(id);
        printf("P#%d finished eating, %d rounds remaining.\n", id, i - 1);
    }

    pthread_mutex_lock(&countdown_mutex);
    global_countdown--;
    if (global_countdown == 0)
        pthread_cond_signal(&all_done);
    pthread_mutex_unlock(&countdown_mutex);

    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc > 1)
    {
        loops = atoi(argv[1]);
        if (loops <= 0)
            loops = 3;
    }

    pthread_t threads[NUM_PHILOSOPHERS];
    global_countdown = NUM_PHILOSOPHERS;

    for (int i = 0; i < NUM_PHILOSOPHERS; i++)
    {
        pthread_cond_init(&conds[i], NULL);
        state[i] = THINKING;
    }

    for (int i = 0; i < NUM_PHILOSOPHERS; i++)
    {
        int *id = malloc(sizeof(int));
        *id = i;
        pthread_create(&threads[i], NULL, philosopher, id);
    }

    pthread_mutex_lock(&countdown_mutex);
    while (global_countdown > 0)
        pthread_cond_wait(&all_done, &countdown_mutex);
    pthread_mutex_unlock(&countdown_mutex);

    for (int i = 0; i < NUM_PHILOSOPHERS; i++)
    {
        pthread_join(threads[i], NULL);
        pthread_cond_destroy(&conds[i]);
    }

    printf("All philosophers have finished eating their rounds!\n");
    return 0;
}
