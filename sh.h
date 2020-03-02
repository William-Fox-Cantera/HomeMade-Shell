
#include "get_path.h"

int pid;
int sh( int argc, char **argv, char **envp);
int isBuiltIn(char *command); 
void runBuiltIn(char *commandList[], struct pathelement *pathList);

// BUILT IN COMMAND FUNCTIONS
char *which(char *command, struct pathelement *pathList);
char *where(char *command, struct pathelement *pathList);
void list ( char *dir );
void printenv(char **envp);
void printWorkingDirectory();
void prompt(char *str);
void exitProgram();
void printPid();


// CONVIENIENCE FUNCTIONS
void printShell();


// CONSTANTS
#define PROMPTMAX 32
#define MAXARGS 10
