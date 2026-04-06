/*********************************************************************
 * paramreader.h
 *
 * Header-only implementation of the ParamReader class.
 *
 *  • Reads lines that start with the literal "Element:".
 *  • Extracts up to three integer parameters + optional M-suffixes.
 *  • Stores each line as a Record in a dynamic array.
 *  • Optionally builds uthash tables for fast lookup.
 *
 *  Public API
 *      - bool  readFile()
 *      - int   findADC(int adc)      // id, -1 if not found, -2 if no file
 *      - int   findTDC(int tdc)      // id, -1 if not found, -2 if no file
 *      - int   findScaler(int scl)   // id, -1 if not found, -2 if no file
 *      - int   getNumberOfElements() const
 *      - int   getMaxADCSuffix() const      // -1 if ADC not requested
 *      - int   getMaxTDCSuffix() const      // -1 if TDC not requested
 *      - int   getMaxScalerSuffix() const   // -1 if Scaler not requested
 *
 *  All I/O is performed with printf/sscanf/fgets; no iostreams,
 *  no C++11 features, no std::string, no std::vector.
 *********************************************************************/

#ifndef PARAMREADER_H
#define PARAMREADER_H

#include <cstdio>   // FILE, fopen, fclose, fgets, printf, fprintf, perror
#include <cstdlib>  // malloc, free, strtol, EXIT_FAILURE, EXIT_SUCCESS
#include <cstring>  // strncpy, strcmp, strchr, strtok
#include <cctype>   // isspace
#include <limits>   // std::numeric_limits
#include "uthash.h" // uthash single-header hash table

/* -----------------------------------------------------------------
 * Record – one line of the input file
 * ----------------------------------------------------------------- */
typedef struct {
    int id;            /* sequential identifier (0,1,2,…)               */
    int adc;           /* ADC value   (-1 if not requested)             */
    int tdc;           /* TDC value   (-1 if not requested)             */
    int scaler;        /* Scaler value(-1 if not requested)             */
    int adc_suffix;    /* integer after 'M' in ADC token   (-1 if none) */
    int tdc_suffix;    /* integer after 'M' in TDC token   (-1 if none) */
    int scaler_suffix; /* integer after 'M' in Scaler token(-1 if none) */
} Record;

/* -----------------------------------------------------------------
 * uthash entry structs
 * ----------------------------------------------------------------- */
typedef struct {
    int   adc;      /* key */
    Record *rec;    /* pointer into the Record array */
    UT_hash_handle hh;
} ADCEntry;

typedef struct {
    int   tdc;
    Record *rec;
    UT_hash_handle hh;
} TDCEntry;

typedef struct {
    int   scaler;
    Record *rec;
    UT_hash_handle hh;
} ScalerEntry;

/* =================================================================
 * ParamReader – header-only class
 * ================================================================= */
class ParamReader {
private:
    /* ------------------- internal state -------------------------- */
    ADCEntry    *_adc_hash;
    TDCEntry    *_tdc_hash;
    ScalerEntry *_scaler_hash;
    Record      **records;        /* dynamic array of Record*              */
    int          rec_cnt;         /* number of records stored              */
    int          rec_cap;         /* current capacity of the array          */
    const char  *filename;        /* file supplied by the user              */

    int max_adc_suffix;
    int max_tdc_suffix;
    int max_scaler_suffix;

    int adc_number;   /* currently unused – kept for future extensions */
    int tdc_number;
    int scaler_number;

    /* ------ configurable token positions (1-based, 0 = ignore) ----- */
    int adc_pos;       /* token number for ADC    (0 = do not read) */
    int tdc_pos;       /* token number for TDC    (0 = do not read) */
    int scaler_pos;    /* token number for Scaler (0 = do not read) */

    /* -------------------------------------------------------------
     * flag that remembers whether the object was created with an
     * empty file name (or readFile() was called with an empty name).
     * ------------------------------------------------------------- */
    bool _emptyFile;

    /* ------------------- helpers -------------------------------- */
    static int parseIntPrefix(const char *tok) {
        if (!tok) return -1;
        char *endptr = NULL;
        long v = strtol(tok, &endptr, 10);
        if (endptr == tok) return -1;                /* no leading digits    */
        if (v > std::numeric_limits<int>::max())
            return std::numeric_limits<int>::max();
        if (v < std::numeric_limits<int>::min())
            return std::numeric_limits<int>::min();
        return static_cast<int>(v);
    }

    static int parseSuffix(const char *tok) {
        if (!tok) return -1;
        const char *m = strchr(tok, 'M');
        if (!m) m = strchr(tok, 'm');
        if (!m) return -1;
        return parseIntPrefix(m + 1);
    }

    void ensureCapacity() {
        if (rec_cnt < rec_cap) return;
        int new_cap = rec_cap ? rec_cap * 2 : 128;
        Record **new_arr = (Record**)malloc(new_cap * sizeof(Record*));
        if (!new_arr) {
            fprintf(stderr,
                    "Error: malloc failed while resizing records (requested %d entries)\n",
                    new_cap);
            exit(EXIT_FAILURE);
        }
        for (int i = 0; i < rec_cnt; ++i) new_arr[i] = records[i];
        free(records);
        records = new_arr;
        rec_cap = new_cap;
    }

