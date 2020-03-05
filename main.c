#include "sh.h"

int main( int argc, char **argv, char **envp ) {
  /* put signal set up stuff here */
  if (signal(SIGINT, sigint_handler) == SIG_ERR)
      printf(" Problem catching SIGINT\n");
  signal(SIGTSTP, sig_handler);
  signal(SIGTERM, sig_handler);
  return sh(argc, argv, envp);
}

void sigint_handler(int signal) {
  /* define your signal handler */
  //if(signal == SIGINT) 
      printf(" Interrupt\n");
  
}

