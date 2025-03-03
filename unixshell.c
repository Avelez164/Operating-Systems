#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

#define ARGUMENTS_SIZE 64
#define HISTORY_SIZE BUFSIZ

void handle_io_redirection(bool io_flag, char io_type, char *io_file);

void parse(char *line, char **argv, bool *echo_flag, bool *io_flag, bool *pipe_flag, char *io_type, char **io_file, char **pipe_programs, int *pipe_count, bool *io_print)
{
    *echo_flag = false;
    *io_flag = false;
    *pipe_flag = false;
    *io_file = NULL;
    *pipe_count = 0;
    *io_print = false;
    char *last_arg = NULL;

    while (*line != '\0')
    {
        while (isspace(*line))
        {
            *line++ = '\0';
        }
        if (*line == '\0')
        {
            break;
        }

        if (*line == '<' || *line == '>')
        {
            *io_flag = true;
            *io_type = *line;
            *line = '\0';
            line++;

            while (*line != '\0' && isspace(*line))
            {
                line++;
            }

            if (*line == '\0')
            {
                fprintf(stderr, "Error: Missing file for I/O redirection.\n");
                return;
            }

            *io_file = line;
            while (*line != '\0' && !isspace(*line))
            {
                line++;
            }
            continue;
        }
        else if (*line == '|')
        {
            *pipe_flag = true;
            *line = '\0';
            line++;

            while (*line != '\0' && isspace(*line))
            {
                line++;
            }

            if (*line != '\0')
            {
                pipe_programs[*pipe_count] = line;
                (*pipe_count)++;
            }
            continue;
        }

        *argv++ = line;
        last_arg = line;

        while (*line != '\0' && !isspace(*line))
        {
            line++;
        }
    }
    *argv = NULL;

    if (last_arg)
    {
        if (strcmp(last_arg, "ECHO") == 0)
        {
            *echo_flag = true;
            *(argv - 1) = NULL;
        }
        else if (strcmp(last_arg, "IO") == 0)
        {
            *io_print = true;
            *(argv - 1) = NULL;
        }
        else if (strcmp(last_arg, "PIPE") == 0)
        {
            *(argv - 1) = NULL;
        }
    }
}

void execute_pipeline(char **argv1, char **argv2)
{
    int pipefd[2];
    if (pipe(pipefd) == -1)
    {
        perror("pipe failed");
        exit(1);
    }

    pid_t pid1 = fork();
    if (pid1 == 0)
    {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        execvp(argv1[0], argv1);
        perror("Execution failed");
        exit(1);
    }

    pid_t pid2 = fork();
    if (pid2 == 0)
    {
        close(pipefd[1]);
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        execvp(argv2[0], argv2);
        perror("Execution failed");
        exit(1);
    }

    close(pipefd[0]);
    close(pipefd[1]);
    wait(NULL);
    wait(NULL);
}

void execute_multiple_pipes(char **argv, int pipe_count, char **pipe_programs, bool io_flag, char io_type, char *io_file)
{
    int pipefd[2 * pipe_count];
    for (int i = 0; i < pipe_count; i++)
    {
        if (pipe(pipefd + i * 2) == -1)
        {
            perror("pipe failed");
            exit(1);
        }
    }

    int pid;
    int j = 0;
    for (int i = 0; i <= pipe_count; i++)
    {
        pid = fork();
        if (pid == 0)
        {
            if (i != 0)
            {
                if (dup2(pipefd[j - 2], STDIN_FILENO) == -1)
                {
                    perror("dup2 failed");
                    exit(1);
                }
            }

            if (i != pipe_count)
            {
                if (dup2(pipefd[j + 1], STDOUT_FILENO) == -1)
                {
                    perror("dup2 failed");
                    exit(1);
                }
            }
            else
            {
                handle_io_redirection(io_flag, io_type, io_file);
            }

            for (int k = 0; k < 2 * pipe_count; k++)
            {
                close(pipefd[k]);
            }

            if (i == 0)
            {
                execvp(argv[0], argv);
            }
            else
            {
                char *cmd_argv[ARGUMENTS_SIZE];
                bool echo_flag = false, io_flag = false, pipe_flag = false, io_print = false;
                char io_type;
                char *io_file = NULL;
                int pipe_count = 0;

                parse(pipe_programs[i - 1], cmd_argv, &echo_flag, &io_flag, &pipe_flag, &io_type, &io_file, pipe_programs, &pipe_count, &io_print);
                execvp(cmd_argv[0], cmd_argv);
            }
            perror("Execution failed");
            exit(1);
        }
        else if (pid < 0)
        {
            perror("Fork failed");
            exit(1);
        }
        j += 2;
    }

    for (int i = 0; i < 2 * pipe_count; i++)
    {
        close(pipefd[i]);
    }

    for (int i = 0; i <= pipe_count; i++)
    {
        wait(NULL);
    }
}

