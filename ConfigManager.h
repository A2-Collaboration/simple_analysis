#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

/*
 * ConfigManager.h
 * 
 * This header defines the ConfigManager class, which is the top-level configuration
 * handler for the detector system.
 * 
 * Responsibilities:
 * - Parse the main configuration file
 * - Manage detector-specific parameter files via ParamReader
 * - Handle TDC reference channel ranges (for time calibration)
 * - Provide a unified interface for channel mapping
 * 
 * The system is designed for single-hit operation, where each physical channel
 * maps to exactly one logical element.
 */

#include "ParamReader.h"
#include <stdio.h>      /* Standard C file operations */
#include <stdlib.h>     /* Memory allocation, exit functions */
#include <string.h>     /* String manipulation */
#include <ctype.h>      /* Character classification */
#include <map>          /* For reference data storage */
#include <climits>      /* For UINT_MAX */

/* Query types for internal use */
enum { qADC, qTDC, qScaler };

/*
 * Detector types
 * 
 * These enums identify the different detectors in the system.
 * The naming convention follows common particle physics detectors:
 * - TAGGER: Tagging spectrometer
 * - MWPC: Multi-Wire Proportional Chamber
 * - PID: Particle Identification detector
 * - CB: Crystal Ball calorimeter
 * - VETO: Veto counter
 * - PBWO4: Lead Tungstate calorimeter
 * - BAF2: Barium Fluoride calorimeter
 * - SCALER: Scaler module
 */
enum {
  D_NONE, D_TAGGER, D_MWPC_W, D_MWPC_S, D_PID,
  D_CB, D_VETO, D_PBWO4, D_PBWO4_S,
  D_BAF2_S, D_BAF2_L, D_SCALER, N_DETECTORS
};

class ConfigManager {
public:
  int verboselvl;      /* Verbosity level for diagnostic output */
  static const int MAX_TDC_REF = 10;  /* Maximum TDC reference ranges */

  /* Structure for TDC reference ranges */
  struct RefRange {
    int start, end, ref;  /* Range [start, end] uses reference channel 'ref' */
  };

  /* Constructor: Initialize with default values */
  ConfigManager() {
    /* Initialize all detectors as not in use */
    for (int i = 0; i < N_DETECTORS; ++i) in_use[i] = 0;
    tdc_ref_cnt = 0;  /* No TDC reference ranges initially */
  }

  /* 
   * Read and parse the main configuration file
   * 
   * Parameters:
   *   cfg_file - Configuration file name
   *   verboselvl_ - Verbosity level (default: 20)
   * 
   * Returns:
   *   1 if successful, 0 if there was an error
   */
  int readConfig(const char *cfg_file, int verboselvl_ = 20) {
    char name[64];     /* Detector name buffer */
    char line[1024];   /* Line buffer for reading */
    
    /* Set verbosity level */
    verboselvl = verboselvl_;
    
    /* Open configuration file */
    FILE *fp = fopen(cfg_file, "r");
    if (!fp) {
      printf("Error: cannot open config file \"%s\"\n", cfg_file);
      exit(-2);
    }

    /* Process each line in the configuration file */
    while (fgets(line, sizeof(line), fp)) {
      trim(line);  /* Remove leading/trailing whitespace */
      
      /* Skip empty lines and comments */
      if (line[0] == '\0' || line[0] == '#') continue;

      /* Handle TDC_REF entries */
      if (strncmp(line, "TDC_REF", 7) == 0) {
        char tag[32];
        int start, end, ref;
        
        /* Parse TDC_REF line */
        int n = sscanf(line, "%31s %d %d %d", tag, &start, &end, &ref);
        
        /* Check if we have valid data and room for more entries */
        if (n == 4 && tdc_ref_cnt < MAX_TDC_REF) {
          /* Store the reference range */
          tdc_ref[tdc_ref_cnt++] = { start, end, ref };
        } 
        /* Warn if we have too many entries */
        else if (n == 4) {
          printf("Warning: too many TDC_REF entries (max %d)\n", MAX_TDC_REF);
        }
        continue;
      }

      /* 
       * Handle detector configuration lines
       * Format: NAME pos_adc n_adc pos_tdc n_tdc pos_scaler n_scaler filename
       */
      int pos_adc, n_adc, pos_tdc, n_tdc, pos_scaler, n_scaler;
      char filename[256];
      
      /* Parse the detector line */
      int fields = sscanf(line, "%63s %d %d %d %d %d %d %255s",
                         name, &pos_adc, &n_adc, &pos_tdc, &n_tdc,
                         &pos_scaler, &n_scaler, filename);
                         
      /* Check if the line is properly formatted */
      if (fields != 8) {
        printf("Warning: malformed line – skipping: %s\n", line);
        continue;
      }

      /* Convert detector name to uppercase for case-insensitive matching */
      my_strupr(name);
      
      /* Find the detector enum value */
      int det = findDetector(name);
      
      /* Process if we recognize the detector */
      if (det >= 0) {
        /* Initialize the ParamReader for this detector */
        pr[det].init(filename,
                     pos_adc, n_adc,
                     pos_tdc, n_tdc,
                     pos_scaler, n_scaler);
                     
        /* Read the parameter file */
        if (!pr[det].readFile()) {
          printf("Can't read file: %s\n", filename);
          fclose(fp);
          return 0;
        }
        
        /* Mark detector as in use */
        in_use[det] = 1;
      } 
      /* Warn about unknown detectors */
      else {
        printf("Warning: unknown detector '%s'\n", name);
      }
    }

    /* Clean up */
    fclose(fp);

    /* Initialize unused detectors with empty ParamReaders */
    for (int i = 0; i < N_DETECTORS; ++i) {
      if (strlen(DetectorName[i]) > 0 && !in_use[i]) {
        /* Verbose output for debugging */
        if (verboselvl > 10)
          printf("Not in use: %s\n", DetectorName[i]);
          
        /* Initialize with empty parameters */
        pr[i].init("", 0, 0, 0, 0, 0, 0);
      }
    }
    return 1;
  }

