#ifndef PARAMREADER_H
#define PARAMREADER_H

/*
 * ParamReader.h
 * 
 * This header defines the ParamReader class, which is responsible for parsing
 * parameter files that map physical detector channels to logical element IDs.
 * 
 * In particle physics experiments, detectors often have many physical channels
 * (ADC, TDC, Scaler) that need to be mapped to logical positions in the detector
 * geometry (e.g., crystal positions in a calorimeter). This class handles that
 * mapping.
 * 
 * Key design points:
 * - Single-hit system: Each physical channel maps to exactly one logical element
 * - Parameter files have "Element:" lines with channel information
 * - Supports M-suffix notation (e.g., 100M0, 100M1) for multi-hit capability,
 *   though the system is designed for single-hit operation
 */

#include <cstdio>      /* Standard C file operations */
#include <cstdlib>     /* Memory allocation, exit functions */
#include <cstring>     /* String manipulation */
#include <cctype>      /* Character classification */
#include <limits>      /* Numeric limits */
#include <algorithm>   /* For std::max */
#include "UtHash.h"    /* Hash table implementation */

/*
 * Record structure: Stores the mapping for a single logical detector element
 * 
 * Each logical element (e.g., a crystal in a calorimeter) has:
 * - id: The logical ID (0, 1, 2, ...)
 * - adc: Physical ADC channel number
 * - tdc: Physical TDC channel number
 * - scaler: Physical scaler channel number
 * - adc_suffix, tdc_suffix, scaler_suffix: M-suffix values (0, 1, 2, ...)
 *   These indicate multiple hits on the same physical channel
 */
typedef struct {
  int id;
  int adc;
  int tdc;
  int scaler;
  int adc_suffix;
  int tdc_suffix;
  int scaler_suffix;
} Record;

/*
 * Hash entry structures: Used to create hash tables for fast lookups
 * 
 * These structures allow us to quickly find the Record corresponding to
 * a physical channel number. For example, given ADC channel 100, we can
 * quickly find which logical element it belongs to.
 */
typedef struct {
  int adc;            /* Physical ADC channel number */
  Record *rec;        /* Pointer to the corresponding Record */
  UT_hash_handle hh;  /* Required by uthash library */
} ADCEntry;

typedef struct {
  int tdc;            /* Physical TDC channel number */
  Record *rec;        /* Pointer to the corresponding Record */
  UT_hash_handle hh;  /* Required by uthash library */
} TDCEntry;

typedef struct {
  int scaler;         /* Physical scaler channel number */
  Record *rec;        /* Pointer to the corresponding Record */
  UT_hash_handle hh;  /* Required by uthash library */
} ScalerEntry;

/*
 * ParamReader class: Handles the parsing and mapping of parameter files
 * 
 * This class:
 * 1. Reads parameter files with "Element:" lines
 * 2. Parses channel numbers and M-suffixes
 * 3. Creates hash tables for fast physical->logical lookups
 * 4. Provides methods for both physical->logical and logical->physical mapping
 * 
 * Important note: This implementation is designed for single-hit operation,
 * meaning each physical channel maps to exactly one logical element.
 * If multiple "Element:" lines use the same physical channel, the last one wins.
 */
class ParamReader {
private:
  /* Hash tables for fast lookups (physical channel -> Record) */
  ADCEntry    *_adc_hash;
  TDCEntry    *_tdc_hash;
  ScalerEntry *_scaler_hash;
  
  /* Array of all Records, indexed by logical ID */
  Record      **records;
  int rec_cnt;        /* Number of Records currently stored */
  int rec_cap;        /* Current capacity of the records array */
  
  const char  *filename;  /* Name of the parameter file being processed */
  
  /* Tracking the maximum M-suffix encountered for each channel type */
  int max_adc_suffix;
  int max_tdc_suffix;
  int max_scaler_suffix;
  
  /* Maximum hits per channel (from config file) */
  int adc_maxhits;
  int tdc_maxhits;
  int scaler_maxhits;
  
  /* Positions of ADC, TDC, Scaler values in parameter file lines */
  int adc_pos;
  int tdc_pos;
  int scaler_pos;
  
  bool _emptyFile;    /* Flag indicating if no parameter file is used */

  /* Helper function: Parse integer prefix from a token */
  static int parseIntPrefix(const char *tok) {
    /* 
     * This function extracts the numeric part from tokens like "100M0"
     * 
     * Parameters:
     *   tok - Input token (e.g., "100M0")
     * 
     * Returns:
     *   The integer prefix (e.g., 100) or -1 if parsing fails
     */
    if (!tok) return -1;
    char *endptr = nullptr;
    long v = strtol(tok, &endptr, 10);
    
    /* Check if we actually parsed a number */
    if (endptr == tok) return -1;
    
    /* Handle overflow/underflow */
    if (v > std::numeric_limits<int>::max())
      return std::numeric_limits<int>::max();
    if (v < std::numeric_limits<int>::min())
      return std::numeric_limits<int>::min();
      
    return (int)v;
  }

