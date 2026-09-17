CC = gcc
CFLAGS = -Wall

all: server client udp_status_client

server: server.c
	$(CC) $(CFLAGS) -pthread -o server server.c

client: client.c
	$(CC) $(CFLAGS) -o client client.c

udp_status_client: udp_status_client.c
	$(CC) $(CFLAGS) -o udp_status_client udp_status_client.c

clean:
	rm -f server client udp_status_client server.log

.PHONY: all clean
