#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>

#define BLOCK_SIZE 4096

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage: %s <server_ip> <port> <total_MB>\n", argv[0]);
        return 1;
    }
    setvbuf(stdout, NULL, _IONBF, 0);

    char *ip = argv[1];
    int port = atoi(argv[2]);
    double total_mb = atof(argv[3]);
    long total_bytes_to_send = (long)(total_mb * 1024 * 1024);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        return 1;
    }
    printf("[CLIENT] Connected to %s:%d\n", ip, port);
    printf("[CLIENT] Sending %.2f MB continuously (no delay on this side)\n", total_mb);

    char buf[BLOCK_SIZE];
    memset(buf, 'A', BLOCK_SIZE); // content is irrelevant, only volume matters here

    long sent = 0;
    struct timeval start, end;
    gettimeofday(&start, NULL);

    while (sent < total_bytes_to_send) {
        long remaining = total_bytes_to_send - sent;
        int to_send = remaining < BLOCK_SIZE ? (int)remaining : BLOCK_SIZE;

        // send() is blocking: once the receiver's advertised window
        // (and our own send buffer) fills up, this call itself will
        // stall until the server drains some data. That stall IS the
        // flow control in action -- we don't have to implement anything.
        int n = send(sockfd, buf, to_send, 0);
        if (n < 0) { perror("send"); break; }
        sent += n;
    }

    gettimeofday(&end, NULL);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1e6;

    printf("[CLIENT] Total bytes sent: %ld\n", sent);
    printf("[CLIENT] Total transfer time: %.3f sec\n", elapsed);
    if (elapsed > 0)
        printf("[CLIENT] Throughput: %.2f MB/s\n", (sent / (1024.0 * 1024.0)) / elapsed);

    close(sockfd);
    return 0;
}
