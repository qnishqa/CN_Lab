/*
server.c -- Concurrent Network Information Service
Problem 1: A multi-threaded TCP server that answers DNS-like lookup
           queries (hostname + record type -> value). One thread is
           spawned per connected client; a client can issue several
           queries before disconnecting by sending EXIT.
Problem 2: A UDP server, running concurrently in its own thread on a
           second port, that answers a simple "STATUS" request with
           the number of currently-connected TCP clients.
Build:  gcc -Wall -pthread -o server server.c
Run:    ./server
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>     /* strcasecmp */
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define TCP_PORT      9090
#define UDP_PORT      9091
#define BACKLOG       16
#define BUF_SIZE      256

/* ---------- DNS-like record table ---------------------------------- */

typedef struct {
    const char *hostname;
    const char *type;      /* A, CNAME, MX, NS */
    const char *value;
} record_t;

static record_t records[] = {
    { "www.example.com",  "A",     "192.168.1.10"     },
    { "mail.example.com", "A",     "192.168.1.20"      },
    { "web.example.com",  "CNAME", "www.example.com"   },
    { "example.com",      "MX",    "mail.example.com"  },
    { "example.com",      "NS",    "ns1.example.com"   },
};
#define NUM_RECORDS (sizeof(records) / sizeof(records[0]))

/* ---------- Shared state (connected client count) ------------------ */

static int client_count = 0;
static pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER;

static void increment_clients(void) {
    pthread_mutex_lock(&count_mutex);
    client_count++;
    pthread_mutex_unlock(&count_mutex);
}

static void decrement_clients(void) {
    pthread_mutex_lock(&count_mutex);
    client_count--;
    pthread_mutex_unlock(&count_mutex);
}

static int get_client_count(void) {
    int n;
    pthread_mutex_lock(&count_mutex);
    n = client_count;
    pthread_mutex_unlock(&count_mutex);
    return n;
}

/* ---------- Lookup logic -------------------------------------------- */

/* Looks up hostname+type in the table. Writes result into out (must be
 * at least BUF_SIZE bytes). */
static void lookup_record(const char *hostname, const char *type, char *out) {
    for (size_t i = 0; i < NUM_RECORDS; i++) {
        if (strcasecmp(records[i].hostname, hostname) == 0 &&
            strcasecmp(records[i].type, type) == 0) {
            snprintf(out, BUF_SIZE, "%s", records[i].value);
            return;
        }
    }
    snprintf(out, BUF_SIZE, "Record not found");
}

/* ---------- TCP per-client thread ----------------------------------- */

typedef struct {
    int sockfd;
    struct sockaddr_in addr;
} client_info_t;

/* Reads a single newline-terminated line from a socket (TCP is a byte
stream, so we can't assume one recv() == one line). Returns number of
bytes read into buf (excluding the trailing NUL), or <=0 on
disconnect/error. */
static ssize_t read_line(int sockfd, char *buf, size_t maxlen) {
    size_t total = 0;
    while (total < maxlen - 1) {
        char c;
        ssize_t n = recv(sockfd, &c, 1, 0);
        if (n <= 0) return n;          /* disconnected or error */
        if (c == '\n') break;
        if (c != '\r') buf[total++] = c;
    }
    buf[total] = '\0';
    return (ssize_t)total;
}

static void *handle_client(void *arg) {
    client_info_t *info = (client_info_t *)arg;
    int sockfd = info->sockfd;
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &info->addr.sin_addr, ip, sizeof(ip));
    int port = ntohs(info->addr.sin_port);

    increment_clients();
    printf("[+] Client connected: %s:%d (active clients: %d)\n",
           ip, port, get_client_count());

    char line[BUF_SIZE];
    char response[BUF_SIZE];

    while (1) {
        ssize_t n = read_line(sockfd, line, sizeof(line));
        if (n <= 0) break;                       /* client closed / error */
        if (strcasecmp(line, "EXIT") == 0) break; /* client asked to stop */
        if (n == 0) continue;                     /* empty line, ignore */

        /* Expected format: "<hostname> <type>" */
        char hostname[128] = {0}, type[16] = {0};
        if (sscanf(line, "%127s %15s", hostname, type) != 2) {
            snprintf(response, sizeof(response), "Invalid request format");
        } else {
            lookup_record(hostname, type, response);
        }

        strncat(response, "\n", sizeof(response) - strlen(response) - 1);
        if (send(sockfd, response, strlen(response), 0) < 0) break;

        printf("    %s:%d queried \"%s\" -> %s", ip, port, line, response);
    }

    close(sockfd);
    decrement_clients();
    printf("[-] Client disconnected: %s:%d (active clients: %d)\n",
           ip, port, get_client_count());

    free(info);
    return NULL;
}

/* ---------- UDP status thread ---------------------------------------- */

static void *udp_status_server(void *arg) {
    (void)arg;
    int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock < 0) { perror("udp socket"); return NULL; }

    struct sockaddr_in servaddr = {0};
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(UDP_PORT);

    if (bind(udp_sock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("udp bind");
        close(udp_sock);
        return NULL;
    }

    printf("[UDP] Status service listening on port %d\n", UDP_PORT);

    char buf[BUF_SIZE];
    while (1) {
        struct sockaddr_in cliaddr;
        socklen_t clilen = sizeof(cliaddr);
        ssize_t n = recvfrom(udp_sock, buf, sizeof(buf) - 1, 0,
                              (struct sockaddr *)&cliaddr, &clilen);
        if (n < 0) continue;
        buf[n] = '\0';

        /* trim trailing CR/LF just in case */
        while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) buf[--n] = '\0';

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &cliaddr.sin_addr, ip, sizeof(ip));

        if (strcasecmp(buf, "STATUS") == 0) {
            char reply[BUF_SIZE];
            snprintf(reply, sizeof(reply), "Server active. Connected clients: %d",
                      get_client_count());
            sendto(udp_sock, reply, strlen(reply), 0,
                   (struct sockaddr *)&cliaddr, clilen);
            printf("[UDP] STATUS from %s:%d -> %s\n",
                   ip, ntohs(cliaddr.sin_port), reply);
        } else {
            const char *reply = "Unknown command";
            sendto(udp_sock, reply, strlen(reply), 0,
                   (struct sockaddr *)&cliaddr, clilen);
        }
    }
    /* unreachable */
}

/* ---------- main: TCP accept loop ------------------------------------ */

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);   /* flush each printed line immediately */

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) { perror("socket"); exit(1); }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in servaddr = {0};
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(TCP_PORT);

    if (bind(listen_fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind"); exit(1);
    }
    if (listen(listen_fd, BACKLOG) < 0) { perror("listen"); exit(1); }

    printf("[TCP] Lookup service listening on port %d\n", TCP_PORT);

    /* Start the UDP status service concurrently */
    pthread_t udp_tid;
    pthread_create(&udp_tid, NULL, udp_status_server, NULL);
    pthread_detach(udp_tid);

    while (1) {
        struct sockaddr_in cliaddr;
        socklen_t clilen = sizeof(cliaddr);
        int connfd = accept(listen_fd, (struct sockaddr *)&cliaddr, &clilen);
        if (connfd < 0) { perror("accept"); continue; }

        client_info_t *info = malloc(sizeof(client_info_t));
        info->sockfd = connfd;
        info->addr = cliaddr;

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, info) != 0) {
            perror("pthread_create");
            close(connfd);
            free(info);
            continue;
        }
        pthread_detach(tid);   /* thread cleans up its own resources */
    }

    close(listen_fd);
    return 0;
}
