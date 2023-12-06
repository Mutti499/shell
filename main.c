#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdbool.h>
#include <fcntl.h>

#define MAX_COMMAND_LENGTH 1024
#define READ_END 0
#define WRITE_END 1
#define BUFFER_SIZE 500

// Function to split the command line into arguments
int parse_command(char *command, char **args)
{
    int count = 0;
    char *separator = " \t\n";
    char *parsed;

    parsed = strtok(command, separator);
    while (parsed != NULL)
    {
        args[count] = parsed;
        count++;
        parsed = strtok(NULL, separator);
    }

    args[count] = NULL; // NULL-terminate the argument list
    return count;       // Number of arguments
}

void reverse_buffer(char *buffer, ssize_t count)
{
    for (int i = 0; i < count / 2; ++i)
    {
        char temp = buffer[i];
        buffer[i] = buffer[count - i - 1];
        buffer[count - i - 1] = temp;
    }
}

// Function to execute the command
int execute_command(char **args, bool background, bool redirect1, bool redirect2, bool redirect3, char *output_file)
{
    int fd[2];

    pid_t pid;

    if (redirect3)
    {
        if (pipe(fd) == -1)
        {
            perror("pipe");
            return 1;
        }
    }

    pid = fork();

    if (pid < 0)
    {
        fprintf(stderr, "Fork Failed");
        return 1;
    }
    else if (pid == 0)
    {
        // Child process
        // execlp(args[0], args[0], NULL); // The additional arguments would go after args[0] before NULL
        // if (output_file == NULL)
        // {
        //     perror("File does not exist failed");
        // }

        if (redirect1)
        {

            int file = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (file < 0)
            {
                perror("Cant open file");
            }
            dup2(file, STDOUT_FILENO);
            close(file);
        }
        else if (redirect2)
        {
            int file = open(output_file, O_WRONLY | O_CREAT | O_APPEND, 0666);
            if (file < 0)
            {
                perror("Cant open file");
            }
            dup2(file, STDOUT_FILENO);
            close(file);
        }
        else if (redirect3)
        {

            close(fd[READ_END]);                // Close read end, child doesn't need it
            dup2(fd[WRITE_END], STDOUT_FILENO); // Redirect stdout to write end of pipe
            close(fd[WRITE_END]);               // Close write end after duplicating
        }

        execvp(args[0], args); // Execlp can not be used since it does not take the args array so it is not dynamic
    }
    else
    { // Parent process

        if (redirect3)
        {
            close(fd[WRITE_END]);
            char read_msg[BUFFER_SIZE];
            ssize_t count = read(fd[READ_END], read_msg, BUFFER_SIZE);

            if (count > 0)
            {
                reverse_buffer(read_msg, count);

                // Write the reversed buffer to the file
                int file_fd = open(output_file, O_WRONLY | O_CREAT | O_APPEND, 0666);
                if (file_fd < 0)
                {
                    perror("Cannot open file");
                    exit(EXIT_FAILURE);
                }
                write(file_fd, read_msg, count);
                close(file_fd);
            }
            close(fd[READ_END]);
        }

        if (!background)
        {
            wait(NULL);
            printf("Child Completed \n");
        }
        else
        {
            printf("Child is running in background \n");
        }
    }
    return 0;
}

void check_background()
{
    int status;
    pid_t pid;
    pid = waitpid(-1, &status, WNOHANG);

    if (pid > 0)
    {
        if (WIFEXITED(status))
        {
            int exit_status = WEXITSTATUS(status);
            if (exit_status == 0)
            {
                printf("Child %d terminated normally\n", pid);
            }
            else
            {
                printf("Child %d terminated with exit status %d\n", pid, exit_status);
            }
        }
        else if (WIFSIGNALED(status))
        {
            // Child killed by a signal.
            printf("Child %d terminated by signal %d\n", pid, WTERMSIG(status));
        }
        else if (WIFSTOPPED(status))
        {
            // Child stopped.
            printf("Child %d stopped by signal %d\n", pid, WSTOPSIG(status));
        }
        else if (WIFCONTINUED(status))
        {
            // Child continue.
            printf("Child %d continued\n", pid);
        }
    }
    if (pid == -1 && errno != ECHILD)
    {
        perror("waitpid failed");
    }
    return;
}





int main()
{
    char command[MAX_COMMAND_LENGTH];
    char *args[64];

    while (1)
    {
        check_background();
        printf("myshell> ");
        fflush(stdout); // WHY?

        if (fgets(command, MAX_COMMAND_LENGTH, stdin) == NULL)
        {
            perror("fgets failed");
            continue;
        }

        // Check for the 'exit' command
        if (strcmp(command, "exit\n") == 0)
        {
            printf("Exiting myshell...\n");
            return 0;
        }

        int arg_count = parse_command(command, args);
        if (arg_count == 0)
        { // No command entered
            continue;
        }
        check_background();
        bool background;
        if (strcmp(args[arg_count - 1], "&") == 0)
        {
            args[arg_count - 1] = NULL;
            arg_count--;
            background = true;
        }
        else
        {
            background = false;
        }

        bool redirect1;
        bool redirect2;
        bool redirect3;
        char *output_file;
        for (int i = 0; i < arg_count; i++)
        {
            if (strcmp(args[i], ">") == 0)
            {
                redirect1 = true;
                output_file = args[i + 1];
                args[i] = NULL;
                arg_count -= 2;
                break;
            }
            else if (strcmp(args[i], ">>") == 0)
            {
                redirect2 = true;
                output_file = args[i + 1];
                args[i] = NULL;
                arg_count -= 2;
                break;
            }
            else if (strcmp(args[i], ">>>") == 0)
            {
                redirect3 = true;
                output_file = args[i + 1];
                args[i] = NULL;
                arg_count -= 2;
                break;
            }
            else
            {
                redirect1 = false;
                redirect2 = false;
                redirect3 = false;
            }
        }




        execute_command(args, background, redirect1, redirect2, redirect3, output_file);

        // check_background();
    }
}



