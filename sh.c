#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <pwd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include "sh.h"

//****************************************************************************************************************************
// CONSTANTS
# define BUFFERSIZE 512
# define BUILT_IN_COMMAND_COUNT 11
const char *BUILT_IN_COMMANDS[] = {"exit", "which", "where", "cd", "pwd", "list", "pid", "kill", "prompt", "printenv", "setenv"};

//****************************************************************************************************************************
// GLOBALS
char *prefix;
char buffer[BUFFERSIZE]; // Read commands from stdin


//****************************************************************************************************************************
int sh( int argc, char **argv, char **envp ) {
    char *prompt = calloc(PROMPTMAX, sizeof(char));
    char *commandline = calloc(MAX_CANON, sizeof(char));
    char *command, *arg, *commandpath, *p, *pwd, *owd;
    char **args = calloc(MAXARGS, sizeof(char*));
    int uid, i, status, argsct, go = 1;
    struct passwd *password_entry;
    char *homedir;
    struct pathelement *pathList;

    uid = getuid();
    password_entry = getpwuid(uid);      
    homedir = password_entry->pw_dir;	
      
    if ( (pwd = getcwd(NULL, PATH_MAX+1)) == NULL )
    {
      perror("getcwd");
      exit(2);
    }
    owd = calloc(strlen(pwd) + 1, sizeof(char));
    memcpy(owd, pwd, strlen(pwd));
    prompt[0] = ' '; prompt[1] = '\0';

    /* Put PATH into a linked list */
    pathList = get_path();



    // MY VARIABLES
    pid_t pid;
    char *commandList[BUFFERSIZE]; // Store the commands and flags typed


    /*
    Main Loop For Shell
    */
    while ( go ) {
        /* print your prompt */
        printShell();

        /* get command line and process */
        fgets(buffer, BUFFERSIZE, stdin);
        if (strlen(buffer) < 2) // Ignore empty stdin, ie. user presses enter with no input
            continue;
        if (buffer[strlen(buffer)-1] == '\n')
            buffer[strlen(buffer)-1] = 0; // Handle \n from fgets
            
        char *token = strtok (buffer, " ");
        for (int i = 0; token != NULL; i++) { // Parsing args
            commandList[i] = token;
            token = strtok (NULL, " ");
        }

        if (isBuiltIn(commandList[0])) { // Check to see if the command refers to an already built in command
        /* check for each built in command and implement */
            printf(" Executing built-in: %s\n", commandList[0]);
            runBuiltIn(commandList, pathList);
        } else { /* else find program to exec */
            /* find it */
            
            // TODO make the which function
            printf("Executing: PATHNAME HERE"); 
            /* do fork(), execve() and waitpid() */
            char *argv[] = { "ls", "-l", NULL };
            char *envp[] = {
                "HOME=/",
                "PATH=/bin:/usr/bin",
                "TZ=UTC0",
                "USER=william",
                "LOGNAME=william",
                NULL
            };
            if ((pid = fork()) < 0) { // Child
                perror("fork error");
            } else if (pid == 0) {
                execve("/bin/ls", argv, envp);
                printf("couldn't execute: %s", strerror(errno));
                exit(127);
            }
            // Parent
            if ((pid = waitpid(pid, &status, 0)) < 0) {
                perror("waitpid error");
            } 
            /*
            else {
                fprintf(stderr, "%s: Command not found.\n", args[0]);
            }
            */
            printShell();
            }
            memset(commandList, 0, sizeof(commandList)); // Reset the array of commands typed by user each loop
        }
        return 0;
    } /* sh() */




// HELPER FUNCTIONS
//***************************************************************************************************************

/**
 * isBuiltIn, determines whether the command given is apart of the built in 
 *            commands list. Returns 1 if true, 0 otherwise.
 * 
 * Consumes: A string
 * Produces: An integer
 */
int isBuiltIn(char *command) {
    int inList = 0; // False
    for (int i = 0; i < BUILT_IN_COMMAND_COUNT; i++) {
        if (strcmp(command, BUILT_IN_COMMANDS[i]) == 0) { // strcmp returns 0 if true
            inList = 1; // True
            break;
        }
    }
    return inList;
}


/**
 * runBuiltIn, runs the built in commands
 * 
 * Consumes: A string
 * Produces: Nothing
 */
void runBuiltIn(char *commandList[], struct pathelement *pathList) { // commandList[0] will always be the command itself
    if (strcmp(commandList[0], "exit") == 0) { // strcmp returns 0 if true
        exitProgram(); // Exit the program and thus the shell
    } else if (strcmp(commandList[0], "which") == 0) {
        printf("Not yet implemented\n");
    } else if (strcmp(commandList[0], "where") == 0) {
        printf("Not yet implemented\n");
    } else if (strcmp(commandList[0], "cd") == 0) {
        printf("Not yet implemented\n");
    } else if (strcmp(commandList[0], "pwd") == 0) {
        printWorkingDirectory(); // Print Working Directory
    } else if (strcmp(commandList[0], "list") == 0) {
        listHandler(commandList);
    } else if (strcmp(commandList[0], "pid") == 0) {
        printPid();
    } else if (strcmp(commandList[0], "kill") == 0) {
        printf("Not yet implemented\n");
    } else if (strcmp(commandList[0], "prompt") == 0) {
        prompt(commandList[1]); // Prompt takes one arg
    } else if (strcmp(commandList[0], "printenv") == 0) {
        printf("Not yet implemented\n");
    } else if (strcmp(commandList[0], "setenv") == 0) {
        printf("Not yet implemented\n");
    }
}


