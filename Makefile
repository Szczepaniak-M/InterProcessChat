CC = gcc
CFLAGS= -Wall

all: server client

server: Files/server.c Files/structures.h
	$(CC) $(CFLAGS) Files/server.c -o server

client: Files/client.c Files/structures.h
	$(CC) $(CFLAGS) Files/client.c -o client


