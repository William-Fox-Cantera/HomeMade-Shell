 # Shell Project CISC-361
 
 # Authors: William Cantera, Ben Segal
 # Student IDS: William --> (702464888), Ben --> (702425559)
 # Emails: wcantera@udel.edu, bensegal@udel.edu
 

CC=gcc -w

mysh: sh.o lists.o main.c 
	$(CC) -g main.c sh.o lists.o -o mysh -lpthread

sh.o: sh.c sh.h
	$(CC) -g -c sh.c

lists.o: lists.c lists.h 
	$(CC) -g -c lists.c

clean:
	rm -rf sh.o lists.o mysh