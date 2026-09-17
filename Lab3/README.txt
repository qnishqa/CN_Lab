Lab Assignment 3 - TCP Client-Server Integer Statistics Service
=================================================================

Compile:
    gcc -Wall -o server server.c
    gcc -Wall -o client client.c

Run (in two separate terminals, server first):
    Terminal 1: ./server <port>
                e.g. ./server 5050

    Terminal 2: ./client <server_ip> <server_port>
                e.g. ./client 127.0.0.1 5050

The server keeps running and accepting new clients until you stop it
with Ctrl+C.