  /* 
   * Find the reference channel for a given TDC channel
   * 
   * Parameters:
   *   id - TDC channel ID
   * 
   * Returns:
   *   The reference channel ID, or -1 if none
   * 
   * This is used for time calibration - certain TDC channels are designated
   * as reference channels to correct for time walk and other effects.
   */
  int getTDC_ref(int id) const {
    /* Check each reference range */
    for (int i = 0; i < tdc_ref_cnt; ++i) {
      /* If id is within the range, return the reference channel */
      if (id >= tdc_ref[i].start && id <= tdc_ref[i].end)
        return tdc_ref[i].ref;
    }
    return -1;
  }

  /* 
   * Get the ParamReader for a specific detector
   * 
   * Parameters:
   *   det - Detector enum value
   * 
   * Returns:
   *   Reference to the ParamReader for that detector
   * 
   * Note: This function validates the detector ID and exits on error
   */
  ParamReader& get(int det) {
    /* Validate detector ID */
    if (det < 0 || det >= N_DETECTORS) {
      printf("ERROR: Invalid detector ID %d\n", det);
      exit(-1);
    }
    return pr[det];
  }

  /* 
   * Logical -> Physical mapping functions
   * 
   * These take a logical element ID and return the physical channel number
   * 
   * Parameters:
   *   det - Detector enum value
   *   id - Logical element ID
   * 
   * Returns:
   *   Physical channel number, or -1 if invalid
   */
  int getADC_ch(int det, int id) const {
    if (!isDetectorInUse(det)) return -1;
    return pr[det].getADC_ch(id);
  }
  
  int getTDC_ch(int det, int id) const {
    if (!isDetectorInUse(det)) return -1;
    return pr[det].getTDC_ch(id);
  }
  
  int getScaler_ch(int det, int id) const {
    if (!isDetectorInUse(det)) return -1;
    return pr[det].getScaler_ch(id);
  }

  /* 
   * Physical -> Logical mapping functions
   * 
   * These take a physical channel number and return the logical element ID
   * 
   * Parameters:
   *   det - Detector enum value
   *   ch - Physical channel number
   * 
   * Returns:
   *   Logical element ID, or -1 if not found
   */
  int getADC_id(int det, int ch) const {
    if (!isDetectorInUse(det)) return -1;
    return pr[det].getADC_id(ch);
  }
  
  int getTDC_id(int det, int ch) const {
    if (!isDetectorInUse(det)) return -1;
    return pr[det].getTDC_id(ch);
  }
  
  int getScaler_id(int det, int ch) const {
    if (!isDetectorInUse(det)) return -1;
    return pr[det].getScaler_id(ch);
  }

  /* 
   * Get the number of logical elements for a detector
   * 
   * Parameters:
   *   det - Detector enum value
   * 
   * Returns:
   *   Number of logical elements, or -1 if invalid
   */
  int getnoCh(int det) const {
    if (det < 0 || det >= N_DETECTORS || !in_use[det]) return -1;
    return pr[det].getNumberOfElements();
  }

  /* 
   * Get maximum hits per channel for a detector
   * 
   * Parameters:
   *   det - Detector enum value
   * 
   * Returns:
   *   Maximum hits, or -1 if invalid
   */
  int getADCMaxHits(int det) const {
    if (det < 0 || det >= N_DETECTORS || !in_use[det]) return -1;
    return pr[det].getADCMaxHits();
  }
  
  int getTDCMaxHits(int det) const {
    if (det < 0 || det >= N_DETECTORS || !in_use[det]) return -1;
    return pr[det].getTDCMaxHits();
  }
  
  int getSCALERMaxHits(int det) const {
    if (det < 0 || det >= N_DETECTORS || !in_use[det]) return -1;
    return pr[det].getSCALERMaxHits();
  }

  /* 
   * Get detector name from enum value
   * 
   * Parameters:
   *   det - Detector enum value
   * 
   * Returns:
   *   Detector name string, or "INVALID" if invalid
   */
  const char* getDetectorName(int det) const {
    if (det < 0 || det >= N_DETECTORS) return "INVALID";
    return DetectorName[det];
  }

