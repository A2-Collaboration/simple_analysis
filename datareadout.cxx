/*********************************************************************
*  main.cpp
*
*  Minimal driver that uses the header‑only DataReader class.
*
*  Features:
*      - Constructs DataReader with the file name supplied on the CLI
*      - Calls readFile() to build the internal structures
*      - Runs a simple prompt:
*            p <value>   – lookup by first_param
*            q <value>   – lookup by second_param
*            m           – show maximum M-suffixes for both parameters
*            f           – show maximum M-suffix for first_param
*            s           – show maximum M-suffix for second_param
*            x           – quit
*
*  All I/O is done with printf/sscanf/fgets (no iostreams).
*********************************************************************/
#include "datareader.h"          // <-- our header from above

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <data-file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    /* -------------------------------------------------------------
     *  1. Build the DataReader object
     * ------------------------------------------------------------- */
    DataReader dr(argv[1]);

    /* -------------------------------------------------------------
     *  2. Parse the file – abort if we cannot read it
     * ------------------------------------------------------------- */
    if (!dr.readFile())
        return EXIT_FAILURE;

    /* -------------------------------------------------------------
     *  3. Print summary of loaded data
     * ------------------------------------------------------------- */
    int n = dr.getNumberOfElements();
    printf("Successfully loaded %d elements.\n", n);

    /* -------------------------------------------------------------
     *  4. Interactive query loop (pure C‑style I/O)
     * ------------------------------------------------------------- */
    char query[256];
    while (true) {
        printf("\nEnter query (p <1st_param>, q <2nd_param>, ");
        printf("m/f/s for max M-suffixes, x to quit): ");
        if (!fgets(query, sizeof query, stdin))
            break;                 // EOF

        /* Skip leading whitespace */
        char *p = query;
        while (*p && isspace((unsigned char)*p)) ++p;

        /* Quit? */
        if (*p == 'x' || *p == 'X')
            break;

        /* Command character */
        char cmd = *p;

        if (cmd == 'p' || cmd == 'P') {
            int value;
            if (sscanf(p + 1, "%d", &value) != 1) {
                printf("  Invalid number – try again.\n");
                continue;
            }
            Record *r = dr.findFirst(value);
            if (r)
                printf("  Found: id=%d (1st_param=%d, 2nd_param=%d)\n",
                       r->id, r->first_param, r->second_param);
            else
                printf("  1st_param=%d not found.\n", value);
        }
        else if (cmd == 'q' || cmd == 'Q') {
            int value;
            if (sscanf(p + 1, "%d", &value) != 1) {
                printf("  Invalid number – try again.\n");
                continue;
            }
            Record *r = dr.findSecond(value);
            if (r)
                printf("  Found: id=%d (1st_param=%d, 2nd_param=%d)\n",
                       r->id, r->first_param, r->second_param);
            else
                printf("  2nd_param=%d not found.\n", value);
        }
        else if (cmd == 'm' || cmd == 'M') {
            int maxFirst  = dr.getMaxFirstSuffix();
            int maxSecond = dr.getMaxSecondSuffix();
            printf("  Max M-suffixes: first_param = %d, second_param = %d\n",
                   maxFirst, maxSecond);
        }
        else if (cmd == 'f' || cmd == 'F') {
            int maxFirst = dr.getMaxFirstSuffix();
            printf("  Max M-suffix for first_param: %d\n", maxFirst);
        }
        else if (cmd == 's' || cmd == 'S') {
            int maxSecond = dr.getMaxSecondSuffix();
            printf("  Max M-suffix for second_param: %d\n", maxSecond);
        }
        else {
            printf("  Unknown command – use 'p', 'q', 'm', 'f', 's', or 'x'.\n");
        }
    }

    printf("\nAll resources freed – exiting\n");
    return EXIT_SUCCESS;
}
