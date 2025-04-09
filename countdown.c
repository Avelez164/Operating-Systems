#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

#define NUM_PHILOSOPHERS 5

// Default rounds of eating
int loops = 5;

// Forks rep as semaphores
sem_t forks[NUM_PHILOSOPHERS];

// countdown: number of philosophers still running
int global_countdown;
pthread_mutex_t countdown_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t all_done = PTHREAD_COND_INITIALIZER;

void think(int id)
{
    printf("P#%d THINKING.\n", id);
    usleep(100000);
}

void eat(int id, int rounds_left)
{
    // rounds_left shows how many rounds remain
    printf("P#%d EATING (%d left).\n", id, rounds_left);
    usleep(100000);
}

void *philosopher(void *arg)
{
    int id = *(int *)arg;
    free(arg);

    // philosophers start with the full number of rounds (loops)
    for (int i = loops; i > 0; i--)
    {
        think(id);
        // ordering: even IDs pick up left fork first, odd IDs pick up right fork first
        if (id % 2 == 0)
        {
            sem_wait(&forks[id]);
            sem_wait(&forks[(id + 1) % NUM_PHILOSOPHERS]);
        }
        else
        {
            sem_wait(&forks[(id + 1) % NUM_PHILOSOPHERS]);
            sem_wait(&forks[id]);
        }
        eat(id, i);
        sem_post(&forks[id]);
        sem_post(&forks[(id + 1) % NUM_PHILOSOPHERS]);
        printf("P#%d finished eating, %d rounds remaining.\n", id, i - 1);
    }

    // philosopher -> done, update the global countdown
    pthread_mutex_lock(&countdown_mutex);
    global_countdown--;
    // signal -> main thread if last philosopher finished
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
            loops = 5;
    }

    // set -> global countdown to -> # of philosophers
    global_countdown = NUM_PHILOSOPHERS;

    pthread_t threads[NUM_PHILOSOPHERS];

    // initialize fork semaphores
    for (int i = 0; i < NUM_PHILOSOPHERS; i++)
    {
        sem_init(&forks[i], 0, 1);
    }

    // Create philosopher threads
    for (int i = 0; i < NUM_PHILOSOPHERS; i++)
    {
        int *id = malloc(sizeof(int));
        *id = i;
        pthread_create(&threads[i], NULL, philosopher, id);
    }

    pthread_mutex_lock(&countdown_mutex);
    while (global_countdown > 0)
    {
        pthread_cond_wait(&all_done, &countdown_mutex);
    }
    pthread_mutex_unlock(&countdown_mutex);

    for (int i = 0; i < NUM_PHILOSOPHERS; i++)
    {
        pthread_join(threads[i], NULL);
        sem_destroy(&forks[i]);
    }

    printf("All philosophers have finished eating their rounds!\n");
    return 0;
}
