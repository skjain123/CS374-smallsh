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

#define EXIT_KEY        "exit"
#define CDIR_KEY        "cd"
#define STAT_KEY        "status"

#define LIST_KEY        "ls"

#define MAX_CHAR        2048
#define MAX_ARG         512

#define EXTRA_ARGS      "Too many Arguments!\n"

#define INPUT_KEY       "<"
#define OUTPUT_KEY      ">"
#define BACKGROUND_KEY  "&"

#define PID_SYMBOL      "$$"

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

/* void list(char* cwd) {
    DIR *root;
    struct dirent *entry;

    root = opendir(cwd);
    
    if (root == NULL)
    {
        printf("Error opening directory.\n");
        fflush(stderr);
        return;
    }

    while ((entry = readdir(root)) != NULL)
    {
        printf("%s  ", entry->d_name);
        fflush(stderr);
    }
    printf("\n");
    fflush(stderr);
    
    if (closedir(root) == -1)
    {
        printf("Error closing directory.\n");
        fflush(stderr);
        return;
    }

} */

void print_console (char input[MAX_CHAR], char cwd[1024]) {
    char userInput[MAX_CHAR];

    /* get user input */
    /* printf("smallsh ~%s : ", cwd); */
    printf(": ");
    fflush(stderr);

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
    printf("removing : %s\n", cmd->command[index]);

    free(cmd->command[index]);

    for (int i = index; i < cmd->arg_num; i++) {
        cmd->command[i] = cmd->command[i+1];
    }
    cmd->arg_num--;
}

void get_input_output_background(struct arr* cmd) {

    cmd->input_bool = 0;
    cmd->output_bool = 0;

    for (int i = 0; i < cmd->arg_num && cmd->command[i] != NULL; i++) {
        if (strcmp(cmd->command[i], INPUT_KEY) == 0) {
            if (cmd->command[i+1]) {
                strcpy(cmd->input_file, cmd->command[i+1]);
                remove_from_command(cmd, i);
                remove_from_command(cmd, i+1);

                cmd->input_bool = 1;
            } else {
                printf("No input file provided!\n");
                fflush(stderr);
            }
            
        } else if (strcmp(cmd->command[i], OUTPUT_KEY) == 0) {
            if (cmd->command[i+1]) {
                strcpy(cmd->output_file, cmd->command[i+1]);
                remove_from_command(cmd, i);
                remove_from_command(cmd, i+1);

                cmd->output_bool = 1;
            } else {
                printf("No output file provided!\n");
                fflush(stderr);
            }
        }
    }
}

void print_command (struct arr cmd) {
    for (int i = 0; i < cmd.arg_num && cmd.command[i] != NULL; i++) {
        printf("%s ", cmd.command[i]);
        fflush(stderr);
    }
    printf("\n");
    fflush(stderr);
}

void free_cmd(struct arr *cmd) {
    for (int i = 0; i < MAX_ARG && cmd->command[i] != NULL; i++) {
        free(cmd->command[i]);
    }
}

void exit_smallsh (struct arr* cmd) {
    cmd->built_in_command_flag = 1;

    if (cmd->arg_num == 1) {
        cmd->exit_flag = 1;
    } else {
        printf(EXTRA_ARGS);
        fflush(stderr);
    }
}

void change_dir(struct arr* cmd) {
    cmd->built_in_command_flag = 1;

    if (cmd->arg_num == 1) {
        chdir(getenv("HOME"));
        
    } else if (cmd->arg_num == 2) {
        chdir(cmd->command[1]);
    } else {
        printf(EXTRA_ARGS);
    }

    fflush(stderr);

}

void display_status(struct arr* cmd, int exit_status) {

    if (exit_status != 0) {
        printf("%d\n", exit_status);
        fflush(stderr);
    } else {
        printf("%d\n", 0);
        fflush(stderr);
    }
    cmd->built_in_command_flag = 1;

    printf("print status\n");
    fflush(stderr);
}

void child_command(struct arr* cmd, int output_fd) {
    
    // Redirect the stdout of the child process to output_fd

    if (cmd->output_bool == 1) {
        if (dup2(output_fd, STDOUT_FILENO) == -1) {
            perror("dup2 failed");
            exit(EXIT_FAILURE);
        }

        // Close the original output_fd as it's no longer needed
        close(output_fd);
    }

    // Execute the command
    execvp(cmd->command[0], cmd->command);

    if (cmd->background_process_flag == 1) {
        printf("Background process with PID %d terminated.\n", getpid());
        fflush(stdout);
    }

    // If execvp fails, it will reach here
    perror("execvp failed");
    exit(EXIT_FAILURE);
}

