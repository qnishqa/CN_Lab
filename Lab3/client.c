
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>


typedef struct {
    int min;
    int max;
    long sum;
    double avg;
} StatsResult;

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_ip> <server_port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *server_ip = argv[1];
    int server_port = atoi(argv[2]);

    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(server_port);

    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid server IP address: %s\n", server_ip);
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    printf("Connected to server.\n");


    int n;
    printf("Enter N (number of integers): ");
    if (scanf("%d", &n) != 1 || n <= 0) {
        fprintf(stderr, "Invalid N.\n");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    int *values = malloc(n * sizeof(int));
    if (!values) {
        perror("malloc");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    printf("Enter %d integer value(s): ", n);
    for (int i = 0; i < n; i++) {
        if (scanf("%d", &values[i]) != 1) {
            fprintf(stderr, "Invalid input.\n");
            free(values);
            close(sock_fd);
            exit(EXIT_FAILURE);
        }
    }

    int net_n = htonl(n);
    write(sock_fd, &net_n, sizeof(net_n));

    for (int i = 0; i < n; i++) {
        int net_val = htonl(values[i]);
        write(sock_fd, &net_val, sizeof(net_val));
    }

    free(values);

    StatsResult net_result;
    ssize_t r = read(sock_fd, &net_result, sizeof(net_result));
    if (r != sizeof(net_result)) {
        fprintf(stderr, "Failed to receive complete results from server.\n");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    StatsResult result;
    result.min = ntohl(net_result.min);
    result.max = ntohl(net_result.max);
    result.sum = ntohl((int)net_result.sum);
    result.avg = net_result.avg;

    printf("Minimum = %d\n", result.min);
    printf("Maximum = %d\n", result.max);
    printf("Sum = %ld\n", result.sum);
    printf("Average = %.2f\n", result.avg);

    close(sock_fd);
    return 0;
}