  /* Helper function: Parse M-suffix from a token */
  static int parseSuffix(const char *tok) {
    /* 
     * This function extracts the M-suffix from tokens like "100M0"
     * 
     * Parameters:
     *   tok - Input token (e.g., "100M0")
     * 
     * Returns:
     *   The M-suffix value (e.g., 0) or -1 if no suffix is present
     */
    if (!tok) return -1;
    
    /* Find 'M' or 'm' in the token */
    const char *m = strchr(tok, 'M');
    if (!m) m = strchr(tok, 'm');
    if (!m) return -1;
    
    /* Parse the number after 'M' */
    return parseIntPrefix(m + 1);
  }

  /* Helper function: Check if line starts with "Element:" */
  static bool lineStartsWithElement(const char* line) {
    /* 
     * This function checks if a line begins with "Element:" (case-insensitive)
     * 
     * Parameters:
     *   line - Input line to check
     * 
     * Returns:
     *   true if the line starts with "Element:", false otherwise
     */
    if (!line) return false;
    
    /* Skip leading whitespace */
    while (*line && isspace((unsigned char)*line)) ++line;
    
    /* Compare with "element:" (case-insensitive) */
    const char* keyword = "element:";
    for (int i = 0; keyword[i]; ++i) {
      if (tolower((unsigned char)line[i]) != keyword[i]) return false;
    }
    return true;
  }

  /* Helper function: Ensure records array has enough capacity */
  void ensureCapacity() {
    /* 
     * This function ensures the records array can hold at least rec_cnt elements
     * 
     * If the current capacity is insufficient, it doubles the capacity (or sets to 128 if empty)
     */
    if (rec_cnt < rec_cap) return;
    
    /* Calculate new capacity (double current or start at 128) */
    int new_cap = rec_cap ? rec_cap * 2 : 128;
    
    /* Allocate new array */
    Record **new_arr = (Record**)malloc(new_cap * sizeof(Record*));
    if (!new_arr) {
      fprintf(stderr, "Out of memory\n");
      exit(EXIT_FAILURE);
    }
    
    /* Copy existing records if any */
    if (records && rec_cnt > 0)
      memcpy(new_arr, records, rec_cnt * sizeof(Record*));
      
    /* Clean up old array and update pointers */
    free(records);
    records   = new_arr;
    rec_cap   = new_cap;
  }

  /* 
   * Helper function: Insert a Record into the hash tables
   * 
   * For single-hit operation, if a physical channel already exists in the hash,
   * we simply update its Record pointer (the last definition wins)
   */
  bool insertIntoHashes(Record *r) {
    /* Process ADC channel */
    if (adc_pos > 0) {
      ADCEntry *e = nullptr;
      HASH_FIND_INT(_adc_hash, &r->adc, e);
      if (e) {
        /* Update existing entry (single-hit: last definition wins) */
        e->rec = r;
      }
      else {
        /* Create new entry */
        e = (ADCEntry*)malloc(sizeof(ADCEntry));
        if (!e) return false;
        e->adc = r->adc;
        e->rec = r;
        HASH_ADD_INT(_adc_hash, adc, e);
      }
    }
    
    /* Process TDC channel (same pattern as ADC) */
    if (tdc_pos > 0) {
      TDCEntry *e = nullptr;
      HASH_FIND_INT(_tdc_hash, &r->tdc, e);
      if (e) e->rec = r;
      else {
        e = (TDCEntry*)malloc(sizeof(TDCEntry));
        if (!e) return false;
        e->tdc = r->tdc;
        e->rec = r;
        HASH_ADD_INT(_tdc_hash, tdc, e);
      }
    }
    
    /* Process Scaler channel (same pattern) */
    if (scaler_pos > 0) {
      ScalerEntry *e = nullptr;
      HASH_FIND_INT(_scaler_hash, &r->scaler, e);
      if (e) e->rec = r;
      else {
        e = (ScalerEntry*)malloc(sizeof(ScalerEntry));
        if (!e) return false;
        e->scaler = r->scaler;
        e->rec    = r;
        HASH_ADD_INT(_scaler_hash, scaler, e);
      }
    }
    return true;
  }

