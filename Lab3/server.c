#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define BACKLOG 5

typedef struct {
    int min;
    int max;
    long sum;
    double avg;
} StatsResult;

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);

    /* 1. Create the listening (welcome) socket */
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    /* Allow immediate reuse of the port after the server restarts,
     * otherwise you often get "Address already in use". */
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));


    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;  
    server_addr.sin_port        = htons(port);

    if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(listen_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(listen_fd, BACKLOG) < 0) {
        perror("listen");
        close(listen_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d ...\n", port);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("accept");
            continue;   /* try again instead of dying */
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        int client_port = ntohs(client_addr.sin_port);

        printf("\nAccepted connection from %s:%d\n", client_ip, client_port);


        int n;
        ssize_t r = read(client_fd, &n, sizeof(n));
        if (r <= 0) {
            printf("Client %s:%d disconnected before sending N.\n", client_ip, client_port);
            close(client_fd);
            continue;
        }
        n = ntohl(n);   /* convert from network byte order */

        if (n <= 0) {
            fprintf(stderr, "Invalid N (%d) from %s:%d, closing connection.\n",
                    n, client_ip, client_port);
            close(client_fd);
            continue;
        }

  
        int *values = malloc(n * sizeof(int));
        if (!values) {
            perror("malloc");
            close(client_fd);
            continue;
        }

        int received = 0;
        while (received < n) {
            int net_val;
            r = read(client_fd, &net_val, sizeof(net_val));
            if (r <= 0) break;              /* client closed early */
            values[received++] = ntohl(net_val);
        }

        if (received < n) {
            printf("Client %s:%d disconnected while sending data (%d/%d received).\n",
                   client_ip, client_port, received, n);
            free(values);
            close(client_fd);
            continue;
        }

        /* --- Display the received integers --- */
        printf("Received %d integers from %s:%d: ", n, client_ip, client_port);
        for (int i = 0; i < n; i++) printf("%d ", values[i]);
        printf("\n");

        /* --- Compute statistics --- */
        StatsResult result;
        result.min = values[0];
        result.max = values[0];
        result.sum = 0;

        for (int i = 0; i < n; i++) {
            if (values[i] < result.min) result.min = values[i];
            if (values[i] > result.max) result.max = values[i];
            result.sum += values[i];
        }
        result.avg = (double)result.sum / n;

        free(values);

        printf("Minimum = %d, Maximum = %d, Sum = %ld, Average = %.2f\n",
               result.min, result.max, result.sum, result.avg);

        StatsResult net_result;
        net_result.min = htonl(result.min);
        net_result.max = htonl(result.max);
        net_result.sum = htonl((int)result.sum);
        net_result.avg = result.avg;

        write(client_fd, &net_result, sizeof(net_result));

        close(client_fd);
        printf("Client %s:%d disconnected.\n", client_ip, client_port);
    }

    close(listen_fd);  
    return 0;
}
