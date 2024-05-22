/**
 * Name: Sunil Jain
 * Student ID: 934059526
 * Oregon State University
 * Operating Systems 1
 * Spring 2024
 * Professor Chaudhrn
 * 
 * Description: This program creates a bash shell called "smallsh"
 * It has user functionality and 3 built-in commands.
*/

#include "main.h"

// for handling the foreground only modes through the parent-child processes
volatile sig_atomic_t foreground_only_mode = 0;
volatile pid_t foreground_pid = -1;

int main(void)
{
    char input[MAX_CHAR];

    struct curr_input cmd;

    cmd.arg_num = 0;
    cmd.exit_flag = 0;

    run_smallsh(&cmd, input);

    return 0;
}

void run_smallsh(struct curr_input* cmd, char input[MAX_CHAR]) {
    
    int exit_status = 0;

    // setup signal handles
    setup_signal_handlers();

    do {
        // display the ":" and get user input
        print_console(input);

        // reset the curr_input structure's variables
        cmd->background_process_flag = 0;
        cmd->built_in_command_flag = 0;
        cmd->output_bool = 0;
        cmd->input_bool = 0;

        // parse the user input and insert it into the curr_input's command
        process_input(input, cmd);

        // if the user gave nothing, then continue
        if (cmd->arg_num <= 0) {
            continue;
        }

        // if the user inputted something, check for "$$" to replace with PID
        replace_PID_SYMBOL(cmd);

        // if the first argument is a "#", it is a comment, ignore and continue
        if (cmd->command[0][0] == '#') {
            continue;
        }

        // if it is an actual command, see if it has input/ouput KEYS
        get_input_output_background(cmd);

        // if the last argument is the BACKGROUND_KEY, run the command as a 
        // background process
        if (strcmp(cmd->command[cmd->arg_num-1], BACKGROUND_KEY) == 0) {
            cmd->background_process_flag = 1;
            remove_from_command(cmd, cmd->arg_num - 1);
        }

        // check if the user wants to exit, if so, then break the loop
        if (built_in_commands(cmd, &exit_status) == 0) {
            break;
        }

        // check if the built in flag is 1, if it is, dont fork()
        if (cmd->built_in_command_flag == 1) {
            free_cmd(cmd);
            continue;
        }

        // here the command is not a built in function, so fork()
        pid_t pid = fork();

        if (pid == -1) { // unsuccessful fork
            perror("fork");
            exit(EXIT_FAILURE);
        } else if (pid == 0) { // successful fork
            // check whether to run as a background/foreground process
            if (cmd->background_process_flag && !foreground_only_mode) {
                printf("background pid is %d\n", getpid());
                fflush(stdout);
            } else {
                // for the child proc, treat CTRL+C as normal
                signal(SIGINT, SIG_DFL); 
            }
            // run the command from the user
            child_command(cmd);
        } else {
            // parent is here: 
            if (cmd->background_process_flag && !foreground_only_mode) {
                // make sure that when the child ends, it runs sigcld_handler
                //this is a background process
                signal(SIGCHLD, sigchld_handler);
            } else {
                // this is a foreground process, wait for it
                foreground_pid = pid;
                waitpid(pid, &exit_status, 0);
                foreground_pid = -1;
            }
        }

        // at the end of the loop, free and reset the cmd->command
        free_cmd(cmd);
    } while (!cmd->exit_flag); // while the user doesnt exit, run
}

void print_console (char input[MAX_CHAR]) {
    char userInput[MAX_CHAR];

    // display the ":"
    printf(": ");
    fflush(stdout);

    /* get user input */
    fgets(userInput, MAX_CHAR, stdin);
    if (strlen(userInput) > 0 && userInput[strlen(userInput) - 1] == '\n') {
        userInput[strlen(userInput) - 1] = '\0';
    }

    // return the user input
    strcpy(input, userInput);
}

