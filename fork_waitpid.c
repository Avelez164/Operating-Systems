#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main()
{
    int rc = fork();

    if (rc < 0)
    {
        fprintf(stderr, "Fork failed\n");
        exit(1);
    }
    else if (rc == 0)
    {
        printf("Child process\n");
    }
    else
    {
        int wc = waitpid(rc, NULL, 0);
        printf("Parent waited for child %d, returned: %d\n", rc, wc);
    }
    return 0;
}
