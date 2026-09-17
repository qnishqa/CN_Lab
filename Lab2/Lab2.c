#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define NSRC 4 // 4 sources

// network parameters
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
    printf("Total packets per experiment (all sources combined): ");
    scanf("%ld", &npkts);
    printf("Random seed: ");
    scanf("%u", &seed);
}

double randu() {
    double u;
    do { u = (double)rand() / ((double)RAND_MAX + 1.0); } while (u <= 0.0);
    return u;
}

// merges NSRC independent Poisson sources into one arrival stream and
// runs them through the same FIFO router queue
void simulate(double rho, double lambdas[NSRC], FILE *fp) {
    srand(seed); // same seed every run so only the source rates change

    double nextGen[NSRC];
    for (int s = 0; s < NSRC; s++)
        nextGen[s] = -log(randu()) / lambdas[s]; // first packet from each source

    double *leaveTime = malloc(sizeof(double) * qcap);
    int head = 0, tail = 0, inQueue = 0;
    double freeAt = 0;
    long delivered = 0, dropped = 0, maxQueue = 0;
    double qDelaySum = 0, e2eSum = 0;

    for (long i = 0; i < npkts; i++) {
        // pick whichever source's next packet comes first
        int src = 0;
        for (int s = 1; s < NSRC; s++)
            if (nextGen[s] < nextGen[src]) src = s;

        double genTime = nextGen[src];
        nextGen[src] += -log(randu()) / lambdas[src]; // schedule that source's next one

        double arrival = genTime + bits / bw1 + delay1;

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

    double aggLambda = 0;
    for (int s = 0; s < NSRC; s++) aggLambda += lambdas[s];

    double dropProb = (double)dropped / npkts;
    double avgQ = delivered ? qDelaySum / delivered : 0;
    double avgE2E = delivered ? e2eSum / delivered : 0;

    printf("%-6.2f %-10.3f %-8ld %-8ld %-8ld %-9.4f %-12.6f %-12.6f %-6ld\n",
           rho, aggLambda, npkts, delivered, dropped, dropProb, avgQ, avgE2E, maxQueue);

    fprintf(fp, "%.2f,%.4f,%ld,%ld,%ld,%.6f,%.8f,%.8f,%ld\n",
            rho, aggLambda, npkts, delivered, dropped, dropProb, avgQ, avgE2E, maxQueue);
}

int main() {
    getInputs();

    bits = pktlen * 8;
    transdelay = bits / bw1 + bits / bw2;
    propdelay = delay1 + delay2;
    servtime = procdelay + bits / bw2;

    printf("%-6s %-10s %-8s %-8s %-8s %-9s %-12s %-12s %-6s\n",
           "rho", "lambda", "gen", "deliv", "drop", "P(drop)", "avgQdelay", "avgE2E", "maxQ");

    // --- part 1: four equal-rate sources, aggregate rho swept ---
    double rhoList[] = {0.2, 0.4, 0.6, 0.8, 0.9, 1.0, 1.2};
    int nrho = sizeof(rhoList) / sizeof(rhoList[0]);

    FILE *fp1 = fopen("multi_equal_sources.csv", "w");
    fprintf(fp1, "rho,agg_lambda,generated,delivered,dropped,drop_prob,avg_queue_delay,avg_e2e_delay,max_queue\n");

    printf("\n-- Part 1: four equal sources --\n");
    for (int i = 0; i < nrho; i++) {
        double aggLambda = rhoList[i] * bw2 / bits;
        double each = aggLambda / NSRC;
        double lambdas[NSRC] = {each, each, each, each};
        simulate(rhoList[i], lambdas, fp1);
    }
    fclose(fp1);

    // --- part 2: four sources with different (but same-total) rates ---
    double configs[4][NSRC] = {
        {100, 100, 100, 700},
        {150, 150, 250, 450},
        {250, 250, 250, 250},
        {400, 300, 200, 100}
    };

    FILE *fp2 = fopen("multi_mixed_sources.csv", "w");
    fprintf(fp2, "rho,agg_lambda,generated,delivered,dropped,drop_prob,avg_queue_delay,avg_e2e_delay,max_queue\n");

    printf("\n-- Part 2: uneven source rates, same total --\n");
    for (int c = 0; c < 4; c++) {
        double aggLambda = 0;
        for (int s = 0; s < NSRC; s++) aggLambda += configs[c][s];
        double rho = aggLambda * bits / bw2;
        simulate(rho, configs[c], fp2);
    }
    fclose(fp2);

    printf("\nWrote multi_equal_sources.csv and multi_mixed_sources.csv\n");
    return 0;
}
