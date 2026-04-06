/*********************************************************************
 * paramreadout.cxx
 *
 * Minimal driver that uses the header‑only DataReader class.
 *
 * Features:
 *   - Constructs a DataReader with the file name supplied on the CLI
 *   - Calls readFile() to build the internal structures
 *   - Runs a simple prompt:
 *         a <value>   - lookup by ADC
 *         t <value>   - lookup by TDC
 *         s <value>   - lookup by Scaler
 *         m           - show maximum M‑suffixes for ADC, TDC and Scaler
 *         f           - show maximum M‑suffix for ADC
 *         d           - show maximum M‑suffix for TDC
 *         c           - show maximum M‑suffix for Scaler
 *         q           - quit
 *
 * All I/O is done with printf / sscanf / fgets (no iostreams).
 *********************************************************************/

#include "paramreader.h"      // Header only implementation
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>           // isspace()

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <data-file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    /* -------------------------------------------------------------
     * 1. Build the ParamReader object
     * ------------------------------------------------------------- */
    /* Default token positions (the same as the original implementation):conf
     *   ADC    - token 1
     *   TDC    - token 6
     *   Scaler - token 11
     * The “number” arguments are unused but required by the ctor,
     * so we pass 0 for each.
     */
    ParamReader pr(argv[1],   /* filename */
                  1, 0,      /* ADC position, ADC number (unused) */
                  6, 0,      /* TDC position, TDC number (unused) */
                 11, 0);     /* Scaler position, Scaler number (unused) */

    /* -------------------------------------------------------------
     * 2. Parse the file – abort if we cannot read it
     * ------------------------------------------------------------- */
    if (!pr.readFile())
        return EXIT_FAILURE;

    /* -------------------------------------------------------------
     * 3. Print a short summary
     * ------------------------------------------------------------- */
    int n = pr.getNumberOfElements();
    printf("Successfully loaded %d elements.\n", n);

    /* -------------------------------------------------------------
     * 4. Interactive query loop (pure C‑style I/O)
     * ------------------------------------------------------------- */
    char query[256];
    while (1) {
        printf("\nEnter query (a <ADC>, t <TDC>, s <Scaler>, "
               "m/f/d/c for max M-suffixes, q to quit): ");

        if (!fgets(query, sizeof query, stdin))
            break;                     /* EOF */

        /* Skip leading whitespace */
        char *p = query;
        while (*p && isspace((unsigned char)*p))
            ++p;

        /* Quit? */
        if (*p == 'q' || *p == 'Q')
            break;

        char cmd = *p;

        /* ----- ADC lookup ------------------------------------------------ */
        if (cmd == 'a' || cmd == 'A') {
            int value;
            if (sscanf(p + 1, "%d", &value) != 1) {
                printf("  Invalid number – try again.\n");
                continue;
            }
            int id = pr.findADC(value);
            if (id >= 0)
                printf("  Found: id=%d (ADC=%d)\n", id, value);
            else
                printf("  ADC=%d not found.\n", value);
        }
        /* ----- TDC lookup ------------------------------------------------ */
        else if (cmd == 't' || cmd == 'T') {
            int value;
            if (sscanf(p + 1, "%d", &value) != 1) {
                printf("  Invalid number – try again.\n");
                continue;
            }
            int id = pr.findTDC(value);
            if (id >= 0)
                printf("  Found: id=%d (TDC=%d)\n", id, value);
            else
                printf("  TDC=%d not found.\n", value);
        }
        /* ----- Scaler lookup --------------------------------------------- */
        else if (cmd == 's' || cmd == 'S') {
            int value;
            if (sscanf(p + 1, "%d", &value) != 1) {
                printf("  Invalid number – try again.\n");
                continue;
            }
            int id = pr.findScaler(value);
            if (id >= 0)
                printf("  Found: id=%d (Scaler=%d)\n", id, value);
            else
                printf("  Scaler=%d not found.\n", value);
        }
        /* ----- Max M suffixes -------------------------------------------- */
        else if (cmd == 'm' || cmd == 'M') {
            printf("  Max M suffixes: ADC=%d, TDC=%d, Scaler=%d\n",
                   pr.getMaxADCSuffix(),
                   pr.getMaxTDCSuffix(),
                   pr.getMaxScalerSuffix());
        }
        else if (cmd == 'f' || cmd == 'F') {
            printf("  Max M-suffix for ADC: %d\n", pr.getMaxADCSuffix());
        }
        else if (cmd == 'd' || cmd == 'D') {
            printf("  Max M-suffix for TDC: %d\n", pr.getMaxTDCSuffix());
        }
        else if (cmd == 'c' || cmd == 'C') {
            printf("  Max M-suffix for Scaler: %d\n", pr.getMaxScalerSuffix());
        }
        else {
            printf("  Unknown command – use a/t/s/m/f/d/c/q.\n");
        }
    }

    printf("\nAll resources freed – exiting\n");
    return EXIT_SUCCESS;
}
