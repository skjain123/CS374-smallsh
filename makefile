all: main.o 
	gcc -g --std=gnu99 main.o  -o smallsh

main.o: main.c
	gcc -g --std=gnu99 -c -Wall main.c

clean:
	rm -f *.o smallsh