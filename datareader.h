/*********************************************************************
 *  datareader.h
 *
 *  Header-only implementation of the DataReader class.
 *
 *  * Reads lines that start with the literal "Element:".
 *  * For each such line it extracts up to three integer parameters plus
 *    the numeric "M-suffix" that may follow each parameter:
 *        ADC    = numeric prefix at token position 'adc_pos'
 *        ADC-suffix    = integer after 'M' in that token (-1 if none)
 *        TDC    = numeric prefix at token position 'tdc_pos'
 *        TDC-suffix    = integer after 'M' in that token (-1 if none)
 *        Scaler = numeric prefix at token position 'scaler_pos'
 *        Scaler-suffix = integer after 'M' in that token (-1 if none)
 *
 *    If a position argument is 0, that parameter is ignored – it is not
 *    read, not stored, and no hash entry is created for it.
 *
 *  * Stores each line in a dynamically-grown C array of Record.
 *  * Maintains up to three uthash tables (only the requested ones):
 *        - adc_hash    : ADC    ? Record
 *        - tdc_hash    : TDC    ? Record
 *        - scaler_hash : Scaler ? Record
 *
 *  * Public API
 *        - bool  readFile()
 *        - Record* findADC(int adc)      // valid only if adc_pos > 0
 *        - Record* findTDC(int tdc)      // valid only if tdc_pos > 0
 *        - Record* findScaler(int scl)   // valid only if scaler_pos > 0
 *        - int   getNumberOfElements() const
 *        - int   getMaxADCSuffix() const     // –1 if ADC not requested
 *        - int   getMaxTDCSuffix() const     // –1 if TDC not requested
 *        - int   getMaxScalerSuffix() const  // –1 if Scaler not requested
 *
 *  All I/O is performed with printf/sscanf/fgets; no iostreams,
 *  no C++11 features, no std::string, no std::vector.
 *********************************************************************/

#ifndef DATAREADER_H
#define DATAREADER_H

#include <cstdio>    // printf, fprintf, fgets, sscanf, fopen, fclose
#include <cstdlib>   // malloc, free, strtol, EXIT_FAILURE, EXIT_SUCCESS
#include <cstring>   // strcmp, strncmp, strchr
#include <cctype>    // isspace
#include <limits>    // std::numeric_limits – overflow clamping
#include "uthash.h"  // single-header hash table

/* -----------------------------------------------------------------
 * Record – one line of the input file
 * ----------------------------------------------------------------- */
typedef struct {
    int id;                // Sequential identifier (0,1,2,…)
    int adc;               // ADC value (-1 if not read)
    int tdc;               // TDC value (-1 if not read)
    int scaler;            // Scaler value (-1 if not read)
    int adc_suffix;        // Integer after 'M' in ADC token   (-1 if none)
    int tdc_suffix;        // Integer after 'M' in TDC token   (-1 if none)
    int scaler_suffix;     // Integer after 'M' in Scaler token(-1 if none)
} Record;

/* -----------------------------------------------------------------
 * Hash entry structs
 * ----------------------------------------------------------------- */