void process_input(char input[MAX_CHAR], struct curr_input *cmd) {
    char* token;

    // reset arg_num
    cmd->arg_num = 0;

    // parse by token
    token = strtok(input, " ");

    // go through user input and split it into arguments
    while (token != NULL && cmd->arg_num < MAX_ARG - 1) {

        // allocate the memory for the current argument
        cmd->command[cmd->arg_num] = malloc(strlen(token) + 1);  /* Allocate memory for the string */

        // if the malloc failed report it
        if (cmd->command[cmd->arg_num] == NULL) {
            perror("malloc failed\n");
            exit(EXIT_FAILURE);
        }

        /* check for inline comments in the input, if there are do not parse any further */
        if (strcmp(token,"//") == 0) {
            break;
        }

        // Copy the token into the allocated memory
        strcpy(cmd->command[cmd->arg_num], token); 

        // go to the next one 
        token = strtok(NULL, " ");

        // increase the amount of arguments
        cmd->arg_num++;
    }

    // set the last arg num to NULL for execvp
    cmd->command[cmd->arg_num] = NULL;
}

void remove_from_command (struct curr_input* cmd, int index) {
    
    // check if the argument to remove is valid
    if (cmd->command[index] != NULL) {
        
        // free that memory
        free(cmd->command[index]);

        // remove the argument
        for (int i = index; i < cmd->arg_num - 1; i++) {
            cmd->command[i] = cmd->command[i+1];
        } 

        // set the last one as NULL for execvp
        cmd->command[cmd->arg_num - 1] = NULL;
        
        // reduce the amount of arguments
        cmd->arg_num--;
    }
    
}

void get_input_output_background(struct curr_input* cmd) {

    // reset these bools
    cmd->input_bool = 0;
    cmd->output_bool = 0;

    // go through the arguments
    for (int i = 0; i < cmd->arg_num && cmd->command[i] != NULL; i++) {
        
        // check for the input key
        if (strcmp(cmd->command[i], INPUT_KEY) == 0) {
            // check if theres a filename next
            if (i + 1 < cmd->arg_num) {
                
                // get the file name
                strcpy(cmd->input_file, cmd->command[i+1]); 
                remove_from_command(cmd, i); // remove <
                remove_from_command(cmd, i); // remove input filename

                cmd->input_bool = 1; // we found an input file
                i -= 2; // for loop offset since we got rid of two args
            } else { // if the user did not provide a filename, error
                printf("No input file provided!\n");
                fflush(stdout);
            }
            
        // check for the output key
        } else if (strcmp(cmd->command[i], OUTPUT_KEY) == 0) {
            
            // check if theres a file name next
            if (i + 1 < cmd->arg_num) {

                // get the file name
                strcpy(cmd->output_file, cmd->command[i+1]);
                remove_from_command(cmd, i); // remove > 
                remove_from_command(cmd, i); // remove output filename

                cmd->output_bool = 1; // we found an input file
                i -= 2; // for loop offset since we got rid of two args
            } else { // if the user did not provide a filename, error
                printf("No output file provided!\n");
                fflush(stdout);
            }
        }
    }
}

void replace_PID_SYMBOL(struct curr_input* cmd) {

    // get pid string into a variable, and get its length
    char pid_str[MAX_CHAR];
    sprintf(pid_str, "%d", getpid());
    int pid_len = strlen(pid_str);

    // go through all my arguments
    for (int i = 0; i < cmd->arg_num && cmd->command[i] != NULL; i++) {
        // get the first instance of the pid symbol in the argument
        char* symbol = strstr(cmd->command[i], PID_SYMBOL);

        // if the symbol is found, replace it with the pid
        while (symbol != NULL) {
            /* expand the memory block of the first instance of $$
             to fit the pid (use appropriate offests) */
            memmove(symbol + pid_len, symbol + strlen(PID_SYMBOL), 
            strlen(symbol) - strlen(PID_SYMBOL) + 1);

            /* copy the pid into that expanded memory */
            memcpy(symbol, pid_str, pid_len);

            /* coninually check for more $$ in the argument */
            symbol = strstr(symbol + pid_len, PID_SYMBOL);
        }
    }
}

void print_command (struct curr_input cmd) {
    // go through all arguments
    for (int i = 0; i < cmd.arg_num && cmd.command[i] != NULL; i++) {

        // print the argument
        printf("%s ", cmd.command[i]);
        fflush(stdout);
    }
    printf("\n");
    fflush(stdout);
}