// BUILT IN COMMAND FUNCTIONS
//*******************************************************************************************************************

/**
 * printPid, prints the pid of the shell.
 * 
 * Consumes: Nothing
 * Produces: Nothing
 */
void printPid() {
    int pid = getpid();
    printf(" pid: %d\n", pid);
}


/**
 * exitProgram, exits the program, but frees all allocs beforehand.
 * 
 * Consumes: Nothing
 * Produces: Nothing
 */
void exitProgram() {
    free(prefix);
    exit(0); // Exit without error
}


/**
 * printWorkingDirectory, just prints the absolute path to the current working
 *                        directory, if only all of these functions were that
 *                        simple.
 * 
 * Consumes: Nothing
 * Produces: Nothing
 */
void printWorkingDirectory() {
    char *ptr = getcwd(NULL, 0);
    printf(" %s\n", ptr);
    free(ptr); 
}


/**
 * prompt, puts a new prefix string at the beginning of the shell. If this
 *         function is called with no argument, it prompts the user to enter
 *         a string with which to prefix the shell with.
 * 
 * Consumes: A string
 * Produces: Nothing
 */
void prompt(char *str) {
    if (str != NULL) {
        prefix = (char *)malloc(strlen(str));
        strcpy(prefix, str); // Remember prefix has a global scope
    } else {
        printf(" input prompt prefix: ");
        fgets(buffer, BUFFERSIZE, stdin);
        buffer[strlen(buffer)-1] = '\0';
        prefix = (char *)malloc(strlen(buffer));
        strcpy(prefix, buffer); // Copy buffer to prefix
    }
}


/**
 * which, locates commands. Returns the location of the command.
 * 
 * Consumes: A string, A list of strings
 * Produces: A string
 */
char *which(char *command, struct pathelement *pathlist ) {
    /* loop through pathlist until finding command and return it.  Return
    NULL when not found. */
    while (pathlist) {         // WHICH
      sprintf(command, "%s/gcc", pathlist->element);
      if (access(command, X_OK) == 0) {
        printf("[%s]\n", command);
        break;
      }
    pathlist = pathlist->next;
    }
    return pathlist->element;
} /* which() */


char *where(char *command, struct pathelement *pathlist )
{
  /* similarly loop through finding all locations of command */
} /* where() */


/**
 * list, acts as the ls command, with no arguments, lists all the files in the
 *       current working directory, with arguments lists the files contained in
 *       the arguments given (directories).
 * 
 * Consumes: A string (directory name)
 * Produces: Nothing
 */
void list (char *dir) {
  /* see man page for opendir() and readdir() and print out filenames for
  the directory passed */
  DIR *dp;
  struct dirent *dirp;
  if (strcmp(dir, "") == 0) { // Case where no arguments are given
      char *cwd = getcwd(NULL, 0); // Only prints cwd
      if ((dp = opendir(cwd)) == NULL) {
          errno = ENOENT;
          perror("No cwd: ");
      }
      while ((dirp = readdir(dp)) != NULL)
          printf(" %s\n", dirp->d_name);
      free(cwd);
  } else { // Case where list is called with arguments
      if ((dp = opendir(dir)) == NULL) {
          errno = ENOENT;
          printf(" list: cannot access %s: %s\n", dir, strerror(errno));
          return; // If directory doesn't exist exit this function.
      }
      printf(" %s:\n", dir);
      while ((dirp = readdir(dp)) != NULL) {
          printf("    %s\n", dirp->d_name);
      }
  }


} /* list() */


// CONVIENIENCE FUNCTIONS
//*******************************************************************************************************

/**
 * printShell, prints the cwd in the form [path]>
 * 
 * Consumes: Nothing 
 * Produces: Nothing
 */
void printShell() {
  char *ptr = getcwd(NULL, 0);
  if (prefix != NULL)
    printf("%s [%s]>", prefix, ptr); // prefix is global in scope
  else
    printf(" [%s]>", ptr);
  free(ptr); 
}


/**
 * listHandler, handles the logic for the list function. Checks if called
 *              with no arguments, or with arguments and calls list accordingly.
 * 
 * Consumes: An array of strings
 * Produces: Nothing
 */
void listHandler(char *commandList[]) {
    if (commandList[1] == NULL) {
            list("");
    } else {
        for (int i = 1; commandList[i] != NULL; i++)  // i = 1 because commandList at index 1 and is the list command
            list(commandList[i]); 
    }
}