  /* Helper function: Free all hash tables */
  void freeHashes() {
    /* 
     * This function frees all memory associated with the hash tables
     * 
     * It iterates through each hash table and frees all entries
     */
    ADCEntry *ae, *tmp_ae;
    HASH_ITER(hh, _adc_hash, ae, tmp_ae) { HASH_DEL(_adc_hash, ae); free(ae); }
    _adc_hash = nullptr;

    TDCEntry *te, *tmp_te;
    HASH_ITER(hh, _tdc_hash, te, tmp_te) { HASH_DEL(_tdc_hash, te); free(te); }
    _tdc_hash = nullptr;

    ScalerEntry *se, *tmp_se;
    HASH_ITER(hh, _scaler_hash, se, tmp_se) { HASH_DEL(_scaler_hash, se); free(se); }
    _scaler_hash = nullptr;
  }

  /* Helper function: Clean up all resources */
  void cleanup() {
    /* 
     * This function frees all memory allocated by the ParamReader
     * 
     * It's called during initialization, destruction, and error handling
     * to ensure no memory leaks
     */
    freeHashes();
    
    /* Free all Records */
    for (int i = 0; i < rec_cnt; ++i) free(records[i]);
    
    /* Free the records array */
    free(records);
    
    /* Reset state */
    records = nullptr;
    rec_cnt = rec_cap = 0;
  }

public:
  /* Constructor: Initialize with default values */
  ParamReader()
    : _adc_hash(nullptr), _tdc_hash(nullptr), _scaler_hash(nullptr),
      records(nullptr), rec_cnt(0), rec_cap(0), filename(nullptr),
      max_adc_suffix(-1), max_tdc_suffix(-1), max_scaler_suffix(-1),
      adc_maxhits(0), tdc_maxhits(0), scaler_maxhits(0),
      adc_pos(0), tdc_pos(0), scaler_pos(0),
      _emptyFile(true)
  {
    /* All initialization happens in the initializer list */
  }

  /* 
   * Initialize the ParamReader with configuration parameters
   * 
   * Parameters:
   *   fname - Parameter file name (nullptr or empty for no file)
   *   adc_pos_, adc_maxhits_ - ADC position in file and max hits
   *   tdc_pos_, tdc_maxhits_ - TDC position in file and max hits
   *   scaler_pos_, scaler_maxhits_ - Scaler position and max hits
   */
  void init(const char *fname,
            int adc_pos_=0, int adc_maxhits_=0,
            int tdc_pos_=0, int tdc_maxhits_=0,
            int scaler_pos_=0, int scaler_maxhits_=0)
  {
    /* Clean up any previous state */
    cleanup();
    
    /* Set configuration parameters */
    filename         = fname;
    max_adc_suffix   = max_tdc_suffix = max_scaler_suffix = -1;
    adc_maxhits      = adc_maxhits_;
    tdc_maxhits      = tdc_maxhits_;
    scaler_maxhits   = scaler_maxhits_;
    adc_pos          = adc_pos_;
    tdc_pos          = tdc_pos_;
    scaler_pos       = scaler_pos_;
    
    /* Mark as empty if no filename provided */
    _emptyFile       = (!fname || fname[0] == '\0');
  }

  /* Destructor: Clean up all resources */
  ~ParamReader() { 
    cleanup(); 
  }