  /* 
   * Check if a detector is in use
   * 
   * Parameters:
   *   det - Detector enum value
   * 
   * Returns:
   *   true if the detector is configured, false otherwise
   */
  bool isDetectorInUse(int det) const {
    return (det >= 0 && det < N_DETECTORS && in_use[det]);
  }

  /* 
   * Store reference data for a reference channel
   * 
   * Parameters:
   *   id - Channel ID (should be a reference channel)
   *   data - Data to store (typically a timestamp)
   * 
   * Note: This function stores data using the reference ID as the key
   */
  void store_ref_data(int id, unsigned int data) {
    /* Find which reference range this ID belongs to */
    for (int i = 0; i < tdc_ref_cnt; ++i) {
      if (id == tdc_ref[i].ref) {
        /* Store using the reference ID as the key (critical fix) */
        ref_data[tdc_ref[i].ref] = data;
        
        /* Verbose output */
        if (verboselvl > 5)
          printf("Ref data stored for ID %d: %u\n",
                 tdc_ref[i].ref, data);
        return;
      }
    }
  }

  /* 
   * Get reference data for a channel
   * 
   * Parameters:
   *   id - Channel ID
   * 
   * Returns:
   *   Stored reference data, or UINT_MAX if none
   * 
   * This function first finds the reference channel for id,
   * then retrieves the stored data for that reference channel.
   */
  unsigned int get_ref_data(int id) const {
    /* Find the reference channel for this ID */
    int ref_id = getTDC_ref(id);
    
    /* If we have a valid reference channel */
    if (ref_id != -1) {
      /* Look up the stored data */
      auto it = ref_data.find(ref_id);
      if (it != ref_data.end()) return it->second;
    }
    return UINT_MAX;  /* No data found */
  }

  /* Reset all stored reference data */
  void reset_ref_data() { 
    ref_data.clear(); 
  }

  /* Print a summary of the configuration */
  void printSummary() const {
    printf("Configuration Summary:\n");
    
    /* List detectors in use */
    printf("\nDetectors in Use:\n");
    bool any = false;
    for (int i = 0; i < N_DETECTORS; ++i) {
      if (in_use[i]) {
        any = true;
        printf("  %s: %s (%d elements)\n",
               DetectorName[i],
               pr[i].getFileName(),
               pr[i].getNumberOfElements());
      }
    }
    if (!any) printf("  None\n");

    /* List TDC reference ranges */
    printf("\nReference Ranges:\n");
    if (tdc_ref_cnt) {
      for (int i = 0; i < tdc_ref_cnt; ++i) {
        printf("  Range %d: %d - %d (Ref ID: %d)\n",
               i, tdc_ref[i].start, tdc_ref[i].end, tdc_ref[i].ref);
      }
    } else {
      printf("  None\n");
    }

    /* List stored reference data */
    printf("\nStored Reference Data:\n");
    if (!ref_data.empty()) {
      for (const auto &e : ref_data)
        printf("  Ref ID %d: Data %u\n", e.first, e.second);
    } else {
      printf("  None (use 'store_ref' command to add)\n");
    }
  }

private:
  /* Array of ParamReaders, one per detector */
  ParamReader pr[N_DETECTORS];
  
  /* Flags indicating which detectors are in use */
  int in_use[N_DETECTORS];
  
  /* Array of TDC reference ranges */
  RefRange tdc_ref[MAX_TDC_REF];
  
  /* Count of TDC reference ranges */
  int tdc_ref_cnt;
  
  /* Map storing reference data (key: reference channel ID) */
  std::map<int, unsigned int> ref_data;

  /* Detector names corresponding to enum values */
  static inline const char* DetectorName[N_DETECTORS] = {
    "NONE", "TAGGER", "MWPC_W", "MWPC_S", "PID",
    "CB", "VETO", "PBWO4", "PBWO4_S",
    "BAF2_S", "BAF2_L", "SCALER"
  };

  /* Helper function: Trim whitespace from a string */
  void trim(char *s) {
    char *p = s;
    /* Skip leading whitespace */
    while (*p && isspace((unsigned char)*p)) ++p;
    /* Move non-whitespace characters to beginning */
    if (p != s) memmove(s, p, strlen(p) + 1);
    
    /* Remove trailing whitespace */
    size_t len = strlen(s);
    if (!len) return;
    char *end = s + len - 1;
    while (end >= s && isspace((unsigned char)*end)) {
      *end = '\0';
      --end;
    }
  }

  /* Helper function: Convert string to uppercase */
  void my_strupr(char *str) {
    for (int i = 0; str[i]; ++i) 
      str[i] = toupper((unsigned char)str[i]);
  }

  /* Helper function: Find detector enum from name */
  int findDetector(const char* name) {
    for (int i = 0; i < N_DETECTORS; ++i)
      if (strcmp(DetectorName[i], name) == 0) return i;
    return -1;
  }
};

#endif
