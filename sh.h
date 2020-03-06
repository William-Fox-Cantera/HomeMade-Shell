#include <signal.h> 
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
#include <errno.h>
#include <glob.h>
#include "get_path.h"


int pid;

// MAIN SHELL FUNCTION
int sh(int argc, char **argv, char **envp);

// HELPER FUNCTIONS
void runExecutable(char **commandList, char **envp, struct pathelement *pathList, int status);
int isBuiltIn(char *command); 
void runBuiltIn(char *commandList[], struct pathelement *pathList, char **envp);
void sig_handler(int signal); 

// BUILT IN COMMAND FUNCTIONS
char *which(char *command, struct pathelement *pathList);
char **where(char *command, struct pathelement *pathList);
void list (char *dir);
void printenv(char **envp);
void printWorkingDirectory();
void prompt(char *commandList[]);
void exitProgram();
void printPid();
void changeDirectory(char *commandList[]);
void printEnvironment(char **commandList, char **envp); 
void setEnvironment(char **commandList, char **envp);
void killIt(char **commandList);

// CONVIENIENCE FUNCTIONS
void printShell();
void listHandler(char **commandList);
void whichHandler(char **commandList, struct pathelement *pathList);
void whereHandler(char **commandList, struct pathelement *pathList);

// CONSTANTS
#define PROMPTMAX 32
#define MAXARGS 10
#define BUFFERSIZE 512
#define BUILT_IN_COMMAND_COUNT 11
#define MAX_COMMAND_LOCATIONS 10
