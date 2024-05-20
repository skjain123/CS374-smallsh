#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <signal.h>

#define _XOPEN_SOURCE 700

#define EXIT_KEY        "exit"
#define CDIR_KEY        "cd"
#define STAT_KEY        "status"

#define MAX_CHAR        2048
#define MAX_ARG         512

#define EXTRA_ARGS      "Too many Arguments!\n"

#define INPUT_KEY       "<"
#define OUTPUT_KEY      ">"
#define BACKGROUND_KEY  "&"

#define PID_SYMBOL      "$$"

volatile sig_atomic_t sigint_received = 0;
volatile sig_atomic_t foreground_only_mode = 0;
volatile pid_t foreground_pid = -1;

struct arr {
    char* command[MAX_ARG];
    int arg_num;

    char input_file[MAX_CHAR];
    int input_bool;
    
    char output_file[MAX_CHAR];
    int output_bool;

    int background_process_flag;
    int built_in_command_flag;

    int exit_flag;
};

void print_console (char input[MAX_CHAR]) {
    char userInput[MAX_CHAR];

    /* get user input */
    printf(": ");
    fflush(stdout);

    fgets(userInput, MAX_CHAR, stdin);
    if (strlen(userInput) > 0 && userInput[strlen(userInput) - 1] == '\n') {
        userInput[strlen(userInput) - 1] = '\0';
    }

    strcpy(input, userInput);
}

void process_input(char input[MAX_CHAR], struct arr *cmd) {
    char* token;
    cmd->arg_num = 0;

    token = strtok(input, " ");

    while (token != NULL && cmd->arg_num < MAX_ARG - 1) {
        cmd->command[cmd->arg_num] = malloc(strlen(token) + 1);  /* Allocate memory for the string */

        if (cmd->command[cmd->arg_num] == NULL) {
            perror("malloc failed\n");
            exit(EXIT_FAILURE);
        }

        /* check for inline comments in the input, if there are do not parse any further */
        if (strcmp(token,"//") == 0) {
            break;
        }

        strcpy(cmd->command[cmd->arg_num], token);  /* Copy the token into the allocated memory */
        token = strtok(NULL, " ");
        cmd->arg_num++;
    }

    cmd->command[cmd->arg_num] = NULL;
}

void remove_from_command (struct arr* cmd, int index) {
    
    
    if (cmd->command[index] != NULL) {
        free(cmd->command[index]);

        for (int i = index; i < cmd->arg_num - 1; i++) {
            cmd->command[i] = cmd->command[i+1];
        }
        cmd->command[cmd->arg_num - 1] = NULL;
        cmd->arg_num--;
    }
    
}

void get_input_output_background(struct arr* cmd) {

    cmd->input_bool = 0;
    cmd->output_bool = 0;

    for (int i = 0; i < cmd->arg_num && cmd->command[i] != NULL; i++) {
        if (strcmp(cmd->command[i], INPUT_KEY) == 0) {
            if (i + 1 < cmd->arg_num) {
                strcpy(cmd->input_file, cmd->command[i+1]);
                remove_from_command(cmd, i); // remove <
                remove_from_command(cmd, i); // remove input filename

                cmd->input_bool = 1;
            } else {
                printf("No input file provided!\n");
                fflush(stdout);
            }
            
        } else if (strcmp(cmd->command[i], OUTPUT_KEY) == 0) {
            if (i + 1 < cmd->arg_num) {
                strcpy(cmd->output_file, cmd->command[i+1]);
                remove_from_command(cmd, i); // remove > 
                remove_from_command(cmd, i); // remove output filename

                cmd->output_bool = 1;
            } else {
                printf("No output file provided!\n");
                fflush(stdout);
            }
        }
    }
}

void print_command (struct arr cmd) {
    for (int i = 0; i < cmd.arg_num && cmd.command[i] != NULL; i++) {
        printf("%s ", cmd.command[i]);
        fflush(stdout);
    }
    printf("\n");
    fflush(stdout);
}

void free_cmd(struct arr *cmd) {
    for (int i = 0; i < cmd->arg_num; i++) {
        if (cmd->command[i] != NULL) {
            free(cmd->command[i]);
            cmd->command[i] = NULL;
        }
    }
    cmd->arg_num = 0;
}

void exit_smallsh (struct arr* cmd) {
    cmd->built_in_command_flag = 1;

    if (cmd->arg_num == 1) {
        cmd->exit_flag = 1;
    } else {
        printf(EXTRA_ARGS);
        fflush(stdout);
    }
}

void change_dir(struct arr* cmd) {
    cmd->built_in_command_flag = 1;

    if (cmd->arg_num == 1) {
        chdir(getenv("HOME"));
        
    } else if (cmd->arg_num == 2) {
        if (chdir(cmd->command[1]) != 0) {
            perror("chdir");
        }
    } else {
        printf(EXTRA_ARGS);
        fflush(stdout);
    }
}

void display_status(int exit_status) {

    if (WIFEXITED(exit_status)) {
        printf("exit value %d\n", WEXITSTATUS(exit_status));
    } else if (WIFSIGNALED(exit_status)) {
        printf("terminated by signal %d\n", WTERMSIG(exit_status));
    }
    fflush(stdout);
    
}

