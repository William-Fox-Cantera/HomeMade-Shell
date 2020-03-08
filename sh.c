#include "sh.h"

/* TODO: 
    - Use stat not access in the runExecutable function
    - Handle memory leaks
*/

//****************************************************************************************************************************
// GLOBALS
char *prefix; // String that precedes prompt when using the prompt function
char previousWorkingDirectory[BUFFERSIZE]; // System for going back to previous directory with "cd -"
int timeout = 0;

//****************************************************************************************************************************
int sh(int argc, char **argv, char **envp) {
    // MY VARIABLES
    int csource, go = 1, builtInExitCode = 0, wasGlobbed = 0, noPattern = 0; // integers
    char buffer[BUFFERSIZE]; // Read commands from stdin
    char *commandList[BUFFERSIZE]; // Store the commands and flags typed by the user
    char *currentWorkingDirectory = ""; // Give it something to chew on to avoid segentation fault
    char *cwd; // For updating the current workinig directory
    struct pathelement *pathList = get_path(); // Put PATH into a linked list 
    glob_t paths;
    char **p;
    char *token; // String that will help parse the buffer from fgets

    if (argv[1] == NULL) { // If no argument was given
        errno = EINVAL;
        perror("Specify time limit");
        freePath(pathList);
        return;
    } 
    if (atoi(argv[1]) == 0) { // If an invalid time limit was given
        errno = EINVAL;
        perror("Time limit");
        freePath(pathList);
        return;
    }

    /* Main Loop For Shell */
    while (go) {
        memset(commandList, 0, sizeof(commandList)); // // Reset the array of commands with NULL each loop
        wasGlobbed, noPattern = 0; // For continuing the loop of no globbing pattern was matched
        /* Keep track of the previous and current working directories */
        if (strcmp(currentWorkingDirectory, cwd = getcwd(NULL, 0)) != 0) {
            if (strcmp(currentWorkingDirectory, "")) {
                strcpy(previousWorkingDirectory, currentWorkingDirectory);
                free(currentWorkingDirectory);
            }
            currentWorkingDirectory = getcwd(NULL, 0);
        }
        free(cwd);

        /* print your prompt */
        printShell();

        /* get command line and process */
        if (fgets(buffer, BUFFERSIZE, stdin) == NULL) { // Ignore ctrl+d a.k.a. EOF
            printf("^D\nUse \"exit\" to leave shell.\n");
            continue;
        }

        if (strlen(buffer) < 2) // Ignore empty stdin, ie. user presses enter with no input
            continue; 

        if (buffer[strlen(buffer)-1] == '\n') // Handle \n from fgets
            buffer[strlen(buffer)-1] = '\0'; 
    
        // Main Argumment Parser
        token = strtok (buffer, " "); 
        for (int i = 0; token != NULL; i++) {
            if ((strstr(token, "*") != NULL) || (strstr(token, "?") != NULL)) { // Globbing support
                wasGlobbed = 1;
                csource = glob(token, 0, NULL, &paths);	   
                if (csource == 0) {
                    for (p = paths.gl_pathv; *p != NULL; p++) {
                        commandList[i] = (char *)malloc(sizeof(*p)); // Must malloc for glob
                        strcpy(commandList[i], *p);
                        i++;
                    }
                    globfree(&paths);
                } else { // If no glob pattern was found
                    errno = GLOB_NOMATCH;
                    perror(" glob ");
                    noPattern = 1;
                    break;
                }
            } else {
                commandList[i] = token;
            }
            token = strtok(NULL, " ");
        }
        if (wasGlobbed && noPattern) // If globbing was attempted but no patterns matched, continue
            continue;
        if (isBuiltIn(commandList[0])) { // Check to see if the command refers to an already built in command
        /* check for each built in command and implement */
            printf(" Executing built-in: %s\n", commandList[0]);
            builtInExitCode = runBuiltIn(commandList, pathList, envp);
            if (builtInExitCode == 1) { // For exiting entire shell program
                freeAll(pathList, currentWorkingDirectory); // Cleanup allocs
                exit(0); // Exit without error
            }
        } else {
            // When this function is done all of its local variables pop off the stack so no need to memset
            runExecutable(commandList, envp, pathList, argv); 
        } // Reset stuff for next iteration
        if (wasGlobbed) { // Globbing requires memory allocation, free it
            for (int i = 0; commandList[i] != NULL; i++) 
                free(commandList[i]);
            }
        if (builtInExitCode == 2) 
            pathList = get_path(); // Update the path linked list if it was changed with setenv
    }
    return 0;
} /* sh() */




