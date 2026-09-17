/*
client.c -- TCP client for the DNS-like lookup service (Problem 1)

Build:  gcc -Wall -o client client.c
Run:    ./client <server-ip>
        ./client            (defaults to 127.0.0.1)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>    /* strcasecmp */
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define TCP_PORT  9090
#define BUF_SIZE  256

static ssize_t read_line(int sockfd, char *buf, size_t maxlen) {
    size_t total = 0;
    while (total < maxlen - 1) {
        char c;
        ssize_t n = recv(sockfd, &c, 1, 0);
        if (n <= 0) return n;
        if (c == '\n') break;
        if (c != '\r') buf[total++] = c;
    }
    buf[total] = '\0';
    return (ssize_t)total;
}

/* Trims the trailing newline from a fgets() result, if present. */
static void chomp(char *s) {
    size_t len = strlen(s);
    if (len > 0 && s[len - 1] == '\n') s[len - 1] = '\0';
}

int main(int argc, char *argv[]) {
    const char *server_ip = (argc > 1) ? argv[1] : "127.0.0.1";

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) { perror("socket"); exit(1); }

    struct sockaddr_in servaddr = {0};
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(TCP_PORT);
    if (inet_pton(AF_INET, server_ip, &servaddr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid server IP: %s\n", server_ip);
        exit(1);
    }

    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("connect");
        exit(1);
    }
    printf("Connected to lookup server at %s:%d\n", server_ip, TCP_PORT);

    char domain[128], type[16], request[BUF_SIZE], response[BUF_SIZE];

    while (1) {
        printf("\nEnter domain (or EXIT to quit): ");
        if (!fgets(domain, sizeof(domain), stdin)) break;
        chomp(domain);

        if (strcasecmp(domain, "EXIT") == 0) {
            send(sockfd, "EXIT\n", 5, 0);
            break;
        }

        printf("Enter type (A/CNAME/MX/NS): ");
        if (!fgets(type, sizeof(type), stdin)) break;
        chomp(type);

        snprintf(request, sizeof(request), "%s %s\n", domain, type);
        if (send(sockfd, request, strlen(request), 0) < 0) {
            perror("send");
            break;
        }

        ssize_t n = read_line(sockfd, response, sizeof(response));
        if (n <= 0) {
            printf("Server closed the connection.\n");
            break;
        }
        printf("Server response: %s\n", response);
    }

    close(sockfd);
    printf("Disconnected.\n");
    return 0;
}
