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
#define BUFFER_SIZE 1024

char aliasses_keys[64][256];   // 64 strings, each can hold 255 characters + null terminator
char aliasses_values[64][256]; // Similarly for command strings
int alias_count = 0;

char last_executed_command[1024] = "";

char parent_shell[256];

int background_process_count = 0;

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

char *get_current_shell_from_proc()
{
    memset(parent_shell, 0, sizeof(parent_shell));

    pid_t ppid = getppid();
    char shell_path[256];
    sprintf(shell_path, "/proc/%d/comm", ppid);


    FILE *fp = fopen(shell_path, "r"); // Open the comm file for reading

    fgets(parent_shell, sizeof(parent_shell), fp);
    fclose(fp);

    // Remove newline character if present
    char *newline = strchr(parent_shell, '\n');
    if (newline)
        *newline = '\0';

    return parent_shell;
}

char *executable_path_finder(char *cmd)
{
    char *path = getenv("PATH");
    if (!path)
        return NULL;

    char *dirs = strdup(path);
    if (!dirs)
        return NULL;

    // printf("dirs: %s \n", dirs); Al paths

    char *dir = strtok(dirs, ":");
    while (dir != NULL)
    {

        char *path = malloc(strlen(dir) + strlen(cmd) + 2); // +2 for the slash and null terminator
        sprintf(path, "%s/%s", dir, cmd);
        if (access(path, X_OK) == 0)
        {
            free(dirs);
            return path; // the executable path
        }

        free(path);
        dir = strtok(NULL, ":");
    }

    free(dirs);
    return NULL;
}

void bello_executer()
{
    char *username = getenv("USER");
    char hostname[512];
    gethostname(hostname, 512);

    char *tty = ttyname(STDIN_FILENO);

    // char* shell = getenv("SHELL"); // This moight not work when there are other shells in the system it just indicates default shell
    // char* shell = get_current_shell_from_proc();

    char *home = getenv("HOME");

    time_t t; // not a primitive datatype
    time(&t);

    printf("Username: %s\nHostname: %s\nLast SUCCESSFULLY Executed Command: %s\nTTY: %s\nCurrent Shell Name: %s\nHome Location: %s\nCurrent Time and Date: %sNumber of Process in Background: %d\n",
           username, hostname, last_executed_command, tty, parent_shell, home, ctime(&t), background_process_count);

    return;
}
// In your main function or initialization code
void eraser(char args[64][256])
{
    for (int i = 0; i < 64; i++)
    {
        int len = strlen(args[i]);
        bool in_quotes = false;
        int shift_offset = 0;

        for (int j = 0; j < len; j++)
        {
            if (args[i][j] == '"' || args[i][j] == '\'')
            {
                in_quotes = !in_quotes; // Toggle the in_quotes status
                shift_offset++;         // Increase the shift offset
            }
            else if (shift_offset > 0)
            {
                args[i][j - shift_offset] = args[i][j]; // Shift characters to the left
            }
        }
        args[i][len - shift_offset] = '\0'; // Null-terminate the string at the new length
    }
}

int controller(char args[64][256])
{
    int single_counter = 0;
    int double_counter = 0;

    for (int i = 0; i < 64; i++)
    {
        for (int j = 0; args[i][j] != '\0'; j++)
        { // end of a word
            if (args[i][j] == '"')
            {
                double_counter++;
            }
            else if (args[i][j] == '\'')
            {
                single_counter++;
            }
            else
            {
                continue;
            }
        }
    }

    if (single_counter % 2 != 0 || double_counter % 2 != 0)
    {
        printf("Syntax error: unbalanced quotes!\n");
        return 1;
    }
    return 0;
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
        else if (redirect3 && strcmp(args[0], "bello") == 0)
        {
            close(fd[READ_END]);                // Close read end
            dup2(fd[WRITE_END], STDOUT_FILENO); // Redirect stdout to write end of pipe
            close(fd[WRITE_END]);               // Close write end
        }
        else if (redirect3)
        {
            close(fd[READ_END]);                // Close read end
            dup2(fd[WRITE_END], STDOUT_FILENO); // Redirect stdout to write end of pipe
            close(fd[WRITE_END]);               // Close write end
        }

        if (controller(args) == 1)
        {
            _exit(1);
        }
        if (strcmp(args[0], "echo") == 0)
        {
            eraser(args);
        }
        if (strcmp(args[0], "bello") == 0)
        {
            bello_executer();
            _exit(0);
        }
        else
        {
            // printf("Executing %s \n", args[0]);
            char *arg_pointers[65]; // Array of pointers
            int j = 0;
            for (int i = 0; i < 64 && args[i][0] != '\0'; i++)
            {
                arg_pointers[j++] = args[i]; // Only copy non-empty arguments
            }
            arg_pointers[j] = NULL; // NULL-terminate the array

            // execvp(arg_pointers[0], arg_pointers); // Execlp can not be used since it does not take the args array so it is not dynamic

            char *executable_path = executable_path_finder(arg_pointers[0]);
            if (executable_path)
            {
                if (execv(executable_path, arg_pointers) == -1)
                {
                    fprintf(stderr, "Error executing %s: %s\n", executable_path, strerror(errno));
                    exit(EXIT_FAILURE);
                }
                free(executable_path);
                _exit(0);
            }
            else
            {
                fprintf(stderr, "%s: Command not found\n", arg_pointers[0]);
                exit(EXIT_FAILURE);
            }
        }
    }
    else
    { // Parent process

        if (redirect3)
        {
            close(fd[WRITE_END]);
            char read_msg[BUFFER_SIZE];
            ssize_t count = read(fd[READ_END], read_msg, BUFFER_SIZE);
            // printf("Read %ld bytes from the pipe: \"%.*s\"\n", count, (int)count, read_msg);

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
            int status;
            waitpid(pid, &status, 0); // Waits for the specific child process to complete and captures its status
            if (WIFEXITED(status))
            {
                return WEXITSTATUS(status); // Return the exit status of the child process
            }
        }
        else
        {
            // printf("Child is running in background \n");
            background_process_count++;
        }
    }
    return 0;
}