    bool insertetoHashes(Record *r) {
        if (adc_pos > 0) {
            ADCEntry *e = (ADCEntry*)malloc(sizeof(ADCEntry));
            if (!e) { perror("malloc ADCEntry"); return false; }
            e->adc = r->adc;
            e->rec = r;
            HASH_ADD_INT(_adc_hash, adc, e);
        }
        if (tdc_pos > 0) {
            TDCEntry *e = (TDCEntry*)malloc(sizeof(TDCEntry));
            if (!e) {
                perror("malloc TDCEntry");
                if (adc_pos > 0) {
                    ADCEntry *ae;
                    HASH_FIND_INT(_adc_hash, &r->adc, ae);
                    if (ae) { HASH_DEL(_adc_hash, ae); free(ae); }
                }
                return false;
            }
            e->tdc = r->tdc;
            e->rec = r;
            HASH_ADD_INT(_tdc_hash, tdc, e);
        }
        if (scaler_pos > 0) {
            ScalerEntry *e = (ScalerEntry*)malloc(sizeof(ScalerEntry));
            if (!e) {
                perror("malloc ScalerEntry");
                if (adc_pos > 0) {
                    ADCEntry *ae;
                    HASH_FIND_INT(_adc_hash, &r->adc, ae);
                    if (ae) { HASH_DEL(_adc_hash, ae); free(ae); }
                }
                if (tdc_pos > 0) {
                    TDCEntry *te;
                    HASH_FIND_INT(_tdc_hash, &r->tdc, te);
                    if (te) { HASH_DEL(_tdc_hash, te); free(te); }
                }
                return false;
            }
            e->scaler = r->scaler;
            e->rec = r;
            HASH_ADD_INT(_scaler_hash, scaler, e);
        }
        return true;
    }

    void freeHashes() {
        ADCEntry *ae, *tmp_ae;
        HASH_ITER(hh, _adc_hash, ae, tmp_ae) {
            HASH_DEL(_adc_hash, ae);
            free(ae);
        }
        _adc_hash = NULL;

        TDCEntry *te, *tmp_te;
        HASH_ITER(hh, _tdc_hash, te, tmp_te) {
            HASH_DEL(_tdc_hash, te);
            free(te);
        }
        _tdc_hash = NULL;

        ScalerEntry *se, *tmp_se;
        HASH_ITER(hh, _scaler_hash, se, tmp_se) {
            HASH_DEL(_scaler_hash, se);
            free(se);
        }
        _scaler_hash = NULL;
    }

public:
    /* ------------------- ctor / dtor --------------------------- */
    explicit ParamReader(const char *fname,
                        int adc_pos_=0,   int adc_number_=0,
                        int tdc_pos_=0,   int tdc_number_=0,
                        int scaler_pos_=0,int scaler_number_=0)
        : _adc_hash(NULL), _tdc_hash(NULL), _scaler_hash(NULL),
          records(NULL), rec_cnt(0), rec_cap(0), filename(fname),
          max_adc_suffix(-1), max_tdc_suffix(-1), max_scaler_suffix(-1),
          adc_number(adc_number_), tdc_number(tdc_number_), scaler_number(scaler_number_),
          adc_pos(adc_pos_), tdc_pos(tdc_pos_), scaler_pos(scaler_pos_),
          _emptyFile( (!fname) || (fname[0] == '\0') )
    {}

    ~ParamReader() {
        freeHashes();
        for (int i = 0; i < rec_cnt; ++i) free(records[i]);
        free(records);
    }

