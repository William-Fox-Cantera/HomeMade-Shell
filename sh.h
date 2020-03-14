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
#include <sys/time.h>
#include <sys/stat.h>
#include "get_path.h"

// MAIN SHELL FUNCTION
int sh(int argc, char **argv, char **envp);

// HELPER FUNCTIONS
int shouldRunAsBackground(char **commandList);
int runCommand(char **commandList, struct pathelement *pathList, char **argv, char **envp, char *cwd);
void runExecutable(char **commandList, char **envp, struct pathelement *pathList, char **argv);
int isBuiltIn(char *command); 
int runBuiltIn(char *commandList[], struct pathelement *pathList, char **envp);
void sigHandler(int signal);
void childHandler(int signal);
void alarmHandler(int);
char *getExternalPath(char **commandList, struct pathelement *pathList);

// BUILT IN COMMAND FUNCTIONS
char *which(char *command, struct pathelement *pathList);
char *where(char *command, struct pathelement *pathList);
void list (char *dir);
void printenv(char **envp);
void printWorkingDirectory();
void prompt(char *commandList[]);
int exitProgram();
void printPid();
void changeDirectory(char *commandList[]);
void printEnvironment(char **commandList, char **envp); 
int setEnvironment(char **commandList, char **envp, struct pathelement *pathList);
void killIt(char **commandList);

// CONVIENIENCE FUNCTIONS
void printShell();
void listHandler(char **commandList);
void whichHandler(char **commandList, struct pathelement *pathList);
void whereHandler(char **commandList, struct pathelement *pathList);
void freeAll(struct pathelement *pathList, char *cwd);
void freePath(struct pathelement *pathList);
void handleInvalidArguments(char *arg);

// CONSTANTS
#define PROMPTMAX 32
#define MAXARGS 10
#define BUFFERSIZE 512
#define BUILT_IN_COMMAND_COUNT 11