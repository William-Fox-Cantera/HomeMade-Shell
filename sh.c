#include "sh.h"


//*****************************************************************************************************************************
// GLOBALS


char *prefix = NULL; // String that precedes prompt when using the prompt function
char previousWorkingDirectory[BUFFERSIZE]; // System for going back to previous directory with "cd -"
char *myCwd = ""; // For keeping track of your current working directory
int timeout = 0; // For ensuring processes are killed after the given time limit at argv[0]
int isChildDone = 0; // For indicating whether or not a child process has finished
int pid; // Variable for holding the pid of processes 
int threadExists = 0; // For indicating if the pthread for watchusers exists, only want to create one
pthread_t watchUserID; // Thread for watchusers, this should be the only running thread for watchusers
pthread_mutex_t mutexLock; // For locking mutually exclusive objects in the user list becuase there is only one thread all users must share
int hasNoClobber = 0; // Either 0 or 1 for turning the "noclobber" option on or off


// END GLOBALS
//*****************************************************************************************************************************


//*****************************************************************************************************************************
// MAIN LOOP FOR SHELL


/**
 * sh, this is the main faunction for the entire shell. Most of the code in this project is called from here.
 *     This function contains a while loops that runs until you exit the shell. First some important variables
 *     are assigned, then into the loop. Input is read using fgets and then the input is parsed. The parser ignores
 *     spaces and puts the input into an array called commandList which is passed  to many other functions in this file.
 *     The parser also handles globbing if necessary, and all memory allocated for the globbing as well as memory used by
 *     the commandList itself is freed before the next iteration.
 * 
 * Consumes: Two arrays of strings, an integer
 * Produces: An integer
 */
void sh(int argc, char **argv, char **envp) {
    handleInvalidArguments(argv[1]); // Make sure a valid number for the time limit was entered, else exit
    char buffer[BUFFERSIZE], **commandList; // For printing to the terminal
    struct pathelement *pathList = getPath(); // Put PATH into a linked list 
    commandList = calloc(MAX_CMD, sizeof(char *)); // The key to this whole project, holds all user input
    while (1) { // Main Loop For Shell, runs until the "exit" built-in is called, or kill is used
        cwdManager(); // For managing current and previous directories
        printShell(); // Prints the prompt each loop
        if (fgets(buffer, BUFFERSIZE, stdin) == NULL) { // Ignore ctrl+d a.k.a. EOF, otherwise gets user input to process
            printf("^D\nUse \"exit\" to leave shell.\n");
            continue; // Continue just ignores the rest of the loop and continues to the next iteration
        }
        buffer[strlen(buffer)-1] = '\0'; // Handle \n appended by fgets()
        if (strlen(buffer) < 1) // Ignore empty stdin, ie. user presses enter with no input
            continue; 
        commandList = parseBuffer(buffer, commandList); // Parsing the command line arguments entered
        executeIt(commandList, envp, pathList, argv); // This function calls all the other functions in this file
    }
} 


// END MAIN LOOP FOR SHELL
//*****************************************************************************************************************************


//*****************************************************************************************************************************
// HELPER FUNCTIONS


/**
 * cwdManager, manages allocating and freeing the space taken by the getcwd(2) system call. MAnages two global variables: myCwd
 *             and previousWorkingDirectory. This function makes sure the user can use the "cd -" command to go back to the
 *             previous directory and makes sure there are no memory leaks in the process.
 * 
 * Consumes: Nothing
 * Produces: Nothing
 */
void cwdManager() {
    char *cwd, *cwd2;
    if (strcmp(myCwd, cwd = getcwd(NULL, 0)) != 0) { // Keep track of the previous and current working directories 
        if (strcmp(myCwd, "")) {
            strcpy(previousWorkingDirectory, myCwd);
            free(myCwd);
        }
        cwd2 = getcwd(NULL, 0);
        myCwd = (char *)malloc((int)strlen(cwd2)+1);
        strcpy(myCwd, cwd2);
        free(cwd2);
    }
    free(cwd); // getcwd(2) system call will dynamically allocate memory, free it
}


/**
 * parseBuffer, gets the string produced by fgets and tokenizes it via strtok. This handles allocating memory for the commandList
 *              be it the commands entered by the user or the glob paths found by the glob(3) library function. This function
 *              returns the command list and of course, it must be reset and freed in the sh function. If globbing is detected
 *              and no patterns are found, an error message is displayed but the command preceding the glob will still go through.
 * 
 * Consumes: A string, An array of strings
 * Produces: An array of strings
 */
