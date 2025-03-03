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
        printf("Child: Executing /bin/ls using exec...\n");
        fflush(stdout);

        // diff exec() var
        execl("/bin/ls", "ls", "-l", (char *)NULL);
        perror("exec failed");
        exit(1);
    }
    else
    {
        printf("Parent: Waiting for child process to complete...\n");
    }

    return 0;
}
