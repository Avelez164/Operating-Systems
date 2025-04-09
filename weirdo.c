#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

#define NUM_PHILOSOPHERS 5

int loops = 3; // default loops

sem_t forks[NUM_PHILOSOPHERS];

void think(int id)
{
    printf("P#%d THINKING.\n", id);
    usleep(100000);
}

void eat(int id, int count)
{
    printf("P#%d EATING (%d/%d).\n", id, count + 1, loops);
    usleep(100000);
}

void *philosopher(void *arg)
{
    int id = *(int *)arg;
    int i;
    for (i = 0; i < loops; i++)
    {
        think(id);
        // "Weirdo" fork pickup: even IDs pick left first, odd IDs pick right first.
        if (id % 2 == 0)
        {
            sem_wait(&forks[id]);                          // picks up left fork
            sem_wait(&forks[(id + 1) % NUM_PHILOSOPHERS]); // picks up right fork
        }
        else
        {
            sem_wait(&forks[(id + 1) % NUM_PHILOSOPHERS]); // picks up right fork
            sem_wait(&forks[id]);                          // picks up left fork
        }
        eat(id, i);
        sem_post(&forks[id]);
        sem_post(&forks[(id + 1) % NUM_PHILOSOPHERS]);
        printf("P#%d finished eating and is thinking again.\n", id);
    }
    free(arg);
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
    int i;
    for (i = 0; i < NUM_PHILOSOPHERS; i++)
    {
        sem_init(&forks[i], 0, 1);
    }
    for (i = 0; i < NUM_PHILOSOPHERS; i++)
    {
        int *id = malloc(sizeof(int));
        *id = i;
        pthread_create(&threads[i], NULL, philosopher, id);
    }
    for (i = 0; i < NUM_PHILOSOPHERS; i++)
    {
        pthread_join(threads[i], NULL);
    }
    for (i = 0; i < NUM_PHILOSOPHERS; i++)
    {
        sem_destroy(&forks[i]);
    }
    printf("All philosophers have finished eating their rounds!\n");
    return 0;
}
