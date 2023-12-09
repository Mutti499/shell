#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdbool.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>


#define MAX_COMMAND_LENGTH 1024
#define READ_END 0
#define WRITE_END 1
#define BUFFER_SIZE 500

char aliasses[64][256];       // 64 strings, each can hold 255 characters + null terminator
char alias_commands[64][256]; // Similarly for command strings
int alias_count = 0;

char parent_shell[256];

// volatile sig_atomic_t child_process_count = 0;

// void sigchld_handler(int signum) {
//     int status;
//     while (waitpid(-1, &status, WNOHANG) > 0) {
//         child_process_count--;
//     }
// }


// Function to split the command line into arguments
int parse_command(char *command, char args[64][256])
{
    int count = 0;
    char *separator = " \t\n";
    char *parsed;

    parsed = strtok(command, separator);
    while (parsed != NULL)
    {
        strncpy(args[count], parsed, 256 - 1); // Copy the token
        args[count][255] = '\0';               // Ensure null termination

        count++;
        parsed = strtok(NULL, separator);
    }

    args[count][0] = '\0'; // NULL-terminate the last argument
    return count;          // Number of arguments
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
int execute_command(char args[64][256], bool background, bool redirect1, bool redirect2, bool redirect3, char *output_file)
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

            close(fd[READ_END]);                // Close read end
            dup2(fd[WRITE_END], STDOUT_FILENO); // Redirect stdout to write end of pipe
            close(fd[WRITE_END]);               // Close write end
        }

        char *arg_pointers[65]; // Array of pointers
        int j = 0;
        for (int i = 0; i < 64 && args[i][0] != '\0'; i++)
        {
            arg_pointers[j++] = args[i]; // Only copy non-empty arguments
        }
        arg_pointers[j] = NULL; // NULL-terminate the array

        // execvp(args[0], args); // Execlp can not be used since it does not take the args array so it is not dynamic
        if (execvp(arg_pointers[0], arg_pointers) == -1)
        {
            if (errno == ENOENT)
            {
                fprintf(stderr, "%s: Command not found\n", arg_pointers[0]);
            }
            exit(EXIT_FAILURE); // Only exit the child process, not the shell itself
        }else{
            _exit(0);
        }
        
    }
    else
    { // Parent process

        // child_process_count++;

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
        }
        else
        {
            // printf("Child is running in background \n");
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

int save_alias(char *alias_command, char **alias_args)
{
    FILE *fp;
    fp = fopen("alias.txt", "a");
    if (fp == NULL)
    {
        perror("Error opening file");
        return -1;
    }
    fprintf(fp, "%s = ", alias_command);
    for (size_t i = 0; i < sizeof(alias_args); i++)
    {

        if (alias_args[i] == NULL)
        {
            break;
        }

        fprintf(fp, "%s ", alias_args[i]);
    }
    fprintf(fp, "\n");
    fclose(fp);
    return 0;
}

int load_aliasses()
{
    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    fp = fopen("alias.txt", "r");
    if (fp == NULL)
    {
        perror("Error opening file");
        return -1;
    }
    char key[256];
    char command[256];
    while ((read = getline(&line, &len, fp)) != -1)
    {
        // printf("%s", line);
        if (sscanf(line, "%s = \"%[^\"]\"", key, command) == 2)
        {
            strcpy(aliasses[alias_count], key);
            strcpy(alias_commands[alias_count], command);

            alias_count++;
        }
    }
    fclose(fp);
    if (line)
        free(line);
    return 0;
}

void executor(char args[64][256], int arg_count)
{
    bool background;
    if (strcmp(args[arg_count - 1], "&") == 0)
    {
        args[arg_count - 1][0] = '\0'; // Null-terminate the string
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
            args[i][0] = '\0';
            arg_count -= 2;
            break;
        }
        else if (strcmp(args[i], ">>") == 0)
        {
            redirect2 = true;
            output_file = args[i + 1];
            args[i][0] = '\0';
            arg_count -= 2;
            break;
        }
        else if (strcmp(args[i], ">>>") == 0)
        {
            redirect3 = true;
            output_file = args[i + 1];
            args[i][0] = '\0';
            arg_count -= 2;
            break;
        }
        redirect1 = false;
        redirect2 = false;
        redirect3 = false;
    }
    char *alias_command;
    char *alias_args[64];
    if (strcmp(args[0], "alias") == 0)
    {
        alias_command = args[1];
        char command_buffer[1024] = "";
        for (size_t i = 3; i < arg_count; i++) // after the =
        {
            if (args[i] == NULL)
            {
                break;
            }
            alias_args[i - 3] = args[i];
            strcat(command_buffer, args[i]);
            strcat(command_buffer, " "); // Add a space between arguments
        }
        strcpy(aliasses[alias_count], alias_command);
        strcpy(alias_commands[alias_count], command_buffer);

        save_alias(alias_command, alias_args);
        alias_count++;
    }
    else
    {
        // printf("%s \n", args[0]);
        // printf("%s \n", args[1]);
        // printf("%s \n", args[2]);
        // printf("%s \n", args[3]);
        execute_command(args, background, redirect1, redirect2, redirect3, output_file);
    }
}

