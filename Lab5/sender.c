#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>

#define TIMEOUT_SEC 2
#define MAX_DATA 1024

// what one data packet looks like on the wire
typedef struct {
    int seq;
    char data[MAX_DATA];
} packet_t;

// what one ACK looks like on the wire
typedef struct {
    char type[4]; // "ACK"
    int seq;
} ack_t;

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <receiver_ip> <receiver_port>\n", argv[0]);
        return 1;
    }

    char *recv_ip = argv[1];
    int recv_port = atoi(argv[2]);

    setvbuf(stdout, NULL, _IONBF, 0); // print immediately instead of buffering

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    // without this, recvfrom() would block forever if an ACK never arrives
    struct timeval tv;
    tv.tv_sec = TIMEOUT_SEC;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in recv_addr;
    memset(&recv_addr, 0, sizeof(recv_addr));
    recv_addr.sin_family = AF_INET;
    recv_addr.sin_port = htons(recv_port);
    inet_pton(AF_INET, recv_ip, &recv_addr.sin_addr);

    int n;
    printf("Enter number of messages: ");
    scanf("%d", &n);
    getchar(); // eat the leftover newline from scanf

    int seq = 0; // alternates between 0 and 1

    for (int i = 0; i < n; i++) {
        packet_t pkt;
        pkt.seq = seq;

        printf("Enter message %d: ", i + 1);
        fgets(pkt.data, MAX_DATA, stdin);
        pkt.data[strcspn(pkt.data, "\n")] = 0; // strip trailing newline

        int acked = 0;
        while (!acked) {
            printf("[SENDER] Sending seq=%d  data=\"%s\"\n", pkt.seq, pkt.data);
            sendto(sockfd, &pkt, sizeof(pkt), 0,
                   (struct sockaddr *)&recv_addr, sizeof(recv_addr));

            ack_t ack;
            socklen_t len = sizeof(recv_addr);
            int r = recvfrom(sockfd, &ack, sizeof(ack), 0,
                              (struct sockaddr *)&recv_addr, &len);

            if (r < 0) {
                // recvfrom timed out -> no ACK arrived in time
                printf("[SENDER] Timeout waiting for ACK of seq=%d, retransmitting\n", pkt.seq);
                continue;
            }

            if (strncmp(ack.type, "ACK", 3) == 0 && ack.seq == seq) {
                printf("[SENDER] ACK received for seq=%d\n\n", ack.seq);
                acked = 1;
            } else {
                printf("[SENDER] Got an unexpected/old ACK, ignoring it\n");
            }
        }

        seq = 1 - seq; // flip 0 <-> 1 for the next packet
    }

    printf("[SENDER] All %d messages delivered successfully.\n", n);
    close(sockfd);
    return 0;
}
