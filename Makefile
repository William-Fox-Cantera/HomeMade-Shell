CC=gcc -w

mysh: sh.o lists.o main.c 
	$(CC) -g main.c sh.o lists.o -o mysh -lpthread

sh.o: sh.c sh.h
	$(CC) -g -c sh.c

lists.o: lists.c lists.h 
	$(CC) -g -c lists.c

clean:
	rm -rf sh.o lists.o mysh