char **parseBuffer(char buffer[], char **commandList) {
    int csource;
    char *token;
    glob_t paths;
    // Main Argumment Parser
    token = strtok (buffer, " ");                       // HOW GLOBBING IS IMPLEMENTED: I used strstr() to check to see if a * or ? character appear
    for (int i = 0; token != NULL;) {                                       //          in the buffer from fgets(). I then used glob(3) to expand the 
        if ((strstr(token, "*") != NULL) || (strstr(token, "?") != NULL)) { //          possible paths if there are any. If there are no paths, an error
            csource = glob(token, 0, NULL, &paths);                         //          message is printed and the normal command is called without any	   
            if (csource == 0) {                                             //          paths such as ls -l. If there were paths, the number of them is
                for (char **p = paths.gl_pathv; *p != NULL; p++) {          //          saved in an int variable and the expanded list provided by glob
                    commandList[i] = (char *)malloc((int)strlen(*p)+1);     //          is passed off to execve() as an argv array. The glob list is freed
                    strcpy(commandList[i], *p);                             //          at the end of this while loop using the counter mentioned previously.
                    i++;
                }
                globfree(&paths);
            } else { // If no glob pattern was found
                errno = ENOENT;
                perror("glob");
            }
        } else {
            commandList[i] = (char *)malloc((int)strlen(token)+1);
            strcpy(commandList[i], token);
            i++;
        }
        token = strtok(NULL, " "); // Tokenize by spaces
    }
    return commandList;
}


/**
 * executeIt, this is the function that is ultimately responsible for running every other function in this file at some point. 
 *            First if a pipe was entered as a command, the handlePipes function is called to use its special logic for
 *            properly running a piped command. Next, if no pipe was found, runCommand is called to run normal commands that
 *            don't use pipes. This will be either an external or built-in command.
 * 
 * Consumes: Three arrays of strings, a struct
 * Produces: Nothing
 */
void executeIt(char **commandList, char **envp, struct pathelement *pathList, char **argv) {
    if (!handlePipes(commandList, envp, pathList, argv))                // This runs logic for handling pipes if they exist in the commandList. If there are
        runExecutable(commandList, envp, pathList, argv);// no pipes, handlePipes will return 0 and thus the run command function will get 
                                                                         // called instead for running a normal command without pipes.
    for (int i = 0; commandList[i] != NULL; i++) { 
        free(commandList[i]); // Freeing the command list 
        commandList[i] = NULL; // Reset it for the next loop
    }
}


/**
 * shouldRunAsBackground, searches through the commandList array tokenized in the main loop and returns
 *                        1 if the & (ampersand) character appears at the end of the array as an indicator
 *                        that a process should be run in the background. 0 otherwise.
 * 
 *                        IMPORTANT: The return for this function acts as an index in which to remove the '&'
 *                                   character, as well as to evaluate to true if non-zero.
 * 
 * Consumes: An array of strings
 * Produces: An integer
 */
int shouldRunAsBackground(char **commandList) {
    int shouldRun = 0;
    for (int i = 0; commandList[i] != NULL; i++) 
        if (!strcmp(commandList[i], "&") && commandList[i+1] == NULL) { // Make sure & is the last thing in commandList
            shouldRun = i; // If it is return 1 (true)
            break;
        }
    return shouldRun;
}   


/**
 * runExecutable, First checks if the command to run is a built-in command. If it is, the built-in id run and this function returns
 *                before any forking takes place. If it is not a built-in the getExternalPath function along with other functions for
 *                checking fore redirection are called. THe path for the external command is returned, either sn absolute, ie. contains
 *                a "./" or "../", or "/", or something of the like, or the which function returns the path to the command if it is not
 *                an absolute or relative path. Then fork() is called and execve is called rirght after witht the appropriate commandList
 *                for either a normal command or one involving redirection. The parent handles a couple of things. FIrst if a command takes
 *                longer than the time interval specified in the command line argument, the process is aborted and the child is killed. If
 *                the command finishes in time, no problem. There is also good use of waitpid here as well as a callback such that zombies
 *                are avoided.
 * 
 * Consumes: Three lists of strings, a struct
 * Produces: Nothing
 */
