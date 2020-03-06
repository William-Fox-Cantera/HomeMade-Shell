#include "sh.h"

int main( int argc, char **argv, char **envp ) {
  /* put signal set up stuff here */
  signal(SIGTSTP, sig_handler);
  signal(SIGINT, sig_handler);
  return sh(argc, argv, envp);
}

void sig_handler(int signal) {
  /* define your signal handler */
  if (signal == SIGTSTP) {
    printf(" Interrupt\n");
    printShell();
    fflush(stdout);
  }
  if (signal == SIGINT) {
    printf(" Interrupt\n");
    printShell();
    fflush(stdout);
  }
}

