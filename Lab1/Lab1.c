#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

// network parameters (fixed for all experiments)
double pktlen, bw1, bw2, delay1, delay2, procdelay;
int qcap;
long npkts;
unsigned int seed;

double bits, transdelay, propdelay, servtime;

void getInputs() {
    printf("Packet length (bytes): ");
    scanf("%lf", &pktlen);
    printf("Source-to-router bandwidth (bps): ");
    scanf("%lf", &bw1);
    printf("Router-to-destination bandwidth (bps): ");
    scanf("%lf", &bw2);
    printf("Source-to-router propagation delay (s): ");
    scanf("%lf", &delay1);
    printf("Router-to-destination propagation delay (s): ");
    scanf("%lf", &delay2);
    printf("Router processing delay (s): ");
    scanf("%lf", &procdelay);
    printf("Queue capacity (packets): ");
    scanf("%d", &qcap);
    printf("Packets per experiment: ");
    scanf("%ld", &npkts);
    printf("Random seed: ");
    scanf("%u", &seed);
}

double randu() {
    double u;
    do { u = (double)rand() / ((double)RAND_MAX + 1.0); } while (u <= 0.0);
    return u;
}

// runs the queue simulation for one lambda, prints a CSV row
void simulate(double rho, double lambda, FILE *fp) {
    srand(seed); // same seed each time so only rho changes
    double *leaveTime = malloc(sizeof(double) * qcap); // departure times of packets in the router
    int head = 0, tail = 0, inQueue = 0;
    double freeAt = 0, clock = 0;
    long delivered = 0, dropped = 0, maxQueue = 0;
    double qDelaySum = 0, e2eSum = 0;

    for (long i = 0; i < npkts; i++) {
        clock += -log(randu()) / lambda; // next packet generated
        double arrival = clock + bits / bw1 + delay1; // reaches router
        // drop packets that already left
        while (inQueue > 0 && leaveTime[head] <= arrival) {
            head = (head + 1) % qcap;
            inQueue--;
        }
        if (inQueue >= qcap) {
            dropped++;
            continue;
        }
        double start = arrival > freeAt ? arrival : freeAt;
        double qDelay = start - arrival;
        double leave = start + servtime;
        freeAt = leave;
        leaveTime[tail] = leave;
        tail = (tail + 1) % qcap;
        inQueue++;
        if (inQueue > maxQueue) maxQueue = inQueue;
        delivered++;
        qDelaySum += qDelay;
        e2eSum += transdelay + propdelay + procdelay + qDelay;
    }
    free(leaveTime);

    double dropProb = (double)dropped / npkts;
    double avgQ = delivered ? qDelaySum / delivered : 0;
    double avgE2E = delivered ? e2eSum / delivered : 0;

    printf("%-6.2f %-10.3f %-8ld %-8ld %-8ld %-9.4f %-12.6f %-12.6f %-6ld\n",
           rho, lambda, npkts, delivered, dropped, dropProb, avgQ, avgE2E, maxQueue);

    fprintf(fp, "%.2f,%.4f,%ld,%ld,%ld,%.6f,%.8f,%.8f,%ld\n",
            rho, lambda, npkts, delivered, dropped, dropProb, avgQ, avgE2E, maxQueue);
}

int main() {
    getInputs();

    bits = pktlen * 8;
    transdelay = bits / bw1 + bits / bw2;
    propdelay = delay1 + delay2;
    servtime = procdelay + bits / bw2; // time router spends per packet on the output link

    double rhoList[] = {0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 0.95, 1.0, 1.1, 1.2};

    FILE *fp = fopen("router_sim_results.csv", "w");
    fprintf(fp, "rho,lambda,generated,delivered,dropped,drop_prob,avg_queue_delay,avg_e2e_delay,max_queue\n");

    printf("%-6s %-10s %-8s %-8s %-8s %-9s %-12s %-12s %-6s\n",
           "rho", "lambda", "gen", "deliv", "drop", "P(drop)", "avgQdelay", "avgE2E", "maxQ");

    for (int i = 0; i < 13; i++) {
        double lambda = rhoList[i] * bw2 / bits; // only lambda changes between runs
        simulate(rhoList[i], lambda, fp);
    }

    fclose(fp);
    printf("\nWrote router_sim_results.csv\n");
    return 0;
}
