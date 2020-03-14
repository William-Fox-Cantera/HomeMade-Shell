#include <stdio.h>
#include <utmpx.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>



// getPath definitions
//*********************************************************************************************


char *path; // I made this global so I can free it from sh.c

/* function prototype.  It returns a pointer to a linked list for the path
   elements. */
struct pathelement *get_path();

struct pathelement
{
  char *element;			/* a dir in the path */
  struct pathelement *next;		/* pointer to next node */
};


// End getPath definitions
//************************************************************************************************
// watchUser definitions


// Struct definition
struct user {
    int isLoggedOn; // 1 if logged on, 0 if not logged on
    char *username; // The user to be watched, or unwatched
    struct user *next; // Pointer to the next userNode
}*head;


// Typical linked list functions
struct user *addUser(char *username);
struct user *findUser(char *username);
struct user *removeUser(char *usernameToRemove);
void freeUsers(struct user *list);