#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define MAX_DATA 32
#define MAX_PKTS 2000

typedef struct { int seq; char data[MAX_DATA]; } packet_t;
typedef struct { char type[4]; int ack; } ack_t;

int main(int argc, char *argv[]) {
    if (argc < 3) { printf("Usage: %s <port> <window_size>\n", argv[0]); return 1; }
    setvbuf(stdout, NULL, _IONBF, 0);

    int port = atoi(argv[1]);
    int window_size = atoi(argv[2]);

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    bind(sockfd, (struct sockaddr *)&addr, sizeof(addr));

    int received[MAX_PKTS] = {0};
    char buf[MAX_PKTS][MAX_DATA];
    int rbase = 0; // left edge of the receive window

    printf("[RECEIVER-SR] Listening on port %d...\n", port);

    while (1) {
        packet_t pkt;
        struct sockaddr_in from; socklen_t len = sizeof(from);
        recvfrom(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&from, &len);

        if (pkt.seq >= rbase && pkt.seq < rbase + window_size) {
            if (!received[pkt.seq]) {
                received[pkt.seq] = 1;
                strncpy(buf[pkt.seq], pkt.data, MAX_DATA);
                if (pkt.seq == rbase)
                    printf("[RECEIVER-SR] seq=%d in order, buffered\n", pkt.seq);
                else
                    printf("[RECEIVER-SR] seq=%d out of order, buffered (waiting for %d)\n",
                           pkt.seq, rbase);
            } else {
                printf("[RECEIVER-SR] seq=%d duplicate, already buffered\n", pkt.seq);
            }

            // SR always ACKs individually, whatever arrives
            ack_t ack;
            strcpy(ack.type, "ACK");
            ack.ack = pkt.seq;
            sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)&from, len);
            printf("[RECEIVER-SR] sent individual ACK=%d\n", pkt.seq);

            // deliver every packet we can now that's in a contiguous
            // run starting at rbase, and slide the window forward
            while (received[rbase]) {
                printf("[RECEIVER-SR] delivering seq=%d data=\"%s\" to app, window slides to %d\n",
                       rbase, buf[rbase], rbase + 1);
                rbase++;
            }
            printf("\n");
        } else if (pkt.seq < rbase) {
            // this is an old packet the sender resent because our ACK
            // for it must have been lost -- just ACK it again
            ack_t ack;
            strcpy(ack.type, "ACK");
            ack.ack = pkt.seq;
            sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)&from, len);
            printf("[RECEIVER-SR] seq=%d already delivered earlier, re-ACKing\n\n", pkt.seq);
        }
        // seq beyond the receive window is simply ignored
    }
}