// HELPER FUNCTIONS
//***************************************************************************************************************

/**
 * runExecutable, if the file contains an absolute path then check if it is executable
 *                   and start a new process with fork and run it with exec. If it is not a
 *                   direct path, then find where the command is using the which function and
 *                   use fork/exec on that. 
 * 
 * Consumes: Three lists of strings, a struct
 * Produces: Nothing
 */
void runExecutable(char **commandList, char **envp, struct pathelement *pathList, char **argv) {
    int status;
    char *externalPath = getExternalPath(commandList, pathList);
    if (externalPath != NULL) {
        printf(" Executing %s\n", externalPath);
        /* do fork(), execve() and waitpid() */
        if ((pid = fork()) < 0) { // Child
            perror(" fork error");
        } else if (pid == 0) {
            execve(externalPath, commandList, envp);
            printf(" couldn't execute: %s", strerror(errno));
            exit(127);
        }
        signal(SIGALRM, alarmHandler); // Callback to update timeout global
        signal(SIGCHLD, childHandler);
        alarm(atoi(argv[1])); // install an alarm to be fired after the given time (argv[1])
        pause(); // Stop everything until child dies
        if (timeout) {
            int result = waitpid(pid, &status, WNOHANG);
            if (result == 0) {
                printf("!!! taking too long to execute this command !!!\n"); 
                kill(pid, 9); // If child is still running after time limit, kill it
                wait(NULL);
            } 
        } 
        printf(" exit code of child: %d\n", WEXITSTATUS(status)); // Print actual exit status
        free(externalPath); // Free it
    } 
}


/**
 * childHandler, callback function for signal SIGCHLD, this function literally 
 *               does nothing except satisfy signals demand for a callback.
 */
void childHandler(int signal) {
    return;
}


/**
 * alarmHandler, updates the global variable timeout when called by signal(2)
 *               in the runExecutable function.
 * 
 * Consumes: An integer
 * Produces: Nothing
 */
void alarmHandler(int signal) {
    timeout = 1;
}

/**
 * getExternalPath, either returns the command if it is an absolute path ie. contains
 *                  a / ./ or ../ or something of the like, or returns the path to an 
 *                  executable found by the which function.
 * 
 * Consumes: A list of strings, a struct
 * Produces: A string
 */
char *getExternalPath(char **commandList, struct pathelement *pathList) {
    char *externalPath;
    char temp[BUFFERSIZE];
    struct stat file;
    // If the command contains an absolute path ie. /, ./ ../, then check for executability
    if (strstr(commandList[0], "./") || strstr(commandList[0], "../") || strstr(commandList[0], "/")) { 
        if (stat(commandList[0], &file) == 0) { // If stat worked
            if (file.st_mode & S_IXUSR  && file.st_mode & S_IXGRP && file.st_mode & S_IXOTH) { // Make sure it is executable by User, Group, Others
                printf(" Executing: %s\n", commandList[0]);
                strcpy(temp, commandList[0]);
                externalPath = (char *)malloc(sizeof(temp));
                strcpy(externalPath, temp);
            } else {
                errno = EACCES; // Permission denied
                printf(" %s: %s\n", commandList[0], strerror(errno));
                return NULL;
            }
        } else { // Else the file is not executable
            perror("stat error");
            return NULL;
        }
    } else { // Find the command in the PATH environment variable, executability for this is already checked in which
        externalPath = which(commandList[0], pathList); // Must free
    }
    return externalPath;
} 


/**
 * isBuiltIn, determines whether the command given is apart of the built in 
 *            commands list. Returns 1 if true, 0 otherwise.
 * 
 * Consumes: A string
 * Produces: An integer
 */
