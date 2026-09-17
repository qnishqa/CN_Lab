#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define MAX_DATA 32

typedef struct { int seq; char data[MAX_DATA]; } packet_t;
typedef struct { char type[4]; int ack; } ack_t;

int main(int argc, char *argv[]) {
    if (argc < 2) { printf("Usage: %s <port>\n", argv[0]); return 1; }
    setvbuf(stdout, NULL, _IONBF, 0);

    int port = atoi(argv[1]);
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    bind(sockfd, (struct sockaddr *)&addr, sizeof(addr));

    int expected_seq = 0; // GBN receiver only ever accepts packets in order
    printf("[RECEIVER-GBN] Listening on port %d...\n", port);

    while (1) {
        packet_t pkt;
        struct sockaddr_in from; socklen_t len = sizeof(from);
        recvfrom(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&from, &len);

        if (pkt.seq == expected_seq) {
            printf("[RECEIVER-GBN] seq=%d in order, delivered\n", pkt.seq);
            expected_seq++;
        } else {
            // anything not exactly the next expected seq is simply
            // dropped -- that's the defining trait of Go-Back-N
            printf("[RECEIVER-GBN] seq=%d out of order (expected %d), discarded\n",
                   pkt.seq, expected_seq);
        }

        // always re-send the cumulative ACK for the highest in-order
        // packet delivered so far, even if this packet was discarded
        ack_t ack;
        strcpy(ack.type, "ACK");
        ack.ack = expected_seq - 1;
        sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)&from, len);
        printf("[RECEIVER-GBN] sent cumulative ACK=%d\n\n", ack.ack);
    }
}
