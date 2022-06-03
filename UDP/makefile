all: server client

server: server.c sqlite3.c
	gcc server.c sqlite3.c -lpthread -ldl -lm -o server

client: client.o
	gcc -o client client.o

client.o: client.c
	gcc -c client.c
