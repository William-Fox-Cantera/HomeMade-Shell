
#include "get_path.h"

int pid;
int sh( int argc, char **argv, char **envp);
int isBuiltIn(char *command); 
void runBuiltIn(char *commandList[], struct pathelement *pathList, char **envp);

// BUILT IN COMMAND FUNCTIONS
char *which(char *command, struct pathelement *pathList);
char *where(char *command, struct pathelement *pathList);
void list ( char *dir );
void printenv(char **envp);
void printWorkingDirectory();
void prompt(char *str);
void exitProgram();
void printPid();
void changeDirectory(char *commandList[]);
void printEnvironment(char **commandList, char **envp); 


// CONVIENIENCE FUNCTIONS
void printShell();
void listHandler(char *commandList[]);


// CONSTANTS
#define PROMPTMAX 32
#define MAXARGS 10
