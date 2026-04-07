#include "ConfigManager.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

int main(int argc, char *argv[])
{
    if (argc != 2) {
        printf("Usage: %s <config-file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    ConfigManager cfg;

    if (!cfg.readConfig(argv[1])) {
        printf("Configuration failed\n");
        return EXIT_FAILURE;
    }

    cfg.printSummary();

    char query[256];

    while (1) {
        printf("\nEnter query (a <ADC>, t <TDC>, s <Scaler>, "
               "m/f/d/c, q to quit): ");

        if (!fgets(query, sizeof query, stdin))
            break;

        char *p = query;
        while (*p && isspace((unsigned char)*p))
            ++p;

        if (*p == 'q' || *p == 'Q')
            break;

        char cmd = *p;

        if (cmd == 'a' || cmd == 'A') {
            int value;
            if (sscanf(p + 1, "%d", &value) != 1) {
                printf("  Invalid number\n");
                continue;
            }
            int id = cfg.get(D_CB).findADC(value);
            if (id >= 0) printf("  Found: id=%d (ADC=%d)\n", id, value);
            else if (id == -1) printf("  ADC=%d not found\n", value);
            else printf("  No data loaded\n");
        }
        else if (cmd == 't' || cmd == 'T') {
            int value;
            if (sscanf(p + 1, "%d", &value) != 1) {
                printf("  Invalid number\n");
                continue;
            }
            int id = cfg.get(D_CB).findTDC(value);
            if (id >= 0) printf("  Found: id=%d (TDC=%d)\n", id, value);
            else if (id == -1) printf("  TDC=%d not found\n", value);
            else printf("  No data loaded\n");
        }
        else if (cmd == 's' || cmd == 'S') {
            int value;
            if (sscanf(p + 1, "%d", &value) != 1) {
                printf("  Invalid number\n");
                continue;
            }
            int id = cfg.get(D_CB).findScaler(value);
            if (id >= 0) printf("  Found: id=%d (Scaler=%d)\n", id, value);
            else if (id == -1) printf("  Scaler=%d not found\n", value);
            else printf("  No data loaded\n");
        }
        else if (cmd == 'm' || cmd == 'M') {
            printf("  Max M suffixes: ADC=%d, TDC=%d, Scaler=%d\n",
                   cfg.get(D_CB).getMaxADCSuffix(),
                   cfg.get(D_CB).getMaxTDCSuffix(),
                   cfg.get(D_CB).getMaxScalerSuffix());
        }
        else if (cmd == 'f' || cmd == 'F') {
            printf("  Max ADC M-suffix: %d\n",
                   cfg.get(D_CB).getMaxADCSuffix());
        }
        else if (cmd == 'd' || cmd == 'D') {
            printf("  Max TDC M-suffix: %d\n",
                   cfg.get(D_CB).getMaxTDCSuffix());
        }
        else if (cmd == 'c' || cmd == 'C') {
            printf("  Max Scaler M-suffix: %d\n",
                   cfg.get(D_CB).getMaxScalerSuffix());
        }
        else {
            printf("  Unknown command\n");
        }
    }

    printf("\nAll resources freed – exiting\n");
    return EXIT_SUCCESS;
}
