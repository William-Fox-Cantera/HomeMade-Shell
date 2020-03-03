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

/* TODO: 
    - Make it so the command line is parsed such that double quotes are seen as one argument
    - Update the path linked list if the PATH environment variable is updated with setenv
    - Handle the signals and stuff like SIGINT ctrl+c ctrl+d ctrl+z
*/

//****************************************************************************************************************************
// GLOBALS
const char *BUILT_IN_COMMANDS[] = {"exit", "which", "where", "cd", "pwd", "list", "pid", "kill", "prompt", "printenv", "setenv"};
char *prefix; // String that precedes prompt when using the prompt function
char *previousWorkingDirectory; // System for going back to previous directory with "cd -"


//****************************************************************************************************************************
int sh( int argc, char **argv, char **envp ) {
    /*
    char *prompt = calloc(PROMPTMAX, sizeof(char));
    char *commandline = calloc(MAX_CANON, sizeof(char));
    char *command, *arg, *commandpath, *p, *pwd, *owd;
    char **args = calloc(MAXARGS, sizeof(char*));
    int uid, i, status, argsct, go = 1;
    struct passwd *password_entry;
    char *homedir;

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
    */

    // MY VARIABLES
    int status;
    int go = 1; // Runs main loop
    char buffer[BUFFERSIZE]; // Read commands from stdin
    pid_t pid;
    char *commandList[BUFFERSIZE]; // Store the commands and flags typed by the user
    char *currentWorkingDirectory = getcwd(NULL, 0); // Initialize to avoid segmentation fault
    /* Put PATH into a linked list */
    struct pathelement *pathList = get_path(); 


    /*
    Main Loop For Shell
    */
    while (go) {
        /* Keep track of the previous and current working directories */
        if (strcmp(currentWorkingDirectory, getcwd(NULL, 0)) != 0)
            previousWorkingDirectory = currentWorkingDirectory;
        currentWorkingDirectory = getcwd(NULL, 0);

        /* print your prompt */
        printShell();

        /* get command line and process */
        fgets(buffer, BUFFERSIZE, stdin);
        if (strlen(buffer) < 2) // Ignore empty stdin, ie. user presses enter with no input
            continue;
        if (buffer[strlen(buffer)-1] == '\n')
            buffer[strlen(buffer)-1] = 0; // Handle \n from fgets
        // Main Argumment Parser
        char *token = strtok (buffer, " "); 
        for (int i = 0; token != NULL; i++) { 
            commandList[i] = token;
            token = strtok (NULL, " "); // Get ris of "-" used in flags for arguments
        }

        if (isBuiltIn(commandList[0])) { // Check to see if the command refers to an already built in command
        /* check for each built in command and implement */
            printf(" Executing built-in: %s\n", commandList[0]);
            runBuiltIn(commandList, pathList, envp);
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
 * runBuiltIn, runs the built in commands, if a command like list is called with multiple
 *             arguments, its handler function is called, calling the command for each
 *             argument given.
 * 
 * Consumes: A string
 * Produces: Nothing
 */
void runBuiltIn(char *commandList[], struct pathelement *pathList, char **envp) { // commandList[0] will always be the command itself
    if (strcmp(commandList[0], "exit") == 0) { // strcmp returns 0 if true
        exitProgram(); // Exit the program and thus the shell
    } else if (strcmp(commandList[0], "which") == 0) {
        whichHandler(commandList, pathList);
    } else if (strcmp(commandList[0], "where") == 0) {
        whereHandler(commandList, pathList);
    } else if (strcmp(commandList[0], "cd") == 0) {
        changeDirectory(commandList);
    } else if (strcmp(commandList[0], "pwd") == 0) {
        printWorkingDirectory(); // Print Working Directory
    } else if (strcmp(commandList[0], "list") == 0) {
        listHandler(commandList);
    } else if (strcmp(commandList[0], "pid") == 0) {
        printPid();
    } else if (strcmp(commandList[0], "kill") == 0) {
        killIt(commandList);
    } else if (strcmp(commandList[0], "prompt") == 0) {
        prompt(commandList); // Prompt takes one arg
    } else if (strcmp(commandList[0], "printenv") == 0) {
        printEnvironment(commandList, envp);
    } else if (strcmp(commandList[0], "setenv") == 0) {
        setEnvironment(commandList, envp);
    }
}


// BUILT IN COMMAND FUNCTIONS
//*******************************************************************************************************************

/**
 * killIt, when given just a pid, sends SIGTERM to it to politely kill that process.
 *         if given a signal number (ie. kill -9 1234), sends that signal to the 
 *         process.
 * 
 * Consumes: A list of strings
 * Produces: Nothing
 */
void killIt(char **commandList) {
    int pid;
    if (commandList[1] == NULL) { // Case where no argument is given
        fprintf(stderr, "%s", " kill: Specify at least one argument\n");
    } else if (commandList[2] != NULL) { // Case where a flag such as "-9" is used 
        int signal;
        if ((signal = atoi(commandList[1])) == 0) { // If the signal number is invalid
            errno = EINVAL;
            perror(" kill, bad signal");
        } else if ((pid = atoi(commandList[2])) == 0) { // If the pid is invalid
            errno = EINVAL;
            perror(" kill, no pid");
        } else {
            if ((kill(pid, signal)) != 0) {
                errno = ESRCH;
                perror(" kill");
            }
        }
    } else { // Case where just a pid is given
        if ((pid = atoi(commandList[1])) == 0) { // If an invalid argument was entered, such as a string
            errno = EINVAL;
            perror(" kill");
        } else {
            if (kill(pid, SIGTERM) != 0) { // If the pid does not exist
                errno = ESRCH;
                perror(" kill");
            }
        }
    }
}


/**
 * setEnvironment, when called with no arguments, prints out all the environment
 *                 variables like printEnvironment does. When called with one
 *                 argument, sets it as an empty environment variable. When called
 *                 with two arguments, sets the first equal to the second. More than
 *                 two arguments should result in an error message. This will also 
 *                 handle two special cases: One if the HOME variable is changed,
 *                 Two if the PATH variable is changed.
 * 
 * Consumes: Two lists of strings
 * Produces: Nothing
 */
void setEnvironment(char **commandList, char **envp) {
    if (commandList[3] != NULL) { // Case where more than two arguments are used
        fprintf(stderr, "%s", " setenv: Too many arguments\n");
    } else if (commandList[1] == NULL) { // Case where command is called with no arguments
        for (int i = 0; envp[i] != NULL; i++) {
            printf(" \n%s", envp[i]);
        }
    } else if (commandList[2] != NULL) { // Case where called with two arguments
        setenv(commandList[1], commandList[2], 1); // Should overwrite existing environment variables
    } else { // Case where called with one argument
        setenv(commandList[1], "", 1); // Empty environment variable
    }
}


/**
 * printEnvironment, when given no arguments, prints all of the enviornment variables.
 *           When given one argument, calls getenv(3). Two or more arguments
 *           are not accepted and will invoke an error message.
 * 
 * Consumes: A string, An array of strings
 * Produces: Nothing
 */
void printEnvironment(char **commandList, char **envp) {
    if (commandList[2] != NULL) { // If two or more args were used
        fprintf(stderr, "%s", " printenv: Too many arguments\n"); 
    } else if (commandList[1] == NULL) { // Case where called with no argument
        for (int i = 0; envp[i] != NULL; i++) {
            printf("\n%s", envp[i]);
        }
    } else { // Else find environment variable given as argument and print
        if (getenv(commandList[1]) != NULL) 
            printf(" %s\n", getenv(commandList[1]));
        else 
            fprintf(stderr, "%s", " Error: environment variable not found\n");
    }
}


/**
 * changeDirectory, with no arguments, changes the cwd to the home directory. 
 *                  with "-" as an argument, changes directory to the one 
 *                  previously in. Otherwise change to the directory given as
 *                  the argument.
 * 
 * Consumes: A list of strings
 * Produces: Nothing
 */
void changeDirectory(char *commandList[]) {
    if (commandList[1] == NULL) { // Case where no argument is given
        if (chdir(getenv("HOME")) != 0) { // Go to home
            errno = ENOENT;
            perror(" Error ");
        }
    } else if (strcmp(commandList[1], "-") == 0) { // Case where "-" is given
        if(chdir(previousWorkingDirectory) != 0) {
            printf(" Error moving to previous directory\n");
        }
    } else {
        if (chdir(commandList[1]) != 0) {
            errno = ENOENT;
            perror(" Error ");
        }
    }
}


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
void prompt(char **commandList) {
    if (commandList[1] != NULL) {
        prefix = '\0'; // strcat must use null terminated strings
        prefix = (char *)malloc(sizeof(commandList));
        for (int i = 1; commandList[i] != NULL; i++) {
            strcat(prefix, commandList[i]); // Remember prefix has a global scope
            strcat(prefix, " ");
        }
    } else {
        char tempBuffer[BUFFERSIZE];
        printf(" input prompt prefix: ");
        fgets(tempBuffer, BUFFERSIZE, stdin);
        tempBuffer[strlen(tempBuffer)-1] = '\0';
        prefix = (char *)malloc(strlen(tempBuffer));
        strcpy(prefix, tempBuffer); // Copy buffer to prefix
    }
}


/**
 * which, locates commands. Returns the location of the command given as the argument.
 *                          If this function is called, don't forget to free the returned
 *                          string at some point.
 * 
 * Consumes: A string, A list of strings
 * Produces: A string
 */
char *which(char *command, struct pathelement *pathlist) {
    /* loop through pathlist until finding command and return it. Return NULL when not found. */
    char temp[BUFFERSIZE];
    DIR *dp;
    struct dirent *dirp;
    while (pathlist) { // Traverse path until NULL 
        if ((dp = opendir(pathlist->element)) == NULL) {  // If element is not a directory
            errno = ENOTDIR;
            perror(" Error opening");
            exit(errno);
        } 
        while ((dirp = readdir(dp)) != NULL) { // traverse files in opened directories
            if (strcmp(dirp->d_name, command) == 0) { // If command is found do some string copying then return
                if (access(pathlist->element, X_OK) == 0) { // Make sure to only return the executable command
                    strcpy(temp, pathlist->element);
                    strcat(temp, "/"); // Special character in UNIX
                    strcat(temp, dirp->d_name); // Concatenate full path name
                    printf(" %s\n", temp);
                    char *path = (char * )malloc(strlen(temp));
                    strcpy(path, temp); // dest, src
                    closedir(dp);
                    return path; // Exits function
                }
            }
        }
        closedir(dp);
        pathlist = pathlist->next;
    }
    printf(" %s: Command not found.\n", command);
    return NULL; // If the command was not found
} /* which() */


/**
 * where, returns all instances of the command in path. This is the same code as 
 *        the which function, except it is checking for existence of the files
 *        rather than execution permission. Also the loop doesn't stop when one
 *        file is found, rather all files containing the command string will be
 *        returned
 * 
 * Consumes: A string, A list of strings
 * Produces: A string
 */
char *where(char *command, struct pathelement *pathlist ) {
  /* similarly loop through finding all locations of command */
  char temp[BUFFERSIZE];
    DIR *dp;
    struct dirent *dirp;
    char *path = NULL;
    while (pathlist) { // Traverse path until NULL 
        if ((dp = opendir(pathlist->element)) == NULL) {  // If element is not a directory
            errno = ENOTDIR;
            perror(" Error opening");
            exit(errno);
        } 
        while ((dirp = readdir(dp)) != NULL) { // traverse files in opened directories
            if (strstr(dirp->d_name, command) == 0) { // If command is found do some string copying then return
                if (access(pathlist->element, F_OK) == 0) { // Make sure to only return the executable command
                    strcpy(temp, pathlist->element);
                    strcat(temp, "/"); // Special character in UNIX
                    strcat(temp, dirp->d_name); // Concatenate full path name
                    strcat(temp, "\n");
                    strcpy(path, temp); // dest, src
                }
            }
        }
        closedir(dp);
        pathlist = pathlist->next;
    }
    if (path == NULL) {
        printf(" %s: Command not found.\n", command);
        return NULL; // Null if not found
    } else {
        char *path = (char * )malloc(strlen(temp));
        return path;
    }
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
      closedir(dp); // Close the directory opened
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


/**
 * whichHandler, Handles multiples args being sent to which
 * 
 * Consumes: Two lists of strings
 * Produces: Nothing
 */
void whichHandler(char *commandList[], struct pathelement *pathList) {
    for (int i = 1; commandList[i] != NULL; i++) {
            which(commandList[i], pathList);
        }
}


/**
 * whereHandler, Handles multiples args being sent to where
 * 
 * Consumes: Two lists of strings
 * Produces: Nothing
 */
void whereHandler(char *commandList[], struct pathelement *pathList) {
    for (int i = 1; commandList[i] != NULL; i++) {
            which(commandList[i], pathList);
        }
}