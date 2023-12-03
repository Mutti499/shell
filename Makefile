myshell: main.o
	gcc main.o -o myshell

main.o: main.c
	gcc -c main.c

clean:
	rm -f myshell main.o
