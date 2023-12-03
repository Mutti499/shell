#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>

#define MAX_COMMAND_LENGTH 1024

// Function to split the command line into arguments
int parse_command(char* command, char** args) {
    int count = 0;
    char* separator = " \t\n";
    char* parsed;

    parsed = strtok(command, separator);
    while (parsed != NULL) {
        args[count] = parsed;
        count++;
        parsed = strtok(NULL, separator);
    }

    args[count] = NULL;  // NULL-terminate the argument list
    return count;  // Number of arguments
}

// Function to execute the command
int execute_command(char** args, bool background) {
    pid_t pid;

    pid = fork();
    if (pid < 0) {
        fprintf(stderr, "Fork Failed");
        return 1;
    } else if (pid == 0) {
        // Child process
        // execlp(args[0], args[0], NULL); // The additional arguments would go after args[0] before NULL
        execvp(args[0], args);// Execlp can not be used since it does not take the args array so it is not dynamic

    } else { // Parent process
        if (!background) {
            wait(NULL);
            printf("Child Completed \n");
        }else{
            printf("Child is running in background \n");
        }
    }
    return 0;
}

void check_background() {
    int status;
    pid_t pid;
    pid = waitpid(-1, &status, WNOHANG);

    //Re implement here
    // if (WIFEXITED(status)){
    //     status = WEXITSTATUS(status);
    //     if(status == 0){
    //         // Child terminated normally
    //         printf("Child %d terminated normally\n", pid);
    //     } else {
    //         // Child is terminated by something else.
    //         printf("Child %d terminated with exit status %d\n", pid, status);
    //     }
    // } else if (WIFSIGNALED(status)) {
    //     // Child killed by a signal.
    //     printf("Child %d terminated by signal %d\n", pid, WTERMSIG(status));
    // } else if (WIFSTOPPED(status)) {
    //     // Child stopped.
    //     printf("Child %d stopped by signal %d\n", pid, WSTOPSIG(status));
    // } else if (WIFCONTINUED(status)) {
    //     // Child continue.
    //     // unnecessary branch
    //     printf("Child %d continued\n", pid);
    // }

}


int main() {
    char command[MAX_COMMAND_LENGTH];
    char* args[64];

    while(1) {
        check_background();
        printf("myshell> ");  
        fflush(stdout); // WHY?

        if (fgets(command, MAX_COMMAND_LENGTH, stdin) == NULL) {
            perror("fgets failed");
            continue;
        }

        // Check for the 'exit' command
        if (strcmp(command, "exit\n") == 0) {
            printf("Exiting myshell...\n");
            return 0;
        }
        
        int arg_count = parse_command(command, args);
        if (arg_count == 0) {  // No command entered
            continue;
        }
        check_background();
        bool background;
        if (strcmp(args[arg_count-1], "&") == 0) {
            args[arg_count-1] = NULL;
            arg_count--;
            background = true;
        } else {
            background = false;
        }
        
        execute_command(args, background);
        // check_background();
    }


}


//Cd is not working since child process is not able to change the directory of the parent process
    // if (strcmp(args[0], "cd") == 0) {
    //     // If 'cd' command, change the directory in the parent process
    //     if (args[1] == NULL) {
    //         fprintf(stderr, "cd: missing operand\n");
    //     } else {
    //         if (chdir(args[1]) != 0) {
    //             // If chdir fails, print the error
    //             perror("cd failed");
    //         }
    //     }
    //     return 0; // Return early, do not fork a child process
    // }

//Hoca gcc kullanip baska bir kodu execute etmek isteyebilir