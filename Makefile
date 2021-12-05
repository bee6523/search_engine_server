CC = gcc
CFLAGS = -Isrc/include -g -pthread

SERVER_SRC = src/server.c src/csapp.c src/utils.c src/linkedlist.c src/invidx.c src/tc_malloc.c
SERVER_HDR = src/include/csapp.h src/include/utils.h src/include/linkedlist.h src/include/invidx.h src/include/tc_malloc.h

CLIENT_SUB = src/csapp.c src/utils.c src/tc_malloc.c
CLIENT2_SRC = src/client.c src/csapp.c src/utils.c
CLIENT_HDR = src/include/csapp.h src/include/utils.h src/include/tc_malloc.h

all : server client1 client2

server : $(SERVER_SRC) $(SERVER_HDR) obj
	$(CC) $(CFLAGS) -o $@ $(SERVER_SRC)

client1 : src/client_multi.c $(CLIENT_SUB) $(CLIENT_HDR) obj
	$(CC) $(CFLAGS) -o $@ src/client_multi.c $(CLIENT_SUB)

client2 : src/client.c $(CLIENT_SUB) $(CLIENT_HDR) obj
	$(CC) $(CFLAGS) -o $@ src/client.c $(CLIENT_SUB)

obj : 
	mkdir -p obj

clean :
	rm server client1 client2
	rm -rf obj