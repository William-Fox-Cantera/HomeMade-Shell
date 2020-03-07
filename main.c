#include "sh.h"

int main( int argc, char **argv, char **envp ) {
  /* put signal set up stuff here */
  sigignore(SIGTSTP);
  sigignore(SIGTERM);
  signal(SIGINT, sigHandler);
  return sh(argc, argv, envp);
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
