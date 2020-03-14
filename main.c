#include "sh.h"

int main( int argc, char **argv, char **envp ) {
  /* put signal set up stuff here */
  signal(SIGCHLD, childHandler);
  sigignore(SIGTSTP);
  sigignore(SIGTERM);
  signal(SIGINT, sigHandler);


  /* // LINKED LIST TEST --> IT WORKS DELETE THIS LATER
  head = NULL;
  addUser("will");
  addUser("sam");
  addUser("jimmy");
  addUser("Holly"); 
  removeUser("will");
  struct user *n = head;

  while (n) {
    printf("NAME: %s\n", n->username);
    n = n->next;
  }

  struct user *will = findUser("jimmy");
  if (will != NULL)
    printf("FOUND: %s\n", will->username);
  else
    printf("NO SUCH USER\n");
  
  freeUsers(head);
  */
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