void runExecutable(char **commandList, char **envp, struct pathelement *pathList, char **argv) {
    if (isBuiltIn(commandList[0])) { // check for each built in command and implement 
        printf("Executing built-in: %s\n", commandList[0]);
        runBuiltIn(commandList, pathList, envp); 
        return; // Definitely don't want to fork() if its a built-in command
    } 
    int result, abortProcess = 0, status = 0, redirectionType = getRedirectionType(commandList);
    int shouldRunInBg = shouldRunAsBackground(commandList); // If the command should run as a bacgground process
    char *externalPath = getExternalPath(commandList, pathList); // Returns the path for the command, using which, or a path entered by user
    if (shouldRunInBg)
        commandList[shouldRunInBg] = '\0'; // Get rid of the &, no need for it plus it causes problems
    if (externalPath != NULL) {
        printf("Executing: %s\n", externalPath);
        // Child
        if ((pid = fork()) < 0) { // do fork(), execve() and waitpid() 
            perror("fork error");
        } else if (pid == 0) {
            if (redirectionType) { // If redirection was detected, not 0
                abortProcess = handleRedirection(redirectionType, getRedirectionDest(commandList)); // This is where the redirection magic happens
                removeAfterRedirect(commandList); // Get rid of the redirection symbol and the arguments that come after it                     
            }
            if (abortProcess) // For redirection problems such as noclobber conflict
                kill(pid, SIGTERM);
            execve(externalPath, commandList, envp);
            perror("execve problem: "); // Code should never reach here...
        }
        // Parent
        if (shouldRunInBg) { // If it SHOULD be run as a background process
            printShell();
        if ((result = waitpid(pid, &status, WNOHANG)) == -1 && errno != ECHILD) // ECHLD error not an error
            perror("waitpid error");
        } else { // If it should NOT be run as a background process
            free(externalPath); // Free it
            signal(SIGALRM, alarmHandler); // Callback to update timeout global
            signal(SIGCHLD, childHandler); // Callback to update isChildDone global
            alarm(atoi(argv[1])); // install an alarm to be fired after the given time (argv[1])
            pause(); // Stop everything until child dies
            if (timeout) {
                if ((result = waitpid(pid, &status, WNOHANG)) == -1 && errno != ECHILD) { // Ignore no child process, not an error
                    perror("waitpid error");
                } else if (result == 0) {
                    printf("!!! taking too long to execute this command !!!\n"); 
                    kill(pid, SIGINT); // If child is still running after time limit, kill it. SIGTERM allows the process to exit normally.
                }
            }
        }
        printf("exit code of child: %d\n", WEXITSTATUS(status)); // Print actual exit status
    } 
}


/**
 * childHandler, callback function for signal SIGCHLD, the reason waitpid uses WNOHANG is so that 
 *               waitpid cannot block. The reason waitpid is in a loop is because if there are multiple
 *               zombies, they all get reaped.
 * 
 * Consumes: An integer
 * Produces: Nothing
 */
