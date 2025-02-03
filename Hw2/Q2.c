#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main()
{
    int rc = fork();
    if (rc < 0)
    {
        perror("Fork failed");
        exit(1);
    }
    else if (rc == 0)
    {
        printf("hello\n");
        fflush(stdout);
        _exit(0);
    }
    else
    {
        printf("goodbye\n");
        exit(0);
    }
}