void free_cmd(struct curr_input *cmd) {
    // go through the arguments
    for (int i = 0; i < cmd->arg_num; i++) {
        if (cmd->command[i] != NULL) { 
            free(cmd->command[i]); // free argument
            cmd->command[i] = NULL;
        }
    }
    cmd->arg_num = 0; // set the amount of arguments to 0
}

int built_in_commands(struct curr_input* cmd, int* exit_status) {
    
    // check if the first argument is the exit key
    if (strcmp(cmd->command[0], EXIT_KEY) == 0) {
        exit_smallsh(cmd); // change the exit_flag
        free_cmd(cmd); // free the command
        return 0; // exit process with 0

    // check if the first argument is the change dir key
    } else if (strcmp(cmd->command[0], CDIR_KEY) == 0) {
        change_dir(cmd); // change directory

    // check if the first argument is the status key
    } else if (strcmp(cmd->command[0], STAT_KEY) == 0) {
        display_status(*exit_status, cmd); // display the status
    }

    return 1;
}

void exit_smallsh (struct curr_input* cmd) {
    // signal that we ran a built in flag
    cmd->built_in_command_flag = 1;

    // only set the exit flag to 1 if theres no extra arguments
    if (cmd->arg_num == 1) {
        cmd->exit_flag = 1;
    } else { // print an error because there is too many arguments
        printf(EXTRA_ARGS);
        fflush(stdout);
    }
}

void change_dir(struct curr_input* cmd) {
    // signal that we ran a built in flag
    cmd->built_in_command_flag = 1;

    // if thers only "cd" then change dir to the home directory
    if (cmd->arg_num == 1) {
        chdir(getenv("HOME"));

    // if theres a second argument, change dir to that path
    } else if (cmd->arg_num == 2) {
        if (chdir(cmd->command[1]) != 0) {
            perror("chdir"); // print error if invalid path
        }
    // too many arguments for cd 
    } else {
        printf(EXTRA_ARGS);
        fflush(stdout);
    }
}

void display_status(int exit_status, struct curr_input* cmd) {
    // signal that we ran a built in flag   
    cmd->built_in_command_flag = 1;

    // if the child exited print the exit value
    if (WIFEXITED(exit_status)) {
        printf("exit value %d\n", WEXITSTATUS(exit_status));

    // if the child is terminated by a signal print it 
    } else if (WIFSIGNALED(exit_status)) {
        printf("terminated by signal %d\n", WTERMSIG(exit_status));
    }
    fflush(stdout);
    
}

void setup_signal_handlers() {
    
    // set the sigint handler and mask normal functions
    struct sigaction sa_int = { .sa_handler = sigint_handler, .sa_flags = SA_RESTART };
    sigfillset(&sa_int.sa_mask);
    sigaction(SIGINT, &sa_int, NULL);

    // set the sigchld handler and mask normal functions
    struct sigaction sa_chld = { .sa_handler = sigchld_handler, .sa_flags = SA_RESTART };
    sigfillset(&sa_chld.sa_mask);
    sigaction(SIGCHLD, &sa_chld, NULL);

    // set the sigstp handler and mask normal functions 
    struct sigaction sa_tstp = { .sa_handler = sigtstp_handler, .sa_flags = SA_RESTART };
    sigfillset(&sa_tstp.sa_mask);
    sigaction(SIGTSTP, &sa_tstp, NULL);
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
        printf("background pid %d is done: ", pid);
        if (WIFEXITED(status)) {
            printf("exit value %d\n", WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("terminated by signal %d\n", WTERMSIG(status));
        }
        fflush(stdout);
    }
}

void sigint_handler(int signum) {
    // kill specified process with specified signal
    if (foreground_pid != -1) { 
        kill(foreground_pid, SIGINT);
    }
}

void sigtstp_handler(int signum) {
    // flip-flop the foreground_only_mode and print accordingly
    if (foreground_only_mode) {
        foreground_only_mode = 0;
        printf("Exiting foreground-only mode\n");
    } else {
        foreground_only_mode = 1;
        printf("Entering foreground-only mode (& is now ignored)\n");
    }
    fflush(stdout);
}

void child_command(struct curr_input* cmd) {
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