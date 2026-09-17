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
        printf("Usage: %s <port> <rcvbuf_bytes> <delay_ms>\n", argv[0]);
        return 1;
    }
    setvbuf(stdout, NULL, _IONBF, 0);

    int port = atoi(argv[1]);
    int rcvbuf = atoi(argv[2]);
    int delay_ms = atoi(argv[3]);

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // request a small receive buffer BEFORE accept() -- this needs to be
    // set on the listening socket so the connection inherits it during
    // the handshake, otherwise the OS may auto-tune to something bigger
    setsockopt(listenfd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(listenfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }
    listen(listenfd, 1);

    // the kernel doesn't give you exactly what you asked for (usually
    // doubles it, and enforces a minimum) -- print what we actually got
    int actual_rcvbuf;
    socklen_t optlen = sizeof(actual_rcvbuf);
    getsockopt(listenfd, SOL_SOCKET, SO_RCVBUF, &actual_rcvbuf, &optlen);
    printf("[SERVER] Requested SO_RCVBUF=%d bytes, kernel actually set %d bytes\n",
           rcvbuf, actual_rcvbuf);
    printf("[SERVER] Delay after every recv() = %d ms (simulates a slow app)\n", delay_ms);
    printf("[SERVER] Waiting for a client on port %d...\n", port);

    int connfd = accept(listenfd, NULL, NULL);
    printf("[SERVER] Client connected\n");

    char buf[BLOCK_SIZE];
    long total_bytes = 0;
    int n;
    struct timeval start, end;
    gettimeofday(&start, NULL);

    while ((n = recv(connfd, buf, BLOCK_SIZE, 0)) > 0) {
        total_bytes += n;
        printf("[SERVER] recv() got %d bytes (cumulative: %ld bytes)\n", n, total_bytes);

        // this is the "slow application" -- while we sleep here, the
        // receive buffer keeps filling from data already in flight,
        // which is exactly what shrinks TCP's advertised window
        if (delay_ms > 0) usleep(delay_ms * 1000);
    }

    gettimeofday(&end, NULL);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1e6;

    printf("\n[SERVER] Client closed the connection.\n");
    printf("[SERVER] Total bytes received: %ld\n", total_bytes);
    printf("[SERVER] Total time: %.3f sec\n", elapsed);

    close(connfd);
    close(listenfd);
    return 0;
}
