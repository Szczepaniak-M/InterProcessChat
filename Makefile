CC = gcc
CFLAGS= -Wall

all: server client

server: server.c structures.h
	$(CC) $(CFLAGS) server.c -o server

client: client.c structures.h
	$(CC) $(CFLAGS) client.c -o client


