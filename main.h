#ifndef MAIN
#define MAIN

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

// something to do with the sigactions, I got this from a docs page
#define _XOPEN_SOURCE 700

// built-in functions to be run when these keys are found as the first argument
#define EXIT_KEY        "exit"
#define CDIR_KEY        "cd"
#define STAT_KEY        "status"

// defining the max characters in an argument and the max arguments
#define MAX_CHAR        2048
#define MAX_ARG         512

// defining an error message for built in functions
#define EXTRA_ARGS      "Too many Arguments!\n"

// defining keys for input/output file declarations and background process
// declaraction
#define INPUT_KEY       "<"
#define OUTPUT_KEY      ">"
#define BACKGROUND_KEY  "&"

// defining what symbol we need to replace with the PID
#define PID_SYMBOL      "$$"

struct curr_input {
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


/**
 * the "main" function of the program
 * input: the curr_input structure, the input str from the user
 * purpose: run until the user inputs "exit" with no arguments
 * continually get the user input, parse it, check for keys, 
 * run that user input using fork() as either a background or foreground
 * process. Reset values at the end and go again.
*/
void run_smallsh(struct curr_input* cmd, char input[MAX_CHAR]);

// getting the input from the user and parsing that input

/**
 * print out the ":" and prompt for user input
 * input: a string named input
 * output: an edited "input" string
*/
void print_console (char input[MAX_CHAR]);

/**
 * parse the input string by spaces.
 * ignore anything after '//'
 * input: a string of user input
 * output: a parsed command by string (char* command[MAX_ARGS])
*/
void process_input(char input[MAX_CHAR], struct curr_input *cmd);

// search for KEYS and remove them from the command

/**
 * removes a argument from a char* command[MAX_ARGS]
 * input: an index to remove
 * output: an edited char* command[MAX_ARGS] without the specified arg
*/
void remove_from_command (struct curr_input* cmd, int index);

/**
 * goes through all arguments and looks for KEYS
 * if the input key appears: the next arg is the input filename
 * if the output key appears: the next arg is the output filename
 * if the background key appears: run the command as a background process
 * 
 * for all cases, remove the KEY and the filename if applicable from the 
 * char* command[MAX_ARGS]
 * 
 * input: curr_input structure with a filled command
 * output: fills the curr_input's input/output/flag variables
*/
void get_input_output_background(struct curr_input* cmd);



// input helper functions: 

/**
 * replace $$ with the PID of smallsh itself
 * input: curr_input structure with filled char* command[MAX_ARGS]
 * output: a char* command[MAX_ARGS] 
*/ 
void replace_PID_SYMBOL(struct curr_input* cmd);

/**
 * print the char* command[MAX_ARGS] of the curr_input
 * input: curr_input
 * output: none
*/
void print_command (struct curr_input cmd);

/**
 * free the commands after every loop iteration
 * frees the char* command[MAX_ARGS] of the curr_input
 * input: curr_input structure
 * output: none
*/

void free_cmd(struct curr_input *cmd);


// built-in functions

/**
 * checks for the built-in function KEYS as the first command
 * if so, run the appropriate function
 * input: curr_input
 * output: exit_status (1-exit the program, 0-continue)
*/
int built_in_commands(struct curr_input* cmd, int* exit_status);

/**
 * exits the program and frees all memory.
 * input: curr_input structure
 * output: exit status
*/
void exit_smallsh (struct curr_input* cmd);

/**
 * change smallsh's directory to the specified path
 * expects 2 arguments cd PATH
 * input: curr_input structure
 * output: appropriate errors, a changed directory
*/
void change_dir(struct curr_input* cmd);

/**
 * display the exit status of the last running process
 * input: integer exit status, curr_input structure
 * output: exit status message of the last PID's exit status
*/
void display_status(int exit_status, struct curr_input* cmd);

// signal handling

/**
 * sets up the signal handlers using sigaction
 * masks signals to prevent stopping the 
 * input: none
 * output: edited signal handlers
*/
void setup_signal_handlers();

/* this runs when the child process ends */
void sigchld_handler(int signum);

/* this runs when CTRL+C is pressed */
void sigint_handler(int signum);

/* this runs when CTRL+Z is pressed */
void sigtstp_handler(int signum);

// parent and child process comamnds

/**
 * the child process from fork() runs this
 * if there are input/output files, read/write from/to them
 * run the command in the child using execvp()
 * output an error message if it fails
 * 
 * input: curr_input structure
 * output: whatever the user comamnd runs/does
*/
void child_command(struct curr_input* cmd);

#endif