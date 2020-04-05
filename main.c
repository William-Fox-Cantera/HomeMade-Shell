#include "sh.h"

int main( int argc, char **argv, char **envp ) {
  signal(SIGCHLD, childHandler);
  sigignore(SIGTSTP); // Ignore ctrl+z
  sigignore(SIGTERM); // Ignore ctrl+z
  signal(SIGINT, sigHandler); // Handle ctrl+c --> shell should ignore, running processes should be killed by it
  sh(argc, argv, envp);
  return 0;
}

/**
 * sig_handler, handles when ctrl+c is pressed. It will kill the running child, 
 *              otherwise it is ignored.
 * 
 * Consumes: An integer
 * Produces: Nothing
 */
void sigHandler(int signal) {
  /* define your signal handler */
  if (signal == SIGINT) { // For ctrl+c
    printf(" Interrupt\n");
    printShell();
    fflush(stdout);
  }
}