void parent_command(int flag, pid_t pid, int* exit_status) {
    if (flag == 0) { /* only wait for the foreround processes */
        int status;
        waitpid(pid, &status, 0); /* wait for child process to finish */
        *exit_status = WEXITSTATUS(status);
        /* printf("Foreground process with PID %d has terminated.\n", pid); */
        fflush(stderr);
        return;
    } else {
        /* printf("Background process with PID %d terminated.\n", pid); */
        fflush(stderr);
        return;
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
        printf("Child process with PID %d terminated\n", pid);
    }
}

void replace_PID_SYMBOL(struct arr* cmd) {

    char pid_str[MAX_CHAR];
    sprintf(pid_str, "%d", getpid());

    int pid_len = strlen(pid_str);

    for (int i = 0; i < cmd->arg_num && cmd->command[i] != NULL; i++) {
        char* symbol = strstr(cmd->command[i], PID_SYMBOL);

        while (symbol != NULL) {
            /* int index = symbol - cmd->command[i]; */

            memmove(symbol + strlen(pid_str), symbol + strlen(PID_SYMBOL), strlen(symbol) - strlen(PID_SYMBOL) + 1);
            memcpy(symbol, pid_str, pid_len);

            symbol = strstr(symbol + pid_len, PID_SYMBOL);
        }
    }
}


int main(void)
{
    
    char cwd[MAX_CHAR]; /* buffer to hold current directory path */
    char input[MAX_CHAR];
    int exit_status = 0;

    struct arr cmd;
    cmd.arg_num = 0;
    cmd.exit_flag = 0;

    getcwd(cwd, sizeof(cwd));

    /* print_console(input, cwd); */

    do {
        print_console(input, cwd);

        cmd.background_process_flag = 0;
        cmd.built_in_command_flag = 0;
        cmd.output_bool = 0;
        cmd.input_bool = 0;

        /* parse the input into the command variable, an array of strings (char*) */
        process_input(input, &cmd);

        /* check for proper amount of arguements */
        if (cmd.arg_num <= 0) {
            continue;
        }

        /* check for comments */
        if (cmd.command[0][0] && cmd.command[0][0] == '#') {
            continue;
        }


        replace_PID_SYMBOL(&cmd);
        


        if (strcmp(cmd.command[cmd.arg_num - 1], BACKGROUND_KEY) == 0) {
            fflush(stderr);
            cmd.background_process_flag = 1;
            remove_from_command(&cmd, cmd.arg_num - 1);
        }

        /* sets the input and output file if the special characters exist */
        get_input_output_background(&cmd);

        print_command(cmd);
        

        if (strcmp(cmd.command[0], EXIT_KEY) == 0) {
            exit_smallsh(&cmd);
            free_cmd(&cmd);
            break;
        } else if (strcmp(cmd.command[0], CDIR_KEY) == 0) {
            change_dir(&cmd);
        } else if (strcmp(cmd.command[0], STAT_KEY) == 0) {
            display_status(&cmd, exit_status);
        }

        if (cmd.built_in_command_flag == 1) {
            free_cmd(&cmd);
            continue;
        }

        signal(SIGCHLD, sigchld_handler);

        pid_t pid = fork();

        if (pid < 0) {
            printf("fork unsuccessful\n");
            fflush(stderr);
            exit(EXIT_FAILURE);
        } else if (pid == 0) {

            int output_fd = stdout;
            printf("sdpojf\n");
            
            if (cmd.background_process_flag == 0) {
                if (cmd.output_bool == 1) {
                    FILE* out = fopen(cmd.output_file, "w");
                    /* if (out == NULL) {
                        printf("file not found sunil\n");
                    } */

                    fclose(out);

                    output_fd = open(cmd.output_file, O_WRONLY | O_CREAT | O_APPEND, 0666);
                    if (output_fd == -1) {
                        perror("open failed");
                        exit(EXIT_FAILURE);
                    }
                }

                child_command(&cmd, output_fd);

                close(output_fd);

            } else {

                int original_stdout = dup(STDOUT_FILENO);  // Save current stdout for later restoration
                int output_fd = open("/dev/null", O_WRONLY);  // Open /dev/null for redirection

                /* Redirect stdout to /dev/null */
                if (dup2(output_fd, STDOUT_FILENO) == -1) {
                    perror("dup2 failed");
                    exit(EXIT_FAILURE);
                }

                // Close fd for /dev/null
                close(output_fd);

                /* Execute command */
                child_command(&cmd, original_stdout);

                /* Restore stdout */
                dup2(original_stdout, STDOUT_FILENO);
                close(original_stdout);

                printf("Background process with PID %d has finished.\n", getpid());
                fflush(stdout);

                exit(EXIT_SUCCESS);
            }
        } else {
            parent_command(cmd.background_process_flag, pid, &exit_status);
        }

        free_cmd(&cmd);
    } while (cmd.exit_flag == 0);
    // im doing status command rn :333333333333333333333333333333
    return 0;
}