
#include "get_path.h"

int pid;

// MAIN SHELL FUNCTION
int sh( int argc, char **argv, char **envp);

// HELPER FUNCTIONS
int isBuiltIn(char *command); 
void runBuiltIn(char *commandList[], struct pathelement *pathList, char **envp);

// BUILT IN COMMAND FUNCTIONS
char *which(char *command, struct pathelement *pathList);
char *where(char *command, struct pathelement *pathList);
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
void listHandler(char *commandList[]);
void whichHandler(char *commandList[], struct pathelement *pathList);
void whereHandler(char *commandList[], struct pathelement *pathList);

// CONSTANTS
#define PROMPTMAX 32
#define MAXARGS 10
#define BUFFERSIZE 512
#define BUILT_IN_COMMAND_COUNT 11