char *get_current_shell_from_proc()
{
    memset(parent_shell, 0, sizeof(parent_shell));

    pid_t ppid = getppid();
    char shell_path[256];
    printf("ppid: %d \n", ppid);
    sprintf(shell_path, "/proc/%d/comm", ppid);

    FILE *fp = fopen(shell_path, "r"); // Open the comm file for reading

    fgets(parent_shell, sizeof(parent_shell), fp);
    fclose(fp);

    // Remove newline character if present
    char *newline = strchr(parent_shell, '\n');
    if (newline) *newline = '\0';

    return parent_shell;
}

void bello_executer(char *last_executed_command)
{
    char *username = getenv("USER");
    char hostname[512];
    gethostname(hostname, 512);

    char *tty = ttyname(STDIN_FILENO);

    // char* shell = getenv("SHELL"); // This moight not work when there are other shells in the system it just indicates default shell
    // char* shell = get_current_shell_from_proc();
    if (parent_shell[0] == '\0')
    {
        get_current_shell_from_proc();
    }

    char *home = getenv("HOME");

    time_t t; // not a primitive datatype
    time(&t);

    printf("Username: %s\n", username);
    printf("Hostname: %s\n", hostname);
    printf("Last Executed Command: %s\n", last_executed_command);
    printf("TTY: %s\n", tty);
    printf("Current Shell Name: %s\n", parent_shell);
    printf("Home Location: %s\n", home);
    printf("Current Time and Date: %s\n", ctime(&t));
}

void reset_args(char args[64][256])

{
    for (int i = 0; i < 64; i++)
    {
        args[i][0] = '\0';
    }
}


// In your main function or initialization code

int main()
{

    // Rest of your code...
    // struct sigaction sa;

    // sa.sa_handler = sigchld_handler;
    // sigemptyset(&sa.sa_mask);
    // sa.sa_flags = 0;
    // sigaction(SIGCHLD, &sa, NULL);

    char command[MAX_COMMAND_LENGTH];
    char args[64][256];
    char args2[64][256];

    char hostname[512];
    gethostname(hostname, 512);

    char *username = getenv("USER");

    char cwd[512];
    getcwd(cwd, 512);

    char shellCommand[1030];
    sprintf(shellCommand, "%s@%s %s --- ", username, hostname, cwd);

    load_aliasses();
    char last_executed_command[1024] = "";
    while (1)
    {

        reset_args(args);
        check_background();
        printf("%s ", shellCommand);
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

        for (int i = 0; i < alias_count; i++)
        {
            if (strcmp(aliasses[i], args[0]) == 0)
            {

                char command2[1024];
                strcpy(command2, alias_commands[i]);
                int arg_count2 = parse_command(command2, args2);
                for (int k = 1; k < arg_count; k++)
                {
                    strcpy(args2[arg_count2++], args[k]);
                }
                arg_count = arg_count2;

                int formatter;
                for (formatter = 0; i < 64 && args2[formatter][0] != '\0'; formatter++)
                {
                    strcpy(args[formatter], args2[formatter]);
                }

                args[formatter][0] = '\0';

                break;
            }
        }
        if (strcmp(args[0], "bello") == 0)
        {
            bello_executer(last_executed_command);
            continue;
        }

        // printf("Args1: %s %s %s %s %s %s \n", args[0], args[1], args[2], args[3], args[4], args[5]);
        // printf("arg counter : %d \n", arg_count);
        executor(args, arg_count);
        strcpy(last_executed_command, command);
    }
}

// Bello yaz
// Syntax error tespiti yap
// cift tirnak ayirmalari guncelle
// rapor bak

// https://moodle.boun.edu.tr/mod/forum/discuss.php?d=141800

// When we print echo "message" in a regular terminal, we get <message> as output, but in my implementation i get with double quotes. Should I remove double quotes or is it okay?
// Better if you remove