void check_background()
{
    int status;
    pid_t pid;

    while (pid = waitpid(-1, &status, WNOHANG) > 0)
    {
        background_process_count--;

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
    }
    if (pid == -1 && errno != ECHILD)
    {
        perror("waitpid failed");
    }
    return;
}

int save_alias(char *alias_key, char *alias_value, bool is_exist)
{

    FILE *fp;
    if (!is_exist)
    {
        fp = fopen("alias.txt", "a");
        if (fp == NULL)
        {
            perror("Error opening file");
            return -1;
        }
        fprintf(fp, "%s = %s\n", alias_key, alias_value);
    }
    else
    {
        fp = fopen("alias.txt", "w");
        if (fp == NULL)
        {
            perror("Error opening file");
            return -1;
        }
        for (int i = 0; i < alias_count; i++)
        {
            fprintf(fp, "%s = %s\n", aliasses_keys[i], aliasses_values[i]);
        }
    }

    fclose(fp);
    return 0;
}

int load_aliasses_keys()
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
        if (sscanf(line, "%s = %[^\n]", key, command) == 2)
        {
            strcpy(aliasses_keys[alias_count], key);
            strcpy(aliasses_values[alias_count], command);

            alias_count++;
        }
    }
    fclose(fp);
    if (line)
        free(line);
    return 0;
}

int executor(char args[64][256], int arg_count)
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
    char *alias_key;
    char *alias_value[64];
    if (strcmp(args[0], "alias") == 0)
    {
        alias_key = args[1];
        char command_buffer[1024] = "";
        for (size_t i = 3; i < arg_count; i++) // after the =
        {
            if (args[i] == NULL)
            {
                break;
            }
            alias_value[i - 3] = args[i];
            strcat(command_buffer, args[i]);
            strcat(command_buffer, " "); // Add a space between arguments
        }

        int len = strlen(command_buffer);
        int quote_count = 0;
        int last_quote_pos = -1;

        for (int i = 0; i < len; i++)
        {
            if (command_buffer[i] == '"')
            {
                last_quote_pos = i;
                quote_count++;
            }
        }

        command_buffer[last_quote_pos] = '#'; // Null-terminate to remove the last quote

        char value[256];
        if (sscanf(command_buffer, "\"%[^#]\"", value) != 1)
        {
            printf("Syntax error: unbalanced quotes!\n");
            return 1;
        }

        bool is_exist = false;
        for (int i = 0; i < alias_count; i++)
        {
            if (strcmp(aliasses_keys[i], alias_key) == 0)
            {
                strcpy(aliasses_values[i], value);
                is_exist = true;
                break;
            }
        }

        if (!is_exist)
        {
            strcpy(aliasses_keys[alias_count], alias_key);
            strcpy(aliasses_values[alias_count], value);
            alias_count++;
        }
        
        save_alias(alias_key, value, is_exist);

        return 0;
    }
    else
    {
        return execute_command(args, background, redirect1, redirect2, redirect3, output_file);
    }
}

void reset_args(char args[64][256])

{
    for (int i = 0; i < 64; i++)
    {
        args[i][0] = '\0';
    }
}

int main()
{
    char command[MAX_COMMAND_LENGTH];
    char args[64][256];
    char args2[64][256];

    get_current_shell_from_proc();

    char hostname[512];
    gethostname(hostname, 512);

    char *username = getenv("USER");

    char cwd[512];
    getcwd(cwd, 512);

    char shellCommand[1030];
    sprintf(shellCommand, "%s@%s %s --- ", username, hostname, cwd);

    load_aliasses_keys();

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
            if (strcmp(aliasses_keys[i], args[0]) == 0)
            {

                char command2[1024];
                strcpy(command2, aliasses_values[i]);
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

        // printf("Args1: %s %s %s %s %s %s \n", args[0], args[1], args[2], args[3], args[4], args[5]);
        // printf("arg counter : %d \n", arg_count);

        int res = executor(args, arg_count);
        // printf("return value: %d \n", res);
        if (res == 0)
        {
            strcpy(last_executed_command, command);
        }
    }
}
// rapor bak

// change alias not implemented