#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

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
        close(STDOUT_FILENO);
        printf("This will not be printed\n");
    }
    else
    {
        printf("Parent process\n");
    }
    return 0;
}