void handle_io_redirection(bool io_flag, char io_type, char *io_file)
{
    if (io_flag)
    {
        if (io_type == '>')
        {
            int fd = open(io_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0)
            {
                perror("Output file open failed");
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
        else if (io_type == '<')
        {
            int fd = open(io_file, O_RDONLY);
            if (fd < 0)
            {
                perror("Input file open failed");
                exit(1);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
    }
}

void execute(char **argv, bool io_flag, char io_type, char *io_file)
{
    if (argv[0] == NULL)
    {
        return;
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        perror("Fork failed");
        exit(1);
    }
    else if (pid == 0)
    {
        handle_io_redirection(io_flag, io_type, io_file);

        if (execvp(argv[0], argv) < 0)
        {
            perror("Execution failed");
            exit(1);
        }
    }
    else
    {
        int status;
        waitpid(pid, &status, 0);
    }
}

int main(int argc, const char *argv[])
{
    char buf[BUFSIZ];
    char last_command[HISTORY_SIZE] = "";

    char *arguments[ARGUMENTS_SIZE];
    char *pipe_programs[ARGUMENTS_SIZE];
    bool echo_flag = false, io_flag = false, pipe_flag = false, io_print = false;
    char io_type;
    char *io_file = NULL;
    int pipe_count = 0;

    while (1)
    {
        memset(buf, 0, sizeof(buf));
        printf("> ");
        char *p = fgets(buf, BUFSIZ, stdin);
        if (p == NULL)
        {
            perror("fgets failed");
            continue;
        }
        buf[strcspn(buf, "\n")] = '\0';

        if (strcmp(buf, "!!") == 0)
        {
            if (strlen(last_command) == 0)
            {
                printf("No previous command to execute.\n");
                continue;
            }
            printf("%s\n", last_command);
            strcpy(buf, last_command);

            if (strncmp(last_command, "cd ", 3) == 0)
            {
                char *dir = last_command + 3;
                chdir(dir);
                continue;
            }

            parse(buf, arguments, &echo_flag, &io_flag, &pipe_flag, &io_type, &io_file, pipe_programs, &pipe_count, &io_print);
        }
        if (strncmp(buf, "cd ", 3) == 0)
        {
            char *dir = buf + 3;

            strcpy(last_command, buf);

            if (chdir(dir) != 0)
            {
                perror("cd failed");
            }
            continue;
        }

        else
        {
            strcpy(last_command, buf);
        }

        if (strncmp(buf, "exit", 4) == 0)
        {
            printf("Exiting shell...\n");
            exit(0);
        }
        else if (strncmp(buf, "help", 4) == 0)
        {
            printf("Shell program.\n");
            printf("Built-in commands:\n");
            printf("  help \n");
            printf("  cd <dir> \n");
            printf("  mkdir <dir> \n");
            printf("  !! \n");
            printf("  exit \n");
            continue;
        }
        else if (strncmp(buf, "mkdir ", 6) == 0)
        {
            char *dir = buf + 6;
            if (mkdir(dir, 0755) != 0)
            {
                perror("mkdir failed");
            }
            continue;
        }

        memset(arguments, 0, sizeof(arguments));
        memset(pipe_programs, 0, sizeof(pipe_programs));

        parse(buf, arguments, &echo_flag, &io_flag, &pipe_flag, &io_type, &io_file, pipe_programs, &pipe_count, &io_print);

        if (echo_flag)
        {
            for (int i = 0; arguments[i] != NULL; i++)
            {
                printf("%s ", arguments[i]);
            }
            printf("\n");
        }
        else if (io_print && io_file)
        {
            if (io_type == '>')
                printf("GT %s\n", io_file);
            else if (io_type == '<')
                printf("LT %s\n", io_file);
        }
        else if (pipe_flag)
        {
            for (int i = 0; i < pipe_count; i++)
            {
                printf("PIPE %s ", pipe_programs[i]);
            }
            printf("\n");
            execute_multiple_pipes(arguments, pipe_count, pipe_programs, io_flag, io_type, io_file);
        }
        else
        {
            execute(arguments, io_flag, io_type, io_file);
        }
    }

    return 0;
}