typedef struct {
    int   adc;            // key
    Record *rec;          // value (pointer into the Record array)
    UT_hash_handle hh;    // uthash bookkeeping field
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
 * DataReader – header-only class
 * ================================================================= */
class DataReader {
private:
    /* ------------------- internal state -------------------------- */
    ADCEntry    * _adc_hash;
    TDCEntry    * _tdc_hash;
    ScalerEntry * _scaler_hash;

    Record      **records;           // dynamic array of Record*
    int          rec_cnt;            // number of records stored
    int          rec_cap;            // current capacity of the array
    const char  *filename;           // file supplied by the user

    int max_adc_suffix;
    int max_tdc_suffix;
    int max_scaler_suffix;
    int adc_number;
    int tdc_number;
    int scaler_number;
    
    /* ------ configurable token positions (1-based, 0 = ignore) ----- */
    int adc_pos;       // token number for ADC    (0 = do not read)
    int tdc_pos;       // token number for TDC    (0 = do not read)
    int scaler_pos;    // token number for Scaler (0 = do not read)

    /* ------------------- helper: integer from token ------------- */
    static int parseIntPrefix(const char *tok) {
        if (!tok) return -1;
        char *endptr = nullptr;
        long v = strtol(tok, &endptr, 10);
        if (endptr == tok) return -1;               // no digits at all ? “not found”
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

    /* ------------------- ensure the Record array can grow ------- */
    void ensureCapacity() {
        if (rec_cnt < rec_cap) return;               // already enough room
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

    /* ------------------- insert a Record into the required hashes */
    bool insertIntoHashes(Record *r) {
        if (adc_pos > 0) {
            ADCEntry *e = (ADCEntry*)malloc(sizeof(*e));
            if (!e) { perror("malloc ADCEntry"); return false; }
            e->adc = r->adc;
            e->rec = r;
            HASH_ADD_INT(_adc_hash, adc, e);
        }
        if (tdc_pos > 0) {
            TDCEntry *e = (TDCEntry*)malloc(sizeof(*e));
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
            ScalerEntry *e = (ScalerEntry*)malloc(sizeof(*e));
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

    /* ------------------- free all hash tables -------------------- */
    void freeHashes() {
        ADCEntry *ae, *tmp_ae;
        HASH_ITER(hh, _adc_hash, ae, tmp_ae) {
            HASH_DEL(_adc_hash, ae);
            free(ae);
        }
        _adc_hash = nullptr;

        TDCEntry *te, *tmp_te;
        HASH_ITER(hh, _tdc_hash, te, tmp_te) {
            HASH_DEL(_tdc_hash, te);
            free(te);
        }
        _tdc_hash = nullptr;

        ScalerEntry *se, *tmp_se;
        HASH_ITER(hh, _scaler_hash, se, tmp_se) {
            HASH_DEL(_scaler_hash, se);
            free(se);
        }
        _scaler_hash = nullptr;
    }

public:
    /* ------------------- ctor / dtor --------------------------- */
    explicit DataReader(const char *fname,
                        int adc_pos_=0,   int adc_number_=0,
                        int tdc_pos_=0,   int tdc_number_=0,
                        int scaler_pos_=0,int scaler_number_=0)
        : _adc_hash(nullptr), _tdc_hash(nullptr), _scaler_hash(nullptr),
          records(nullptr), rec_cnt(0), rec_cap(0), filename(fname),
          max_adc_suffix(-1), max_tdc_suffix(-1), max_scaler_suffix(-1),
          adc_pos(adc_pos_), tdc_pos(tdc_pos_), scaler_pos(scaler_pos_)
    {
      adc_number=adc_number_;
      tdc_number=tdc_number_;
      scaler_number=scaler_number_;
        // Note: adc_number_, tdc_number_, scaler_number_ are currently unused
        // but are passed through for future extensibility or documentation.
    }

    ~DataReader() {
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
        records = nullptr;
        rec_cnt = rec_cap = 0;
        max_adc_suffix = max_tdc_suffix = max_scaler_suffix = -1;

        FILE *fp = fopen(filename, "r");
        if (!fp) {
            fprintf(stderr, "Error: cannot open \"%s\"\n", filename);
            return false;
        }

        char line[1024];
        int lineId = 0;
        bool success = true;

        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "Element:", 8) != 0) continue;

            /* -------------------------------------------------
             * Split the line into up to 20 whitespace-separated tokens.
             * ------------------------------------------------- */
            char tokens[20][64] = {{0}};
            int n = sscanf(line,
                "Element: %63s %63s %63s %63s %63s %63s %63s %63s %63s %63s "
                "%63s %63s %63s %63s %63s %63s %63s %63s %63s %63s",
                tokens[0], tokens[1], tokens[2], tokens[3], tokens[4],
                tokens[5], tokens[6], tokens[7], tokens[8], tokens[9],
                tokens[10], tokens[11], tokens[12], tokens[13], tokens[14],
                tokens[15], tokens[16], tokens[17], tokens[18], tokens[19]);

            int max_needed = 0;
            if (adc_pos    > max_needed) max_needed = adc_pos;
            if (tdc_pos    > max_needed) max_needed = tdc_pos;
            if (scaler_pos > max_needed) max_needed = scaler_pos;

            if (max_needed > 0 && n < max_needed) {
                fprintf(stderr,
                        "Warning: line %d has only %d tokens (need %d) – skipping\n",
                        lineId + 1, n, max_needed);
                continue;
            }

            /* -------------------------------------------------
             * Extract the individual values.
             * ------------------------------------------------- */
            int adc_val   = -1, adc_suf   = -1;
            int tdc_val   = -1, tdc_suf   = -1;
            int scaler_val= -1, scaler_suf= -1;

            if (adc_pos > 0) {
                adc_val = parseIntPrefix(tokens[adc_pos - 1]);
                adc_suf = parseSuffix(tokens[adc_pos - 1]);
            }
            if (tdc_pos > 0) {
                tdc_val = parseIntPrefix(tokens[tdc_pos - 1]);
                tdc_suf = parseSuffix(tokens[tdc_pos - 1]);
            }
            if (scaler_pos > 0) {
                scaler_val = parseIntPrefix(tokens[scaler_pos - 1]);
                scaler_suf = parseSuffix(tokens[scaler_pos - 1]);
            }

            /* -------------------------------------------------
             * Allocate a new Record and fill it.
             * ------------------------------------------------- */
            Record *r = (Record*)malloc(sizeof(*r));
            if (!r) {
                perror("malloc Record");
                success = false;
                break;
            }
            r->id            = lineId++;
            r->adc           = adc_val;
            r->tdc           = tdc_val;
            r->scaler        = scaler_val;
            r->adc_suffix    = adc_suf;
            r->tdc_suffix    = tdc_suf;
            r->scaler_suffix = scaler_suf;

            ensureCapacity();
            records[rec_cnt++] = r;

            if (adc_pos > 0 && adc_suf > max_adc_suffix)    max_adc_suffix    = adc_suf;
            if (tdc_pos > 0 && tdc_suf > max_tdc_suffix)    max_tdc_suffix    = tdc_suf;
            if (scaler_pos > 0 && scaler_suf > max_scaler_suffix) max_scaler_suffix = scaler_suf;

            if (!insertIntoHashes(r)) {            // <-- correct name
                free(r);
                rec_cnt--;
                success = false;
                break;
            }
        }

        fclose(fp);

        if (!success) {
            for (int i = 0; i < rec_cnt; ++i) free(records[i]);
            rec_cnt = 0;
            freeHashes();
        }
        return success;
    }

    /* ------------------- look-ups ------------------------------ */
    Record *findADC(int adc) const {
        ADCEntry *e = nullptr;
        HASH_FIND_INT(_adc_hash, &adc, e);
        return e ? e->rec : nullptr;
    }

    Record *findTDC(int tdc) const {
        TDCEntry *e = nullptr;
        HASH_FIND_INT(_tdc_hash, &tdc, e);
        return e ? e->rec : nullptr;
    }

    Record *findScaler(int scaler) const {
        ScalerEntry *e = nullptr;
        HASH_FIND_INT(_scaler_hash, &scaler, e);
        return e ? e->rec : nullptr;
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

#endif /* DATAREADER_H */
