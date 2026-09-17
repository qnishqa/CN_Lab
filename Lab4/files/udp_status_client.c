/*
 * udp_status_client.c -- UDP client for the STATUS service (Problem 2)
 *
 * Build:  gcc -Wall -o udp_status_client udp_status_client.c
 * Run:    ./udp_status_client <server-ip>
 *         ./udp_status_client            (defaults to 127.0.0.1)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define UDP_PORT  9091
#define BUF_SIZE  256

int main(int argc, char *argv[]) {
    const char *server_ip = (argc > 1) ? argv[1] : "127.0.0.1";

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) { perror("socket"); exit(1); }

    struct sockaddr_in servaddr = {0};
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(UDP_PORT);
    if (inet_pton(AF_INET, server_ip, &servaddr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid server IP: %s\n", server_ip);
        exit(1);
    }

    const char *msg = "STATUS";
    if (sendto(sockfd, msg, strlen(msg), 0,
               (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("sendto");
        exit(1);
    }

    char buf[BUF_SIZE];
    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);
    ssize_t n = recvfrom(sockfd, buf, sizeof(buf) - 1, 0,
                          (struct sockaddr *)&from, &fromlen);
    if (n < 0) { perror("recvfrom"); exit(1); }
    buf[n] = '\0';

    printf("Response: %s\n", buf);

    close(sockfd);
    return 0;
}
