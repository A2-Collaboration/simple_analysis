/*********************************************************************
 *  main.cpp
 *
 *  Minimal driver that uses the header-only DataReader class.
 *
 *  Features:
 *      - Constructs DataReader with the file name supplied on the CLI
 *      - Calls readFile() to build the internal structures
 *      - Runs a simple prompt:
 *            a <value>   - lookup by ADC
 *            t <value>   - lookup by TDC
 *            s <value>   - lookup by Scaler
 *            m           - show maximum M-suffixes for ADC, TDC and Scaler
 *            f           - show maximum M-suffix for ADC
 *            d           - show maximum M-suffix for TDC
 *            c           - show maximum M-suffix for Scaler
 *            q           - quit
 *
 *  All I/O is done with printf/sscanf/fgets (no iostreams).
 *********************************************************************/

#include "datareader.h"      // Header-only implementation (ASCII only)

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <data-file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    /* -------------------------------------------------------------
     *  1. Build the DataReader object
     * ------------------------------------------------------------- */
    /* Default token positions (the same as the original implementation):
     *   ADC    - token 1
     *   TDC    - token 6
     *   Scaler - token 11
     * The *_number arguments are currently unused but required by the
     * constructor signature, so we pass 0 for each.
     */
    DataReader dr(argv[1],   // filename
                  1, 0,      // ADC position, ADC number (unused)
                  6, 0,      // TDC position, TDC number (unused)
                  0, 0);    // Scaler position, Scaler number (unused)

    /* -------------------------------------------------------------
     *  2. Parse the file - abort if we cannot read it
     * ------------------------------------------------------------- */
    if (!dr.readFile())
        return EXIT_FAILURE;

    /* -------------------------------------------------------------
     *  3. Print summary of loaded data
     * ------------------------------------------------------------- */
    int n = dr.getNumberOfElements();
    printf("Successfully loaded %d elements.\n", n);

    /* -------------------------------------------------------------
     *  4. Interactive query loop (pure C-style I/O)
     * ------------------------------------------------------------- */
    char query[256];
    while (true) {
        printf("\nEnter query (a <ADC>, t <TDC>, s <Scaler>, ");
        printf("m/f/d/c for max M-suffixes, q to quit): ");
        if (!fgets(query, sizeof query, stdin))
            break;                 /* EOF */

        /* Skip leading whitespace */
        char *p = query;
        while (*p && isspace((unsigned char)*p)) ++p;

        /* Quit? */
        if (*p == 'q' || *p == 'Q')
            break;

        char cmd = *p;

        if (cmd == 'a' || cmd == 'A') {
            int value;
            if (sscanf(p + 1, "%d", &value) != 1) {
                printf("  Invalid number - try again.\n");
                continue;
            }
            Record *r = dr.findADC(value);
            if (r)
                printf("  Found: id=%d (ADC=%d, TDC=%d, Scaler=%d)\n",
                       r->id, r->adc, r->tdc, r->scaler);
            else
                printf("  ADC=%d not found.\n", value);
        }
        else if (cmd == 't' || cmd == 'T') {
            int value;
            if (sscanf(p + 1, "%d", &value) != 1) {
                printf("  Invalid number - try again.\n");
                continue;
            }
            Record *r = dr.findTDC(value);
            if (r)
                printf("  Found: id=%d (ADC=%d, TDC=%d, Scaler=%d)\n",
                       r->id, r->adc, r->tdc, r->scaler);
            else
                printf("  TDC=%d not found.\n", value);
        }
        else if (cmd == 's' || cmd == 'S') {
            int value;
            if (sscanf(p + 1, "%d", &value) != 1) {
                printf("  Invalid number - try again.\n");
                continue;
            }
            Record *r = dr.findScaler(value);
            if (r)
                printf("  Found: id=%d (ADC=%d, TDC=%d, Scaler=%d)\n",
                       r->id, r->adc, r->tdc, r->scaler);
            else
                printf("  Scaler=%d not found.\n", value);
        }
        else if (cmd == 'm' || cmd == 'M') {
            int maxADC    = dr.getMaxADCSuffix();
            int maxTDC    = dr.getMaxTDCSuffix();
            int maxScaler = dr.getMaxScalerSuffix();
            printf("  Max M-suffixes: ADC=%d, TDC=%d, Scaler=%d\n",
                   maxADC, maxTDC, maxScaler);
        }
        else if (cmd == 'f' || cmd == 'F') {
            int maxADC = dr.getMaxADCSuffix();
            printf("  Max M-suffix for ADC: %d\n", maxADC);
        }
        else if (cmd == 'd' || cmd == 'D') {
            int maxTDC = dr.getMaxTDCSuffix();
            printf("  Max M-suffix for TDC: %d\n", maxTDC);
        }
        else if (cmd == 'c' || cmd == 'C') {
            int maxScaler = dr.getMaxScalerSuffix();
            printf("  Max M-suffix for Scaler: %d\n", maxScaler);
        }
        else {
            printf("  Unknown command - use 'a', 't', 's', 'm', 'f', 'd', 'c', or 'x'.\n");
        }
    }

    printf("\nAll resources freed - exiting\n");
    return EXIT_SUCCESS;
}