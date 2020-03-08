/* 
  get_path.h
  Ben Miller

*/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

char *path; // I made this global so I can free it from sh.c

/* function prototype.  It returns a pointer to a linked list for the path
   elements. */
struct pathelement *get_path();

struct pathelement
{
  char *element;			/* a dir in the path */
  struct pathelement *next;		/* pointer to next node */
};