int isBuiltIn(char *command) {
    const char *builtInCommands[] = {"exit", "which", "where", "cd", "pwd", "list", "pid", "kill", "prompt", "printenv", "setenv"};
    int inList = 0; // False
    for (int i = 0; i < BUILT_IN_COMMAND_COUNT; i++) {
        if (strcmp(command, builtInCommands[i]) == 0) { // strcmp returns 0 if true
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
 *             EXIT CODES FOR THIS FUNCTION:
 *                 - 1 --> Exit program
 *                 - 2 --> Path environment variable was changed
 * 
 * Consumes: A string
 * Produces: Nothing
 */
int runBuiltIn(char *commandList[], struct pathelement *pathList, char **envp) { // commandList[0] will always be the command itself
    int shouldExit = 0;
    int pathChanged = 0;
    if (strcmp(commandList[0], "exit") == 0) { // strcmp returns 0 if true
        shouldExit = exitProgram(); // Exit the program and thus the shell
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
        pathChanged = setEnvironment(commandList, envp, pathList);
    }
    if (pathChanged)
        return pathChanged;
    return shouldExit ? 1 : 0;
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
        commandList[1][0] = ' '; // Ignore the prepended "-" character
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
 * Produces: An integer
 */
int setEnvironment(char **commandList, char **envp, struct pathelement *pathList) {
    int pathChanged = 0;
    if (commandList[3] != NULL) { // Case where more than two arguments are used
        fprintf(stderr, "%s", " setenv: Too many arguments.\n");
    } else if (commandList[1] == NULL) { // Case where command is called with no arguments
        for (int i = 0; envp[i] != NULL; i++) {
            printf(" \n%s", envp[i]);
        }
    } else if (commandList[2] != NULL) { // Case where called with two arguments
        if (strcmp("PATH", commandList[1])) {
            freePath(pathList); // Free up space from old path
            setenv("PATH", commandList[2], 1); // Make new path
            pathChanged = 1; // Return this
        } else {
            setenv(commandList[1], commandList[2], 1); // Should overwrite existing environment variables
            pathChanged = 1; // Return this
        }
    } else { // Case where called with one argument
        if (strcmp(commandList[1], "PATH") == 0) { // Case where user changes path to empty string
            freePath(pathList);
            setenv("PATH", "", 1);
        } else {
            setenv(commandList[1], "", 1); // Empty environment variable
        }
    }
    return pathChanged ? 2 : 0; // See exit codes in the runBuiltIn function
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
        if (chdir(commandList[1]) != 0) { // Handles normal in chdir call
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
 * exitProgram, returns 1 if called.
 * 
 * Consumes: Nothing
 * Produces: Nothing
 */
int exitProgram() {
    return 1; // Exit without error
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
        prefix = (char *)malloc(sizeof(commandList));
        strcpy(prefix, "");
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
                    char *path = (char *)malloc(strlen(temp));
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
 *        the which function except the loop doesn't stop when one
 *        file is found, rather all files containing the command string will be
 *        returned assuming they're executables.
 * 
 * Consumes: A string, A list of strings
 * Produces: A string
 */
char *where(char *command, struct pathelement *pathlist) {
/* similarly loop through finding all locations of command */
    char temp[BUFFERSIZE] = "";
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
                    strcat(temp, pathlist->element);
                    strcat(temp, "/"); // Special character in UNIX
                    strcat(temp, dirp->d_name); // Concatenate full path name
                    strcat(temp, "\n\n");
                }
            }
        }
        closedir(dp);
        pathlist = pathlist->next;
    }
    if (temp == NULL)
        return NULL;
    char *path = (char *)malloc(sizeof(temp));
    strcpy(path, temp);
    return path;
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
      free(dp);
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
    printf("[%s]>", ptr);
  free(ptr); 
}


/**
 * listHandler, handles the logic for the list function. Checks if called
 *              with no arguments, or with arguments and calls list accordingly.
 * 
 * Consumes: An array of strings
 * Produces: Nothing
 */
void listHandler(char **commandList) {
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
void whichHandler(char **commandList, struct pathelement *pathList) {
    char *pathToCmd;
    for (int i = 1; commandList[i] != NULL; i++) {
            pathToCmd = which(commandList[i], pathList);
            if (pathToCmd != NULL)
                printf(" %s\n", pathToCmd);
        }
}


/**
 * whereHandler, Handles multiples args being sent to where
 * 
 * Consumes: Two lists of strings
 * Produces: Nothing
 */
void whereHandler(char **commandList, struct pathelement *pathList) {
    char *paths;
    int isNull = 1;
    for (int i = 1; commandList[i] != NULL; i++) {
        paths = where(commandList[i], pathList);
        if (paths != NULL)
            printf("%s", paths);
        else 
            printf(" %s: command not found\n", commandList[i]);
        free(paths);
    }
}


/**
 * freeAll, frees all of the allocs that were not already freed right after
 *          they were used.
 * 
 * Consumes: Stuff
 * Produces: Nothing
 */
void freeAll(struct pathelement *pathList, char *cwd) {
    freePath(pathList);
    free(path); // From get_path.h
    free(cwd);
}


/**
 * freePath, if the path environment variable is changed vis setenv, 
 *           the linked list data structure holding the path elements
 *           is freed using this function.
 * 
 * Consumes: A structure
 * Produces: Nothing
 */
void freePath(struct pathelement *pathList) {
    struct pathelement *temp;
    while (pathList != NULL) {
        temp = pathList;
        pathList = pathList->next;
        free(temp);
    }
} 