#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define MAX_DATA 1024

typedef struct {
    int seq;
    char data[MAX_DATA];
} packet_t;

typedef struct {
    char type[4]; // "ACK"
    int seq;
} ack_t;

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <port>\n", argv[0]);
        return 1;
    }

    setvbuf(stdout, NULL, _IONBF, 0); // print immediately instead of buffering

    int port = atoi(argv[1]);
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }

    int expected_seq = 0; // next seq number we're willing to deliver
    int ack_count = 0;    // counts every ack we would send, used to drop every 5th one

    printf("[RECEIVER] Listening on port %d...\n\n", port);

    while (1) {
        packet_t pkt;
        struct sockaddr_in sender_addr;
        socklen_t len = sizeof(sender_addr);

        int r = recvfrom(sockfd, &pkt, sizeof(pkt), 0,
                          (struct sockaddr *)&sender_addr, &len);
        if (r <= 0) continue;

        if (pkt.seq == expected_seq) {
            printf("[RECEIVER] Packet seq=%d data=\"%s\" -> delivered to app\n",
                   pkt.seq, pkt.data);
            expected_seq = 1 - expected_seq;
        } else {
            // same seq number as last time we delivered => sender must have
            // missed our ACK and retransmitted. Ack it again, don't deliver again.
            printf("[RECEIVER] Duplicate packet seq=%d received, discarding (already delivered)\n",
                   pkt.seq);
        }

        ack_count++;

        // deliberately drop every 5th ack to demonstrate the sender's timeout/retransmit
        if (ack_count % 5 == 0) {
            printf("[RECEIVER] (simulated loss) dropping ACK #%d for seq=%d\n\n",
                   ack_count, pkt.seq);
            continue;
        }

        ack_t ack;
        strcpy(ack.type, "ACK");
        ack.seq = pkt.seq;
        sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)&sender_addr, len);
        printf("[RECEIVER] ACK sent for seq=%d\n\n", pkt.seq);
    }

    close(sockfd);
    return 0;
}
