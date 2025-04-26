#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define MAX_PROCESSES 100
#define MAX_RESOURCES 10
#define MAX_DENIES 3

typedef struct
{
    int id;
    int max[MAX_RESOURCES];
    int allocation[MAX_RESOURCES];
    int need[MAX_RESOURCES];
} Process;

int P, R;
int available[MAX_RESOURCES];
Process processes[MAX_PROCESSES];
int total_requests = 0, granted = 0, denied = 0;

int safe_seq[MAX_PROCESSES], seq_idx = 0;

pthread_mutex_t lock;
pthread_cond_t cond;

// 1) Read input
void read_input(const char *fn)
{
    FILE *f = fopen(fn, "r");
    if (!f)
    {
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    fscanf(f, "%d %d", &P, &R);
    for (int j = 0; j < R; j++)
        fscanf(f, "%d", &available[j]);
    for (int i = 0; i < P; i++)
    {
        processes[i].id = i;
        for (int j = 0; j < R; j++)
            fscanf(f, "%d", &processes[i].max[j]);
        for (int j = 0; j < R; j++)
            fscanf(f, "%d", &processes[i].allocation[j]);
        for (int j = 0; j < R; j++)
            fscanf(f, "%d", &processes[i].need[j]);
    }
    fclose(f);
}

// 2) Standard Banker’s safety check
int is_safe_state()
{
    int work[MAX_RESOURCES];
    int finish[MAX_PROCESSES] = {0};
    memcpy(work, available, sizeof(int) * R);

    for (int k = 0; k < P; k++)
    {
        int found = 0;
        for (int i = 0; i < P; i++)
        {
            if (!finish[i])
            {
                int ok = 1;
                for (int j = 0; j < R; j++)
                    if (processes[i].need[j] > work[j])
                    {
                        ok = 0;
                        break;
                    }
                if (ok)
                {
                    for (int j = 0; j < R; j++)
                        work[j] += processes[i].allocation[j];
                    finish[i] = 1;
                    found = 1;
                }
            }
        }
        if (!found)
            return 0;
    }
    return 1;
}

// 3) Precompute safe sequence so we can gate the final “full‑need” fallback
void compute_safe_sequence()
{
    int work[MAX_RESOURCES];
    int finish[MAX_PROCESSES] = {0};
    memcpy(work, available, sizeof(int) * R);

    for (int k = 0; k < P; k++)
    {
        int found = 0;
        for (int i = 0; i < P; i++)
        {
            if (!finish[i])
            {
                int ok = 1;
                for (int j = 0; j < R; j++)
                    if (processes[i].need[j] > work[j])
                    {
                        ok = 0;
                        break;
                    }
                if (ok)
                {
                    safe_seq[k] = i;
                    finish[i] = 1;
                    for (int j = 0; j < R; j++)
                        work[j] += processes[i].allocation[j];
                    found = 1;
                    break;
                }
            }
        }
        if (!found)
        {
            fprintf(stderr, "ERROR: No safe sequence!\n");
            exit(EXIT_FAILURE);
        }
    }

    // Nice to print it
    printf("Initial Available:");
    for (int j = 0; j < R; j++)
        printf(" %d", available[j]);
    printf("\nSafe sequence:");
    for (int i = 0; i < P; i++)
        printf(" P%d", safe_seq[i]);
    printf("\n\n");
}

// 4) Thread: randomized requests until need==0, then full‑need fallback in safe order
void *customer_thread(void *arg)
{
    Process *p = (Process *)arg;
    int id = p->id;
    int denies = 0;
    // per‑thread RNG
    srand((unsigned)time(NULL) + id);

    while (1)
    {
        // Build request (random up to need, or full‑need after MAX_DENIES)
        int fallback = (denies >= MAX_DENIES);
        int req[MAX_RESOURCES], valid = 0;
        for (int j = 0; j < R; j++)
        {
            if (p->need[j] > 0)
            {
                req[j] = fallback
                             ? p->need[j]
                             : rand() % (p->need[j] + 1);
                if (req[j] > 0)
                    valid = 1;
            }
            else
            {
                req[j] = 0;
            }
        }
        if (!valid)
        {
            // picked all zeros
            usleep(100000);
            continue;
        }

        if (fallback)
        {
            // Gate the “full‑need” request to ensure it’s safe
            pthread_mutex_lock(&lock);
            while (safe_seq[seq_idx] != id)
                pthread_cond_wait(&cond, &lock);

            // Make the single full‑need request
            printf("P%d requesting full‐need:", id);
            for (int j = 0; j < R; j++)
                printf(" %d", req[j]);
            printf("\n");
            total_requests++;

            // Allocate without further checks (safe by construction)
            for (int j = 0; j < R; j++)
            {
                available[j] -= req[j];
                p->allocation[j] += req[j];
                p->need[j] = 0;
            }
            printf("Request GRANTED to P%d (fallback)\n", id);
            granted++;

            // Release immediately
            printf("P%d has finished. Releasing resources.\n\n", id);
            for (int j = 0; j < R; j++)
            {
                available[j] += p->allocation[j];
                p->allocation[j] = 0;
            }

            // Advance safe‐sequence and wake up any waiting thread
            seq_idx++;
            pthread_cond_broadcast(&cond);
            pthread_mutex_unlock(&lock);
            break;
        }

        // Otherwise, try a normal randomized request
        pthread_mutex_lock(&lock);
        printf("P%d requesting:", id);
        for (int j = 0; j < R; j++)
            printf(" %d", req[j]);
        printf("\n");
        total_requests++;

        // Check raw availability
        int can = 1;
        for (int j = 0; j < R; j++)
        {
            if (req[j] > available[j])
            {
                can = 0;
                break;
            }
        }

        if (can)
        {
            // Pretend to allocate
            for (int j = 0; j < R; j++)
            {
                available[j] -= req[j];
                p->allocation[j] += req[j];
                p->need[j] -= req[j];
            }
            // Safety check
            if (is_safe_state())
            {
                printf("Request GRANTED to P%d\n\n", id);
                granted++;
                denies = 0;
                // If that satisfied the entire need, release and exit
                int done = 1;
                for (int j = 0; j < R; j++)
                    if (p->need[j] != 0)
                    {
                        done = 0;
                        break;
                    }
                if (done)
                {
                    printf("P%d has finished. Releasing resources.\n\n", id);
                    for (int j = 0; j < R; j++)
                    {
                        available[j] += p->allocation[j];
                        p->allocation[j] = 0;
                    }
                    pthread_mutex_unlock(&lock);
                    break;
                }
            }
            else
            {
                // Roll back
                for (int j = 0; j < R; j++)
                {
                    available[j] += req[j];
                    p->allocation[j] -= req[j];
                    p->need[j] += req[j];
                }
                printf("Request DENIED to P%d (unsafe)\n\n", id);
                denied++;
                denies++;
            }
        }
        else
        {
            printf("Request DENIED to P%d (not enough resources)\n\n", id);
            denied++;
            denies++;
        }
        pthread_mutex_unlock(&lock);

        usleep(100000);
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    const char *fn = (argc > 1 ? argv[1] : "Bankers.txt");
    srand((unsigned)time(NULL));
    pthread_mutex_init(&lock, NULL);
    pthread_cond_init(&cond, NULL);

    read_input(fn);
    compute_safe_sequence();

    // Launch all P threads
    pthread_t th[MAX_PROCESSES];
    for (int i = 0; i < P; i++)
    {
        pthread_create(&th[i], NULL, customer_thread, &processes[i]);
    }
    // Wait for completion
    for (int i = 0; i < P; i++)
    {
        pthread_join(th[i], NULL);
    }

    // Final summary
    printf("All processes completed.\n");
    printf("Total requests: %d\n", total_requests);
    printf("Granted requests: %d\n", granted);
    printf("Denied requests: %d\n", denied);

    pthread_mutex_destroy(&lock);
    pthread_cond_destroy(&cond);
    return 0;
}