  /* 
   * Read and parse the parameter file
   * 
   * Returns:
   *   true if successful, false if there was an error
   */
  bool readFile() {
    /* Start with a clean state */
    cleanup();

    /* Handle empty file case */
    if (!filename || filename[0] == '\0') {
      _emptyFile = true;
      return true;
    }

    /* Open the parameter file */
    FILE *fp = fopen(filename, "r");
    if (!fp) {
      printf("Error: cannot open \"%s\"\n", filename);
      _emptyFile = false;
      return false;
    }
    _emptyFile = false;

    char line[1024];
    int elementLine = 0;  /* Counter for logical IDs */

    /* Process each line in the file */
    while (fgets(line, sizeof(line), fp)) {
      /* Skip non-Element lines */
      if (!lineStartsWithElement(line)) continue;

      /* Skip the "Element:" token */
      char *tok = strtok(line, " \t\r\n");
      if (!tok) continue;

      /* Parse tokens after "Element:" */
      char *tokens[25];
      int tokenCount = 0;
      while ((tok = strtok(nullptr, " \t\r\n")) && tokenCount < 25)
        tokens[tokenCount++] = tok;

      /* Initialize channel values */
      int adc_val = -1, tdc_val = -1, scaler_val = -1;
      int adc_suf = -1, tdc_suf = -1, scaler_suf = -1;

      /* Parse ADC channel if position is specified */
      if (adc_pos > 0 && tokenCount >= adc_pos) {
        adc_val = parseIntPrefix(tokens[adc_pos - 1]);
        adc_suf = parseSuffix   (tokens[adc_pos - 1]);
      }
      
      /* Parse TDC channel if position is specified */
      if (tdc_pos > 0 && tokenCount >= tdc_pos) {
        tdc_val = parseIntPrefix(tokens[tdc_pos - 1]);
        tdc_suf = parseSuffix   (tokens[tdc_pos - 1]);
      }
      
      /* Parse Scaler channel if position is specified */
      if (scaler_pos > 0 && tokenCount >= scaler_pos) {
        scaler_val = parseIntPrefix(tokens[scaler_pos - 1]);
        scaler_suf = parseSuffix   (tokens[scaler_pos - 1]);
      }

      /* Create a new Record for this element */
      Record *rec = (Record*)malloc(sizeof(Record));
      if (!rec) {
        fclose(fp);
        cleanup();
        return false;
      }

      /* Fill in the Record */
      rec->id            = elementLine++;
      rec->adc           = adc_val;
      rec->tdc           = tdc_val;
      rec->scaler        = scaler_val;
      rec->adc_suffix    = adc_suf;
      rec->tdc_suffix    = tdc_suf;
      rec->scaler_suffix = scaler_suf;

      /* Ensure we have space in the records array */
      ensureCapacity();
      
      /* Add the Record to the array */
      records[rec_cnt++] = rec;

      /* Update maximum M-suffix values */
      if (adc_suf   > max_adc_suffix)   max_adc_suffix   = adc_suf;
      if (tdc_suf   > max_tdc_suffix)   max_tdc_suffix   = tdc_suf;
      if (scaler_suf> max_scaler_suffix)max_scaler_suffix = scaler_suf;

      /* Insert the Record into the hash tables */
      if (!insertIntoHashes(rec)) {
        fclose(fp);
        cleanup();
        return false;
      }
    }

    /* Clean up and return success */
    fclose(fp);
    return true;
  }

  /* 
   * Physical -> Logical mapping functions
   * 
   * These functions take a physical channel number and return the corresponding
   * logical element ID, or -1 if not found.
   * 
   * Note: These use the hash tables for fast lookups.
   */
  int findADC(int adc) const {
    if (_emptyFile) return -2;
    ADCEntry *e = nullptr;
    HASH_FIND_INT(_adc_hash, &adc, e);
    return e ? e->rec->id : -1;
  }
  
  int findTDC(int tdc) const {
    if (_emptyFile) return -2;
    TDCEntry *e = nullptr;
    HASH_FIND_INT(_tdc_hash, &tdc, e);
    return e ? e->rec->id : -1;
  }
  
  int findScaler(int scaler) const {
    if (_emptyFile) return -2;
    ScalerEntry *e = nullptr;
    HASH_FIND_INT(_scaler_hash, &scaler, e);
    return e ? e->rec->id : -1;
  }


  int getADC_id(int ch) const {
    if (ch < 0 || ch >= rec_cnt) return -1;
    return records[ch]->adc;
  }
  
  int getTDC_id(int ch) const {
    if (ch < 0 || ch >= rec_cnt) return -1;
    return records[ch]->tdc;
  }
  
  int getScaler_id(int ch) const {
    if (ch < 0 || ch >= rec_cnt) return -1;
    return records[ch]->scaler;
  }


  int getADC_ch(int id) const    { return findADC(id);    }
  int getTDC_ch(int id) const    { return findTDC(id);    }
  int getScaler_ch(int id) const    { return findScaler(id);    }

  int getNumberOfElements() const { return rec_cnt; }

  /* 
   * Get the maximum number of hits per channel
   * 
   * This considers both the M-suffix values in the parameter file
   * and the maxhits values from the config file.
   * 
   * For single-hit systems, this should typically be 1.
   */
  int getADCMaxHits() const {
    int suffix_hits = (max_adc_suffix >= 0) ? max_adc_suffix + 1 : 0;
    return (suffix_hits > adc_maxhits) ? suffix_hits : adc_maxhits;
  }
  
  int getTDCMaxHits() const {
    int suffix_hits = (max_tdc_suffix >= 0) ? max_tdc_suffix + 1 : 0;
    return (suffix_hits > tdc_maxhits) ? suffix_hits : tdc_maxhits;
  }
  
  int getSCALERMaxHits() const {
    int suffix_hits = (max_scaler_suffix >= 0) ? max_scaler_suffix + 1 : 0;
    return (suffix_hits > scaler_maxhits) ? suffix_hits : scaler_maxhits;
  }

  /* Get the parameter file name */
  const char* getFileName() const { return filename; }
};

#endif
