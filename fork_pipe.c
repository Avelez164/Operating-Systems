#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main()
{
    int fd[2];
    pipe(fd);

    int rc1 = fork();

    if (rc1 == 0)
    {
        close(fd[0]);
        write(fd[1], "Hello from child\n", 17);
        close(fd[1]);
    }
    else
    {
        int rc2 = fork();
    }
    return 0;
}
