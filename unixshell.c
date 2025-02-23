#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

#define ARGUMENTS_SIZE 64
#define HISTORY_SIZE BUFSIZ

void parse(char *line, char **argv, bool *echo_flag, bool *io_flag, bool *pipe_flag, char *io_type, char **io_file, char **pipe_programs, int *pipe_count)
{
    *echo_flag = false;
    *io_flag = false;
    *pipe_flag = false;
    *io_file = NULL;
    *pipe_count = 0;
    char *last_arg = NULL;

    while (*line != '\0')
    {
        while (isspace(*line))
        {
            *line++ = '\0';
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
        else if (strcmp(last_arg, "IO") == 0 || strcmp(last_arg, "PIPE") == 0)
        {
            *(argv - 1) = NULL;
        }
    }
}

void execute(char **argv)
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
    char last_directory[BUFSIZ] = "";

    char *arguments[ARGUMENTS_SIZE];
    char *pipe_programs[ARGUMENTS_SIZE];
    bool echo_flag = false, io_flag = false, pipe_flag = false;
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

            parse(buf, arguments, &echo_flag, &io_flag, &pipe_flag, &io_type, &io_file, pipe_programs, &pipe_count);
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
        //

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
            printf("  help  - Show this help message\n");
            printf("  cd <dir> - Change directory\n");
            printf("  mkdir <dir> - Create a new directory\n");
            printf("  !! - Run the last command\n");
            printf("  exit - Exit the shell\n");
            continue;
        }
        else if (strncmp(buf, "cd ", 3) == 0)
        {
            char *dir = buf + 3;
            if (getcwd(last_directory, sizeof(last_directory)) == NULL)
            {
                perror("getcwd failed");
                continue;
            }
            if (chdir(dir) != 0)
            {
                perror("cd failed");
            }
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

        parse(buf, arguments, &echo_flag, &io_flag, &pipe_flag, &io_type, &io_file, pipe_programs, &pipe_count);

        if (echo_flag)
        {
            for (int i = 0; arguments[i] != NULL; i++)
            {
                printf("%s ", arguments[i]);
            }
            printf("\n");
        }
        else if (io_flag && io_file)
        {
            if (io_type == '<')
                printf("LT %s\n", io_file);
            else if (io_type == '>')
                printf("GT %s\n", io_file);
        }
        else if (pipe_flag)
        {
            for (int i = 0; i < pipe_count; i++)
            {
                printf("PIPE %s ", pipe_programs[i]);
            }
            printf("\n");
        }
        else
        {
            execute(arguments);
        }
    }

    return 0;
}