void child_command(struct arr* cmd) {
    // Redirect input if specified
    if (cmd->input_bool) {
        int input_fd = open(cmd->input_file, O_RDONLY);
        if (input_fd == -1) {
            perror("open input file");
            fflush(stdout);
            exit(EXIT_FAILURE);
        }
        if (dup2(input_fd, STDIN_FILENO) == -1) {
            perror("dup2 input");
            fflush(stdout);
            exit(EXIT_FAILURE);
        }
        close(input_fd);
    }

    // Redirect output if specified
    if (cmd->output_bool) {
        int output_fd = open(cmd->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (output_fd == -1) {
            perror("open output file");
            fflush(stdout);
            exit(EXIT_FAILURE);
        }
        if (dup2(output_fd, STDOUT_FILENO) == -1) {
            perror("dup2 output");
            fflush(stdout);
            exit(EXIT_FAILURE);
        }
        close(output_fd);
    }

    // Execute the command
    execvp(cmd->command[0], cmd->command);

    // If execvp fails
    perror("execvp failed");
    fflush(stdout);
    exit(EXIT_FAILURE);
}


void parent_command(int flag, pid_t pid, int* exit_status) {

    if (flag == 0) { /* only wait for the foreground processes */
        waitpid(pid, exit_status, 0); /* wait for child process to finish */
    }
}

void sigchld_handler(int signum) {
    pid_t pid;
    int status;

    /* waitpid usage:
    If the WNOHANG bit is set in OPTIONS, and that child
    is not already dead, return (pid_t) 0. If successful,
    return PID and store the dead child's status in STAT_LOC. 
    */
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (!foreground_only_mode) {
            printf("background pid %d is done: ", pid);
            if (WIFEXITED(status)) {
                printf("exit value %d\n", WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                printf("terminated by signal %d\n", WTERMSIG(status));
            }
            fflush(stdout);
        }
    }
}

void sigint_handler(int signum) {
    if (foreground_pid != -1) {
        kill(foreground_pid, SIGINT);
    }

    fflush(stdout);
}

void sigtstp_handler(int signum) {
    if (foreground_only_mode) {
        foreground_only_mode = 0;
        printf("Exiting foreground-only mode\n");
    } else {
        foreground_only_mode = 1;
        printf("Entering foreground-only mode (& is now ignored)\n");
    }
    fflush(stdout);
}

void setup_signal_handlers() {
    
    struct sigaction sa_int = { .sa_handler = sigint_handler, .sa_flags = SA_RESTART };
    sigfillset(&sa_int.sa_mask);
    sigaction(SIGINT, &sa_int, NULL);

    struct sigaction sa_chld = { .sa_handler = sigchld_handler, .sa_flags = SA_RESTART };
    sigfillset(&sa_chld.sa_mask);
    sigaction(SIGCHLD, &sa_chld, NULL);

    struct sigaction sa_tstp = { .sa_handler = sigtstp_handler, .sa_flags = SA_RESTART };
    sigfillset(&sa_tstp.sa_mask);
    sigaction(SIGTSTP, &sa_tstp, NULL);
}


void replace_PID_SYMBOL(struct arr* cmd) {

    char pid_str[MAX_CHAR];
    sprintf(pid_str, "%d", getpid());
    int pid_len = strlen(pid_str);

    for (int i = 0; i < cmd->arg_num && cmd->command[i] != NULL; i++) {
        char* symbol = strstr(cmd->command[i], PID_SYMBOL);

        while (symbol != NULL) {
            memmove(symbol + strlen(pid_str), symbol + strlen(PID_SYMBOL), strlen(symbol) - strlen(PID_SYMBOL) + 1);
            memcpy(symbol, pid_str, pid_len);

            symbol = strstr(symbol + pid_len, PID_SYMBOL);
        }
    }
}

int built_in_commands(struct arr* cmd, int* exit_status) {
    if (strcmp(cmd->command[0], EXIT_KEY) == 0) {
        exit_smallsh(cmd);
        free_cmd(cmd);
        return 0;
    } else if (strcmp(cmd->command[0], CDIR_KEY) == 0) {
        change_dir(cmd);
    } else if (strcmp(cmd->command[0], STAT_KEY) == 0) {
        display_status(*exit_status);
        cmd->built_in_command_flag = 1;
    }

    return 1;
}

void run_smallsh(struct arr* cmd, char input[MAX_CHAR]) {
    
    int exit_status = 0;

    setup_signal_handlers();

    do {
        print_console(input);

        cmd->background_process_flag = 0;
        cmd->built_in_command_flag = 0;
        cmd->output_bool = 0;
        cmd->input_bool = 0;

        process_input(input, cmd);
        if (cmd->arg_num <= 0) {
            continue;
        }

        replace_PID_SYMBOL(cmd);

        if (cmd->command[0][0] == '#') {
            continue;
        }

        get_input_output_background(cmd);

        if (strcmp(cmd->command[cmd->arg_num-1], BACKGROUND_KEY) == 0) {
            cmd->background_process_flag = 1;
            remove_from_command(cmd, cmd->arg_num - 1);
        }

        if (!built_in_commands(cmd, &exit_status)) {
            break;
        }

        if (cmd->built_in_command_flag) {
            free_cmd(cmd);
            continue;
        }

        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            if (cmd->background_process_flag && !foreground_only_mode) {
                printf("background pid is %d\n", getpid());
                fflush(stdout);
            } else {
                signal(SIGINT, SIG_DFL);
            }
            child_command(cmd);
        } else {
            if (cmd->background_process_flag && !foreground_only_mode) {
                signal(SIGCHLD, sigchld_handler);
            } else {
                foreground_pid = pid;
                waitpid(pid, &exit_status, 0);
                foreground_pid = -1;
                /* display_status(exit_status); */
            }
        }

        free_cmd(cmd);
    } while (!cmd->exit_flag);
}

int main(void)
{
    char input[MAX_CHAR];

    struct arr cmd;

    cmd.arg_num = 0;
    cmd.exit_flag = 0;

    memset(&cmd, 0, sizeof(cmd));
    memset(input, 0, sizeof(input));

    run_smallsh(&cmd, input);

    return 0;
}