    /* ------------------- read the input file ------------------ */
    bool readFile() {
        /* Reset everything – a second call to readFile() is allowed */
        freeHashes();
        for (int i = 0; i < rec_cnt; ++i) free(records[i]);
        free(records);
        records = NULL;
        rec_cnt = rec_cap = 0;
        max_adc_suffix = max_tdc_suffix = max_scaler_suffix = -1;

        /* ---------------------------------------------------------
         * Empty or missing file name: treat as “no data”.  All
         * lookup functions will now return -2.
         * --------------------------------------------------------- */
        if (!filename || filename[0] == '\0') {
            _emptyFile = true;
            return true;                /* success – just an empty data set */
        }

        /* ---------------------------------------------------------
         * Normal case – try to open the file
         * --------------------------------------------------------- */
        FILE *fp = fopen(filename, "r");
        if (!fp) {
            fprintf(stderr, "Error: cannot open \"%s\"\n", filename);
            _emptyFile = false;        /* file name supplied, but opening failed */
            return false;
        }

        _emptyFile = false;            /* we have a real file to read */

        const int MAX_TOKENS    = 20;
        const int MAX_TOKEN_LEN = 64;
        char line[1024];
        int elementLine = 0;          /* counts only processed Element: lines */

        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "Element:", 8) != 0) continue;

            /* -------------------------------------------------
             * Split the line into whitespace-separated tokens.
             * Discard the literal "Element:" token; token[0] now
             * corresponds to the first data token after it.
             * ------------------------------------------------- */
            char tokens[MAX_TOKENS][MAX_TOKEN_LEN];
            int tokenCount = 0;

            char *tok = strtok(line, " \t\r\n");   /* should be "Element:" */
            if (!tok || strcmp(tok, "Element:") != 0) continue;

            while ((tok = strtok(NULL, " \t\r\n")) && tokenCount < MAX_TOKENS) {
                strncpy(tokens[tokenCount], tok, MAX_TOKEN_LEN - 1);
                tokens[tokenCount][MAX_TOKEN_LEN - 1] = '\0';
                ++tokenCount;
            }

            /* -------------------------------------------------
             * Verify we have enough tokens for the requested fields.
             * ------------------------------------------------- */
            int max_needed = 0;
            if (adc_pos    > max_needed) max_needed = adc_pos;
            if (tdc_pos    > max_needed) max_needed = tdc_pos;
            if (scaler_pos > max_needed) max_needed = scaler_pos;

            if (max_needed > 0 && tokenCount < max_needed) {
                fprintf(stderr,
                        "Warning: line %d has only %d tokens (needs %d) – skipping\n",
                        elementLine + 1, tokenCount, max_needed);
                continue;
            }

            /* -------------------------------------------------
             * Extract values (or -1 if the field is disabled).
             * ------------------------------------------------- */
            int adc_val   = -1, adc_suf   = -1;
            int tdc_val   = -1, tdc_suf   = -1;
            int scaler_val= -1, scaler_suf= -1;

            if (adc_pos > 0) {
                const char *tp = tokens[adc_pos - 1];
                adc_val = parseIntPrefix(tp);
                adc_suf = parseSuffix(tp);
            }
            if (tdc_pos > 0) {
                const char *tp = tokens[tdc_pos - 1];
                tdc_val = parseIntPrefix(tp);
                tdc_suf = parseSuffix(tp);
            }
            if (scaler_pos > 0) {
                const char *tp = tokens[scaler_pos - 1];
                scaler_val = parseIntPrefix(tp);
                scaler_suf = parseSuffix(tp);
            }

            /* -------------------------------------------------
             * Allocate a new Record and store it.
             * ------------------------------------------------- */
            Record *rec = (Record*)malloc(sizeof(Record));
            if (!rec) {
                perror("malloc Record");
                fclose(fp);
                return false;
            }
            rec->id            = elementLine;   /* sequential id */
            rec->adc           = adc_val;
            rec->tdc           = tdc_val;
            rec->scaler        = scaler_val;
            rec->adc_suffix    = adc_suf;
            rec->tdc_suffix    = tdc_suf;
            rec->scaler_suffix = scaler_suf;

            ensureCapacity();
            records[rec_cnt++] = rec;
            ++elementLine;

            /* -------------------------------------------------
             * Update maximum M-suffix trackers.
             * ------------------------------------------------- */
            if (adc_pos    > 0 && adc_suf   > max_adc_suffix)    max_adc_suffix    = adc_suf;
            if (tdc_pos    > 0 && tdc_suf   > max_tdc_suffix)    max_tdc_suffix    = tdc_suf;
            if (scaler_pos > 0 && scaler_suf> max_scaler_suffix) max_scaler_suffix = scaler_suf;

            /* -------------------------------------------------
             * Insert into the requested hash tables.
             * ------------------------------------------------- */
            if (!insertetoHashes(rec)) {
                free(rec);
                --rec_cnt;
                fclose(fp);
                return false;
            }
        }

        fclose(fp);
        return true;
    }

    /* ------------------- look-ups (return id, -1, or -2) ------------ */
    int findADC(int adc) const {
        if (_emptyFile) return -2;
        ADCEntry *e = NULL;
        HASH_FIND_INT(_adc_hash, &adc, e);
        return e ? e->rec->id : -1;
    }

    int findTDC(int tdc) const {
        if (_emptyFile) return -2;
        TDCEntry *e = NULL;
        HASH_FIND_INT(_tdc_hash, &tdc, e);
        return e ? e->rec->id : -1;
    }

    int findScaler(int scaler) const {
        if (_emptyFile) return -2;
        ScalerEntry *e = NULL;
        HASH_FIND_INT(_scaler_hash, &scaler, e);
        return e ? e->rec->id : -1;
    }

    /* ------------------- public helpers ----------------------- */
    int getNumberOfElements() const { return rec_cnt; }
    int getMaxADCSuffix() const { return max_adc_suffix; }
    int getMaxTDCSuffix() const { return max_tdc_suffix; }
    int getMaxScalerSuffix() const { return max_scaler_suffix; }

    int getADCInitW() const { return adc_number; }
    int getTDCInitW() const { return tdc_number; }
    int getScalerInitW() const { return scaler_number; }

    struct MaxSuffixInfo {
        int maxADC;
        int maxTDC;
        int maxScaler;
    };
    MaxSuffixInfo getMaxSuffixInfo() const {
        MaxSuffixInfo info = { max_adc_suffix, max_tdc_suffix, max_scaler_suffix };
        return info;
    }
};

#endif /* PARAMREADER_H */
