#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define MAX_DATA 32
#define MAX_PKTS 2000
#define TIMEOUT_SEC 2

typedef struct { int seq; char data[MAX_DATA]; } packet_t;
typedef struct { char type[4]; int ack; } ack_t;

int sockfd;
struct sockaddr_in recv_addr;
int loss_prob;
int total_sent = 0, total_retrans = 0;

void send_packet(int seq) {
    packet_t pkt;
    pkt.seq = seq;
    snprintf(pkt.data, MAX_DATA, "pkt-%d", seq);

    total_sent++;
    if (rand() % 100 < loss_prob) {
        printf("[SENDER] seq=%d -> (simulated loss, dropped in network)\n", seq);
        return;
    }
    printf("[SENDER] seq=%d sent\n", seq);
    sendto(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&recv_addr, sizeof(recv_addr));
}

int main(int argc, char *argv[]) {
    if (argc < 6) {
        printf("Usage: %s <ip> <port> <num_packets> <window_size> <loss_prob_percent> [seed]\n", argv[0]);
        return 1;
    }

    char *ip = argv[1];
    int port = atoi(argv[2]);
    int num_packets = atoi(argv[3]);
    int window_size = atoi(argv[4]);
    loss_prob = atoi(argv[5]);
    srand(argc > 6 ? atoi(argv[6]) : time(NULL));

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    memset(&recv_addr, 0, sizeof(recv_addr));
    recv_addr.sin_family = AF_INET;
    recv_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &recv_addr.sin_addr);

    int acked[MAX_PKTS] = {0};
    int base = 0, nextseqnum = 0;
    struct timeval start, end;
    gettimeofday(&start, NULL);

    while (base < num_packets) {
        while (nextseqnum < base + window_size && nextseqnum < num_packets) {
            send_packet(nextseqnum);
            nextseqnum++;
        }

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(sockfd, &fds);
        struct timeval tv = { TIMEOUT_SEC, 0 };

        int ready = select(sockfd + 1, &fds, NULL, NULL, &tv);

        if (ready > 0) {
            ack_t ack;
            struct sockaddr_in from; socklen_t len = sizeof(from);
            recvfrom(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)&from, &len);

            if (!acked[ack.ack]) {
                acked[ack.ack] = 1;
                printf("[SENDER] individual ACK=%d received\n", ack.ack);
            }

            // slide the window forward only while its left edge is acked
            while (base < num_packets && acked[base]) {
                base++;
            }
            printf("[SENDER] window base now at %d\n", base);
        } else {
            // timeout -> unlike GBN, we ONLY resend the packets in the
            // current window that haven't been individually acked yet
            printf("[SENDER] TIMEOUT, resending unacked packets in [%d..%d]\n",
                   base, nextseqnum - 1);
            for (int i = base; i < nextseqnum; i++) {
                if (!acked[i]) {
                    send_packet(i);
                    total_retrans++;
                }
            }
        }
    }

    gettimeofday(&end, NULL);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1e6;

    printf("\n[SENDER] Transfer complete (Selective Repeat).\n");
    printf("Total packet-sends attempted (incl. retransmissions): %d\n", total_sent);
    printf("Total retransmissions: %d\n", total_retrans);
    printf("Total transfer time: %.3f sec\n", elapsed);

    close(sockfd);
    return 0;
}
