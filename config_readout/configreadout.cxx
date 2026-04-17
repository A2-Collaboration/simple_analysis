#include "ConfigManager.h"
#include <stdio.h>      /* Standard I/O */
#include <stdlib.h>     /* Exit functions */
#include <ctype.h>      /* Character classification */
#include <string.h>     /* String manipulation */

/*
 * main.cpp
 * 
 * The main program for the configuration tool.
 * 
 * This program:
 * 1. Reads a configuration file
 * 2. Provides an interactive interface to query the configuration
 * 3. Supports testing TDC reference channel functionality
 * 
 * Usage:
 *   ./config_tool <config-file>
 * 
 * Interactive commands:
 *   d <det>       - Switch to detector <det> (by enum value)
 *   a <ADC>       - Query ADC channel mapping
 *   t <TDC>       - Query TDC channel mapping
 *   s <Scaler>    - Query Scaler channel mapping
 *   m/f/d/c       - Show max M-suffix values
 *   r <chan>      - Check reference channel for TDC channel
 *   store_ref <id> <data> - Store reference data
 *   l             - List available detectors
 *   q             - Quit
 */

int main(int argc, char *argv[])
{
  /* Check command-line arguments */
  if (argc != 2) {
    printf("Usage: %s <config-file>\n", argv[0]);
    return EXIT_FAILURE;
  }

  /* Create and initialize the configuration manager */
  ConfigManager cfg;
  if (!cfg.readConfig(argv[1], 10)) {  /* Verbosity level 10 */
    printf("Configuration failed\n");
    return EXIT_FAILURE;
  }

  /* Print configuration summary */
  cfg.printSummary();

  /* 
   * Select a detector for queries
   * Default is TAGGER, but if not configured, pick the first available detector
   */
  int det = D_TAGGER;
  if (!cfg.isDetectorInUse(det)) {
    printf("\nWARNING: TAGGER detector not configured.\n");
    for (int i = 0; i < N_DETECTORS; ++i) {
      if (cfg.isDetectorInUse(i)) {
        det = i;
        printf("Switching to %s (id=%d) for interactive queries.\n",
               cfg.getDetectorName(i), i);
        break;
      }
    }
  }
  printf("\nCurrent detector: %s\n", cfg.getDetectorName(det));

  /* Main interactive loop */
  char query[256];
  while (1) {
    /* Display command prompt */
    printf("\nEnter query (d <det>, a <ADC>, t <TDC>, s <Scaler>,\n"
           "               m/f/d/c, r <chan> (ref), store_ref <id> <data>, q to quit): ");

    /* Read user input */
    if (!fgets(query, sizeof query, stdin)) break;

    /* Skip leading whitespace */
    char *p = query;
    while (*p && isspace((unsigned char)*p)) ++p;
    
    /* Check for quit command */
    if (*p == 'q' || *p == 'Q') break;

    /* Extract command character */
    char cmd = *p;

    /* 
     * Command: d <det> - Switch to a different detector
     * Example: d 5 (switches to CB detector)
     */
    if (cmd == 'd' || cmd == 'D') {
      int newdet;
      /* Parse detector ID */
      if (sscanf(p + 1, "%d", &newdet) != 1 ||
          newdet < 0 || newdet >= N_DETECTORS) {
        printf("  Invalid detector ID\n");
        continue;
      }
      /* Check if detector is configured */
      if (!cfg.isDetectorInUse(newdet)) {
        printf("  Detector %s not configured\n", cfg.getDetectorName(newdet));
        continue;
      }
      /* Switch to new detector */
      det = newdet;
      printf("  Now querying %s\n", cfg.getDetectorName(det));
      continue;
    }

    /* 
     * Command: l - List available detectors
     */
    if (cmd == 'l' || cmd == 'L') {
      printf("Available detectors:\n");
      for (int i = 0; i < N_DETECTORS; ++i) {
        if (cfg.isDetectorInUse(i))
          printf("  %d: %s (%d elements)\n",
                 i, cfg.getDetectorName(i), cfg.getnoCh(i));
      }
      continue;
    }

    /* 
     * Command: store_ref <id> <data> - Store reference data
     * Example: store_ref 50 12345
     */
    if (strncmp(p, "store_ref", 9) == 0) {
      int id; 
      unsigned int data;
      /* Parse command */
      if (sscanf(p + 9, "%d %u", &id, &data) != 2) {
        printf("  Usage: store_ref <ref_id> <data>\n");
        continue;
      }
      /* Store the reference data */
      cfg.store_ref_data(id, data);
      continue;
    }

    /* 
     * Command: r <chan> - Check reference channel for TDC channel
     * Example: r 150
     */
    if (cmd == 'r' || cmd == 'R') {
      int chan;
      /* Parse channel number */
      if (sscanf(p + 1, "%d", &chan) != 1) {
        printf("  Invalid channel number\n");
        continue;
      }
      /* Get reference channel */
      int ref = cfg.getTDC_ref(chan);
      printf("  Channel %d -> Reference channel %d\n", chan, ref);
      /* If reference channel exists, check for stored data */
      if (ref != -1) {
        unsigned int d = cfg.get_ref_data(chan);
        if (d != UINT_MAX)
          printf("  Reference data: %u\n", d);
        else
          printf("  No reference data stored yet\n");
      }
      continue;
    }

    /* 
     * Command: a <ADC> - Query ADC channel mapping
     * Example: a 100
     */
    if (cmd == 'a' || cmd == 'A') {
      int adc;
      /* Parse ADC channel */
      if (sscanf(p + 1, "%d", &adc) != 1) {
        printf("  Invalid number\n");
        continue;
      }
      /* Get logical ID for physical ADC channel */
      int logical = cfg.getADC_id(det, adc);
      /* Handle results */
      if (logical >= 0) {
        printf("  Physical ADC %d → logical ID %d\n", adc, logical);
        /* Verify reverse mapping (logical → physical) */
        int phys = cfg.getADC_ch(det, logical);
        if (phys == adc)
          printf("  Reverse mapping verified\n");
        else
          printf("  WARNING: reverse mapping mismatch (got %d)\n", phys);
      } 
      /* Channel not found */
      else if (logical == -1) {
        printf("  ADC %d not found in detector %s\n", adc, cfg.getDetectorName(det));
      } 
      /* No ADC data loaded */
      else {
        printf("  No ADC data loaded for detector %s\n", cfg.getDetectorName(det));
      }
      continue;
    }

    /* 
     * Command: t <TDC> - Query TDC channel mapping
     * Example: t 200
     */
    if (cmd == 't' || cmd == 'T') {
      int tdc;
      /* Parse TDC channel */
      if (sscanf(p + 1, "%d", &tdc) != 1) {
        printf("  Invalid number\n");
        continue;
      }
      /* Get logical ID for physical TDC channel */
      int logical = cfg.getTDC_id(det, tdc);
      /* Handle results */
      if (logical >= 0) {
        printf("  Physical TDC %d → logical ID %d\n", tdc, logical);
        /* Verify reverse mapping */
        int phys = cfg.getTDC_ch(det, logical);
        if (phys == tdc)
          printf("  Reverse mapping verified\n");
        else
          printf("  WARNING: reverse mapping mismatch (got %d)\n", phys);

        /* Check if this channel uses a reference channel */
        int ref = cfg.getTDC_ref(tdc);
        if (ref != -1) {
          printf("  Channel %d uses reference channel %d\n", tdc, ref);
          unsigned int d = cfg.get_ref_data(tdc);
          if (d != UINT_MAX) printf("  Ref data = %u\n", d);
        }
      } 
      /* Channel not found */
      else if (logical == -1) {
        printf("  TDC %d not found in detector %s\n", tdc, cfg.getDetectorName(det));
      } 
      /* No TDC data loaded */
      else {
        printf("  No TDC data loaded for detector %s\n", cfg.getDetectorName(det));
      }
      continue;
    }

    /* 
     * Command: s <Scaler> - Query Scaler channel mapping
     * Example: s 0
     */
    if (cmd == 's' || cmd == 'S') {
      int scal;
      /* Parse Scaler channel */
      if (sscanf(p + 1, "%d", &scal) != 1) {
        printf("  Invalid number\n");
        continue;
      }
      /* Get logical ID for physical Scaler channel */
      int logical = cfg.getScaler_id(det, scal);
      /* Handle results */
      if (logical >= 0) {
        printf("  Physical Scaler %d → logical ID %d\n", scal, logical);
        /* Verify reverse mapping */
        int phys = cfg.getScaler_ch(det, logical);
        if (phys == scal)
          printf("  Reverse mapping verified\n");
        else
          printf("  WARNING: reverse mapping mismatch (got %d)\n", phys);
      } 
      /* Channel not found */
      else if (logical == -1) {
        printf("  Scaler %d not found in detector %s\n", scal, cfg.getDetectorName(det));
      } 
      /* No Scaler data loaded */
      else {
        printf("  No Scaler data loaded for detector %s\n", cfg.getDetectorName(det));
      }
      continue;
    }

    /* 
     * Command: m - Show all max M-suffix values
     */
    if (cmd == 'm' || cmd == 'M') {
      printf("  Max M suffixes: ADC=%d, TDC=%d, Scaler=%d\n",
             cfg.getADCMaxHits(det) - 1,
             cfg.getTDCMaxHits(det) - 1,
             cfg.getSCALERMaxHits(det) - 1);
      continue;
    }
    /* Command: f - Show max ADC M-suffix */
    if (cmd == 'f' || cmd == 'F') {
      printf("  Max ADC  M‑suffix: %d\n", cfg.getADCMaxHits(det) - 1);
      continue;
    }
    /* Command: d - Show max TDC M-suffix */
    if (cmd == 'd' || cmd == 'D') {
      printf("  Max TDC  M‑suffix: %d\n", cfg.getTDCMaxHits(det) - 1);
      continue;
    }
    /* Command: c - Show max Scaler M-suffix */
    if (cmd == 'c' || cmd == 'C') {
      printf("  Max Scaler M‑suffix: %d\n", cfg.getSCALERMaxHits(det) - 1);
      continue;
    }

    /* Unknown command */
    printf("  Unknown command\n");
  }

  /* Program exit message */
  printf("\nAll resources freed – exiting\n");
  return EXIT_SUCCESS;
}