void childHandler(int signal) {
    isChildDone = 1;
    while (waitpid((pid_t)(-1), 0, WNOHANG) > 0) {}
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
            if (S_ISDIR(file.st_mode)) { // Case where the path to be executed is a directory
                errno = EISDIR;
                printf("shell: %s: %s\n", commandList[0], strerror(errno));
                return NULL;
            }
            if (file.st_mode & S_IXUSR && file.st_mode & S_IXGRP && file.st_mode & S_IXOTH) { // Make sure it is executable by User, Group, Others
                strcpy(temp, commandList[0]);
                externalPath = (char *)malloc(sizeof(temp));
                strcpy(externalPath, temp);
            } else {
                errno = EACCES; // Permission denied
                printf("%s: %s\n", commandList[0], strerror(errno));
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
 * watchMailCallback, this function is a callback which is called when pthread_create is used when using the watchmail
 *                    built-in command. When a file specified by the user in the mailList linked list increases in size,
 *                    a beep noise is played and a message is printed out to notify the user that there is new mail in 
 *                    file that got larger. This is accomplished through an infinite loop that sleeps for 1 second each
 *                    iteration and uses stat(2) to check for file size increases among the files in the mailList data
 *                    structure (linked list).
 * 
 * Consumes: Args that are given by pthread_create
 * Produces: Nothing
 */
void *watchMailCallback(void *callbackArgs) {
    char *fileName = (char *)callbackArgs;
    struct stat st;
    if (stat(fileName, &st) != 0) {
        perror("stat problem"); // If stat system call failed
        pthread_exit(NULL); // As per instructions, use pthread_exit if an error condition occurrs in a watchmail thread
    }
    long prevFileSize = (long)st.st_size;
    struct timeval tv;
    time_t currentTime;
    while(1) {
        if (gettimeofday(&tv, NULL) != 0) { // Timezone does not matter
            perror("Get current time problem"); // If getting thhe current time failed
            pthread_exit(NULL);
        }
        currentTime = tv.tv_sec; // Update the timeval struct
        if (stat(fileName, &st) != 0) { // Keep updating the stat buffer
            perror("stat problem"); // If stat system call failed
            pthread_exit(NULL);
        }
        if ((long)st.st_size > prevFileSize) { // Print a notification saying you've got mail if the file size increased
            printf("\a\nYou've Got Mail in %s at %s", fileName, ctime(&currentTime));
            prevFileSize = (long)st.st_size; // Update previous time
        }
        sleep(1); // Wait for 1 second in between possible mail notifications
    }
}


/**
 * watchUserCallback, loops infinitely on a sleep timer of 20 seconds. Finds the user on the machine and tracks their logins.
 *                  The data shared the global users linked list is protected by a mutex_lock and unlock.
 * 
 * Consumes: Anything, whatever the watchUser built-in supplies
 * Produces: Nothing
 */
void *watchUserCallback(void *callbackArgs) {
    struct utmpx *up;
    while(1) {
        setutxent(); // Start at beginning
        while((up = getutxent())) { // Get an entry
            if (up->ut_type == USER_PROCESS) { // Only care about users
                pthread_mutex_lock(&mutexLock);
                struct user *someUser = userHead;
                while (someUser) {
                    if (strcmp(someUser->username, up->ut_user) == 0) { // If user exists
                        someUser->isLoggedOn = 1;
                        printf("\n%s has logged on %s from %s\n", up->ut_user, up->ut_line, up->ut_host);
                    } else {
                        printf("\n%s, no such user\n", someUser->username);
                    }
                    someUser = someUser->next;
                }
                pthread_mutex_unlock(&mutexLock);
            }
        }
        printUsers();
        sleep(5);
    }
    return NULL;
}


/**
 * isBuiltIn, determines whether the command given is apart of the built in commands list. Returns 1 if true, 0 otherwise.
 * 
 * Consumes: A string
 * Produces: An integer
 */
int isBuiltIn(char *command) {
    const char *builtInCommands[] = {"exit", "which", "where", "cd", "pwd", "list", "pid", "kill", "prompt", 
                                     "printenv", "setenv", "watchuser", "watchmail", "noclobber"};
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
 * runBuiltIn, runs the built in commands, if a command like list is called with multiple arguments, its handler function is called, 
 *             calling the command for each argument given. This function also handles exiting the program and freeing/getting the 
 *             old and new linked lists for the PATH environment variable if need be.
 * 
 * Consumes: Two arrays of strings, a string, a struct
 * Produces: Nothing
 */
void runBuiltIn(char **commandList, struct pathelement *pathList, char **envp) { // commandList[0] will always be the command itself
    int shouldExit = 0, pathChanged = 0;
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
    } else if (strcmp(commandList[0], "watchuser") == 0) {
        watchUser(commandList);
    } else if (strcmp(commandList[0], "watchmail") == 0) {
        watchMail(commandList);
    } else if (strcmp(commandList[0], "noclobber") == 0) {
        noClobber();
    }
    if (pathChanged) {
        freePath(pathList);
        pathList = getPath();
    }
    if (shouldExit)
        freeAndExit(pathList, commandList);
}


// END HELPER FUNCTIONS
//*****************************************************************************************************************************


//*****************************************************************************************************************************
// PIPES AND REDIRECTION


// REDIRECTION
/**
 * getRedirectionType, traverses the commandList array and searches for a redirection type. If found it will return one of ">",
 *                     ">&", ">", ">>&", ">". If none of these are found, NULL will be returned.
 * 
 *                     IMPORTANT: This function returns 1 of 6 integers 0-5
 *                                -0: No redirection found
 *                                -1: ">"   found
 *                                -2: ">>"  found
 *                                -3: "<"   found 
 *                                -4: ">>&" found
 *                                -5: ">&"  found
 * 
 * Consumes: An array of strings
 * Produces: An integer
 */
int getRedirectionType(char **commandList) {
    int redirectionType = 0;
    for (int i = 0; commandList[i] != NULL; i++) {
        if (!strcmp(commandList[i], ">")) 
            redirectionType = 1;
        if (!strcmp(commandList[i], ">>"))
            redirectionType = 2;
        if (!strcmp(commandList[i], "<"))
            redirectionType = 3;
        if (!strcmp(commandList[i], ">>&"))
            redirectionType = 4;
        if (!strcmp(commandList[i], ">&"))
            redirectionType = 5;
    }
    return redirectionType;
}


/**
 * removeAfterRedirect, this function removes all garbage in the command list including and after the redirection symbol because execve
 *                      only cares about what comes before the redirection symbol. All the extra stuff after will cause problems in 
 *                      exec if not removed.
 * 
 * Consumes: An array of strings
 * Produces: Nothing
 */
void removeAfterRedirect(char **commandList) {
    int commandCount = 0;
    while(1) {
        if (strstr(commandList[commandCount], "<") || strstr(commandList[commandCount], ">"))
            break;
        commandCount++;
    }
    while (commandList[commandCount] != NULL) {
        commandList[commandCount] = NULL;
        commandCount++;
    }
}


/**
 * getRedirectionDest, gets the string input by th user directly after the redirection symbol. This string is taken to be a filename
 *                     in which the contents of a command will be redirected to. So ls > temp.txt would dump the contents of ls into
 *                     temp.txt. This function gets that temp.txt filename.
 * 
 * Consumes: An array of strings
 * Produces: A string
 */
char *getRedirectionDest(char **commandList) {
    char *redirectionDest;
    for (int i = 0; commandList[i] != NULL; i++)
        if (strstr(commandList[i], "<") || strstr(commandList[i], ">"))
            if (commandList[i+1] != NULL) {
                redirectionDest = (char *)malloc(strlen(commandList[i+1])+1);
                strcpy(redirectionDest, commandList[i+1]);
                return redirectionDest;
            }
    return NULL;
}


/**
 * handleRedirection, please refer to the documentation above the getRedirectionType function to know more about what the redirection
 *                    parameter is. This function also returns an integer, 0 if the redirection can go through, or 1 if noclobber is
 *                    turned on and an issue overwriting could arise. 
 * 
 * Consumes: An integer
 * Produces: Nothing
 */
int handleRedirection(int redirectionType, char *destFile) {
    int fileDescriptor;
    int abort = 0; // For letting the run executable function know to stop the command from running if noclobber is on
    int wrx = 0666; // Files in UNIX have read and write permissions for user, group, other by default
    struct stat buffer;
    int fileExists = (stat(destFile, &buffer) == 0) ? 1 : 0; // Check if the specified file already exists
    if (redirectionType == 0) { // If no redirection was detected
        abort = 0;
    } else if (redirectionType == 1) { // For ">"
        if (hasNoClobber && fileExists) {
            printf("noclobber is on, cannot overwrite %s, aborting...\n", destFile);
            abort = 1;
        } else {
            fileDescriptor = open(destFile, O_WRONLY|O_CREAT|O_TRUNC, wrx); // Give all permissions except execute
            close(1); // Close standard out
            dup(fileDescriptor);
            close(fileDescriptor);
        }
    } else if (redirectionType == 2) { // For ">>"
        if (hasNoClobber && fileExists) {
            printf("noclobber is on, cannot overwrite %s, aborting...\n", destFile);
            abort = 1;
        } else {
            fileDescriptor = open(destFile, O_WRONLY|O_CREAT|O_APPEND, wrx);
            close(1); // Close stdout
            dup(fileDescriptor);
            close(fileDescriptor);
        }
    } else if (redirectionType == 3) { // For "<"
        if (!fileExists) {
            printf("%s: %s", destFile, strerror(ENOENT));
            abort = 1;
        } else {
            fileDescriptor = open(destFile, O_RDONLY);
            close(0); // Close standard in
            dup(fileDescriptor);
            close(fileDescriptor);
        }
    } else if (redirectionType == 4) { // For ">>&"
        if (hasNoClobber && fileExists) {
            printf("noclobber is on, cannot overwrite %s, aborting...\n", destFile);
            abort = 1;
        } else {
            fileDescriptor = open(destFile, O_WRONLY|O_CREAT|O_APPEND, wrx);
            close(1); // Close stdout
            dup(fileDescriptor);
            close(2); // Close standard error
            dup(fileDescriptor);
            close(fileDescriptor);
        }
    } else if (redirectionType == 5) { // For ">&"
        if (hasNoClobber && fileExists) {
            printf("noclobber is on, cannot overwrite %s, aborting...\n", destFile);
            abort = 1;
        } else {
            fileDescriptor = open(destFile, O_WRONLY|O_CREAT|O_TRUNC, wrx);
            close(1); // Close stdout
            dup(fileDescriptor);
            close(2); // Close standard error
            dup(fileDescriptor);
            close(fileDescriptor);
        }
    }
    free(destFile);
    return abort;
}


// PIPES
/**
 * getPipeType, checks whether or not there is a pipe in the commandList. Returns an integer based on which type
 *              of pipe was entered by the user.
 *                  
 *              IMPORTANT: this function returns integers 0-2
 *                         -0: No pipe found
 *                         -1: "|"  found
 *                         -2: "|&" found
 * 
 * Consumes: An array of strings
 * Produces: An integer
 */
int getPipeType(char **commandList) {
    int hasPipe = 0;
    for (int i = 0; commandList[i] != NULL; i++) {
        if (!strcmp(commandList[i], "|"))
            hasPipe = 1;
        if (!strcmp(commandList[i], "|&"))
            hasPipe = 2;
    }
    return hasPipe;
}


/**
 * getPipeIndex, small helper function for getting the index in the commandList at which the pipe exists.
 *               Returns -1 if there is no pipe.
 * 
 * Consumes: An array of strings
 * Produces: An integer
 */
int getPipeIndex(char **commandList) {
    for (int i = 0; commandList[i] != NULL; i++)
        if (strstr(commandList[i], "|"))
            return i;
    return -1;
}


/**
 * splitPipe, This function splits the command list before or after where the pipe lies. If the beforeOrAfter parameter
 *            is non-zero, this function returns the commandList before the pipe, if it is zero, this function returns
 *            the commandList after the pipe.
 * 
 * Consumes: An array of strings, an integer
 * Produces: An array of strings
 */
char **splitPipe(char **commandList, int beforeOrAfter) { // 1 for before the pipe, 0 for after the pipe
    char **pipeList = calloc(MAX_CMD, sizeof(char *));
    int pipeIndex = getPipeIndex(commandList);
    if (beforeOrAfter) { // Before
        for (int i = 0; i < pipeIndex; i++) {
            pipeList[i] = (char *)malloc((int)strlen(commandList[i])+1);
            strcpy(pipeList[i], commandList[i]); // Fuck
        }
    } else {             // After
        int saveIndex = 0;
        for (int i = pipeIndex+1; commandList[i] != NULL; i++) {
            pipeList[saveIndex] = (char *)malloc((int)strlen(commandList[i])+1);
            strcpy(pipeList[saveIndex], commandList[i]); 
            saveIndex++;
        }
    }
    return pipeList;
}


/**
 * handlePipes, handles the logic for piping. If the pipeType function returns 0, that means there is no pipe and this function
 *              does nothing. If the pipe exists, then pipe(2) is called to set a file descriptor array and the file descriptors
 *              for stdin, stdout, and stderr are opened/closed appropriately. There are two calls the the runExecutable function
 *              here, the first call executes the command given before the pipe, stdout and stderr are returned to the terminal 
 *              and then the second call to runExecutable is made. This time, runExecutable runs the command that comes after the
 *              pipe.  
 * 
 * Consumes: Three arrays of strings, A struct
 * Produces: An integer
 */
int handlePipes(char **commandList, char **envp, struct pathelement *pathList, char **argv) {
    int fileDescriptor, before = 1, after = 0, wasPiped = 0, pipeType = getPipeType(commandList), pipeFileDescriptor[2];
    char **beforePipe = splitPipe(commandList, before), **afterPipe = splitPipe(commandList, after);
    if (pipeType) {
        if (pipe(pipeFileDescriptor) != 0) {
            perror("pipe");
        }
    //--------------------------------------
        close(0); // Close stdin
        dup(pipeFileDescriptor[0]);
        close(pipeFileDescriptor[0]); // Send stdin to pipe
    //--------------------------------------
        if (pipeType == 2) { // getPipeType returns 2 if the pipe is a "|&"
            close(2);
            dup(pipeFileDescriptor[1]); // Send standard error to pipe
        }
    //--------------------------------------
        close(1); // Close stdout
        dup(pipeFileDescriptor[1]);
        close(pipeFileDescriptor[1]); // Send stdout to pipe
    //--------------------------------------
        runExecutable(beforePipe, envp, pathList, argv); // First call to exec, command output is sent to the pipe
    //--------------------------------------
        fileDescriptor = open("/dev/tty", O_WRONLY); // Open stdout back to shell
        close(1);
        dup(fileDescriptor);
        close(fileDescriptor);
    //--------------------------------------
        fileDescriptor = open("/dev/tty", O_WRONLY); // Open stderr back to shell
        close(2);
        dup(fileDescriptor);
        close(fileDescriptor);
    //--------------------------------------
        runExecutable(afterPipe, envp, pathList, argv); // Second call to exec, output of the second command is sent to stdout, stderr if applicable
    //--------------------------------------
        fileDescriptor = open("/dev/tty", O_RDONLY); // Open stdin back to shell
        close(0);
        dup(fileDescriptor);
        close(fileDescriptor);
    //--------------------------------------
        wasPiped = 1;
    }
    freePipeArrays(beforePipe, afterPipe);
    return wasPiped;
} 


/**
 * freePipeArrays, frees the two arrays allocated for piping
 * 
 * Consumes: Two arrays of strings
 * Produces: Nothing
 */
void freePipeArrays(char **beforePipe, char **afterPipe) {
    // Free the string arrays sent to execve after all the function calls return
    for (int i = 0; afterPipe[i] != NULL; i++) {
        free(afterPipe[i]);
    }
    free(afterPipe);
    for (int i = 0; beforePipe[i] != NULL; i++) {
        free(beforePipe[i]);
    }
    free(beforePipe);
}



// END PIPES AND REDIRECTION
//*****************************************************************************************************************************


// BUILT IN COMMAND FUNCTIONS
//*****************************************************************************************************************************


/**
 * noClobber, for managing the hasNoClobber global variable. When noclobber is entered as a command, the hasNoClobber global will
 *            be updated to turn it on or off. It is off(0) by default, so typing noclobber would turnit on, typing it again would
 *            turn it off.
 * 
 * Consumes: Nothing
 * Produces: Nothing
 */
void noClobber() {
    if (hasNoClobber == 0) { // On
        hasNoClobber = 1;
        printf("noclobber is now on\n");
    }
    else {                   // Off
        hasNoClobber = 0;
        printf("noclobber is now off\n");
    }
}


/**
 * watchMail, if no arguments are given, an error message is printed. With one argument given, this function spawns 
 *            a thread that calls the watchMailCallback function which watches the given file in an infinite loop for
 *            updates to the file, read more about that function in the description for it. If two arguments are given,
 *            if the second argument is the word "off", then the thread is killed and all of its memory is collected.
 * 
 * Consumes: An array of strings
 * Produces: Nothing
 */
void watchMail(char **commandList) {
    char *fileName = strdup(commandList[1]);
    if (commandList[1] == NULL) { //  Case where no arguments are given
        errno = ENOENT;
        perror("watchmail");
    } else if (commandList[2] != NULL) { // Case where user enters "off" to stop watching mail
        if (strcmp("off", commandList[2]) == 0) {
            struct mail *toRemove = findMail(commandList[1]);
            free(fileName); // Free the strdup
            if (toRemove == NULL) { // If user tries to use the off option on mail not in the watchlist
                printf("Cannot unwatch %s, not in mail list.\n", commandList[1]);
                return;
            }
            pthread_cancel(toRemove->thread);
            pthread_join(toRemove->thread, NULL);
            removeMail(toRemove->pathToFile); // Remove from linked list
        } else {
            errno = EINVAL;
            perror("watchmail");
        }
    } else { // Case where one argument was entered to watch certain mail. 
        struct stat buffer;
        pthread_t mailID;
        int fileExists;
        if ((fileExists = stat(commandList[1], &buffer)) != 0) {
            errno = ENOENT;
            perror("stat");
            return;
        }
        pthread_create(&mailID, NULL, watchMailCallback, (void *)fileName);
        addMail(fileName, mailID); // Must use strdup to copy the string in order for it to remain properly in memory.
    }                              // Of course, this means more frees to deal with later
 }


/**
 * watchUser, if no argumemnts are given, print an error message. If one argument is given, add the user given as 
 *            the argument to the global users list. There is an optional second argument for untracking a user. 
 *            by typing: watchuser USER off, that user will be removed from the user list.
 * 
 * Consumes: An array of strings
 * Produces: Nothing
 */
void watchUser(char **commandList) {
    char *username = strdup(commandList[1]);
    if (commandList[1] == NULL) { // Case where no argument is given
        errno = EINVAL;
        printf("Enter a username to track: %s\n", strerror(errno));
    } else if (commandList[2] != NULL) { // Case where two arguments, (username, off) are given
        if (strcmp("off", commandList[2]) == 0) {
            struct user *toRemove = findUser(username);
            free(username); // Cleanup after strdup
            if (toRemove == NULL) {
                printf("Cannot remove, no such user\n");
                return;
            }
            pthread_mutex_lock(&mutexLock);
            removeUser(toRemove->username);
            pthread_mutex_unlock(&mutexLock);
        } else {
            errno = EINVAL;
            perror("watchuser");
        }
    } else { // Case where one argument is given (username) to track
        pthread_mutex_lock(&mutexLock);
        addUser(username);
        pthread_mutex_unlock(&mutexLock);
        if (!threadExists) {
            threadExists = 1;
            if (pthread_create(&watchUserID, NULL, watchUserCallback, NULL) != 0) {
                perror("pthread_create problem");
            }
        }
    }
}


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
        if (strcmp("PATH", commandList[1]) == 0) { // The pathlist linked list gets freed from the main loop
            setenv("PATH", commandList[2], 1); // Make new path
            pathChanged = 1; // Return this
        } else {
            setenv(commandList[1], commandList[2], 1); // Should overwrite existing environment variables
        }
    } else { // Case where called with one argument
        if (strcmp(commandList[1], "PATH") == 0) { // Case where user changes path to empty string
            setenv("PATH", "", 1);
            pathChanged = 1;
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
void changeDirectory(char **commandList) {
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
        if (prefix != NULL) // If a new prompt is made, get rid of the old one if it exists
            free(prefix);
        prefix = (char *)malloc(sizeof(commandList)+1); // Thanks valgrind
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
        if (prefix != NULL) // If a new prompt is made, get rid of the old one if it exists
            free(prefix);
        prefix = (char *)malloc(strlen(tempBuffer)+1); // Thanks valgrind
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
            perror("opendir");
            exit(errno);
        } 
        while ((dirp = readdir(dp)) != NULL) { // traverse files in opened directories
            if (strcmp(dirp->d_name, command) == 0) { // If command is found do some string copying then return
                if (access(pathlist->element, X_OK) == 0) { // Make sure to only return the executable command
                    strcpy(temp, pathlist->element);
                    strcat(temp, "/"); // Special character in UNIX
                    strcat(temp, dirp->d_name); // Concatenate full path name
                    char *executablePath = (char *)malloc(strlen(temp)+1); // +1 --> thanks valgrind
                    strcpy(executablePath, temp); // dest, src
                    closedir(dp);
                    return executablePath; // Exits function
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
            perror(" Error opening");
            return NULL;
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
} 


// END BUILT-IN COMMAND FUNCTIONS
//*****************************************************************************************************************************


//*****************************************************************************************************************************
// CONVIENIENCE FUNCTIONS


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
            if (pathToCmd != NULL) {
                printf(" %s\n", pathToCmd);
                free(pathToCmd);
            }
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
    free(path); // From getPath.h
} 


/**
 * handleInvalidArguments, if no argument was given for the time limit of
 *                         the program, or the argument could not be converted
 *                         to an integer with atoi, exit the program with a perror
 *                         message.
 * 
 * Consumes: A string
 * Produces: Nothing
 */
void handleInvalidArguments(char *arg) {
    if (arg == NULL) { // If no argument was given
        errno = EINVAL;
        perror("Specify time limit");
        exit(0); // Exit without error
    } 
    for (int i = 0; i < strlen(arg); i++) { // If an invalid time limit was given
        if (arg[i] < 48 || arg[i] > 57) { // If the input contains alphabetic characters, invalid
            errno = EINVAL;
            perror("Time limit");
            exit(0); // Exit without error
        }
    }
}


/**
 * freeAndExit, this function gets called when exit is typed to exit. Frees all of the things that are still taking
 *              up space and exits the program.
 * 
 * Consumes: A struct, An array of strings
 * Produces: Nothing
 */ 
void freeAndExit(struct pathelement *pathList, char **commandList) {
    if (prefix) // there may be no prefix, must check if it exists
        free(prefix);
    freePath(pathList);
    free(myCwd);
    freeUsers(userHead);
    pthread_cancel(watchUserID);
    pthread_join(watchUserID, NULL);
    freeAllMail(mailHead);
    free(commandList[0]); // Freeing the command list elements, literally just frees the "exit" command typed
    free(commandList);
    exit(0); // Exit without error
} 


// END CONVIENIENCE FUNCTIONS
//*****************************************************************************************************************************