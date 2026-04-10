#ifndef PARAMREADER_H
#define PARAMREADER_H

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <limits>
#include <algorithm>   // <-- REQUIRED for std::max
#include "UtHash.h"

/* ---------------- Record ---------------- */
typedef struct {
    int id;
    int adc;
    int tdc;
    int scaler;
    int adc_suffix;
    int tdc_suffix;
    int scaler_suffix;
} Record;

/* ---------------- Hash entries ---------------- */
typedef struct {
    int adc;
    Record *rec;
    UT_hash_handle hh;
} ADCEntry;

typedef struct {
    int tdc;
    Record *rec;
    UT_hash_handle hh;
} TDCEntry;

typedef struct {
    int scaler;
    Record *rec;
    UT_hash_handle hh;
} ScalerEntry;

/* ================================================= */
class ParamReader {
private:
    ADCEntry    *_adc_hash;
    TDCEntry    *_tdc_hash;
    ScalerEntry *_scaler_hash;

    Record      **records;
    int rec_cnt;
    int rec_cap;

    const char  *filename;

    int max_adc_suffix;
    int max_tdc_suffix;
    int max_scaler_suffix;

    // ✅ FIX: missing members
    int adc_maxhits;
    int tdc_maxhits;
    int scaler_maxhits;

    int adc_pos;
    int tdc_pos;
    int scaler_pos;

    bool _emptyFile;

    /* ---------------- helpers ---------------- */

    static int parseIntPrefix(const char *tok) {
        if (!tok) return -1;
        char *endptr = NULL;
        long v = strtol(tok, &endptr, 10);
        if (endptr == tok) return -1;

        if (v > std::numeric_limits<int>::max())
            return std::numeric_limits<int>::max();
        if (v < std::numeric_limits<int>::min())
            return std::numeric_limits<int>::min();

        return (int)v;
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
            fprintf(stderr, "Out of memory\n");
            exit(EXIT_FAILURE);
        }

        if (records)
            memcpy(new_arr, records, rec_cnt * sizeof(Record*)); // ✅ faster copy

        free(records);
        records = new_arr;
        rec_cap = new_cap;
    }

    bool insertIntoHashes(Record *r) {
        // -------- ADC --------
        if (adc_pos > 0) {
            ADCEntry *existing = NULL;
            HASH_FIND_INT(_adc_hash, &r->adc, existing);

            if (existing) {
                existing->rec = r; // replace
            } else {
                ADCEntry *e = (ADCEntry*)malloc(sizeof(ADCEntry));
                if (!e) return false;
                e->adc = r->adc;
                e->rec = r;
                HASH_ADD_INT(_adc_hash, adc, e);
            }
        }

        // -------- TDC --------
        if (tdc_pos > 0) {
            TDCEntry *existing = NULL;
            HASH_FIND_INT(_tdc_hash, &r->tdc, existing);

            if (existing) {
                existing->rec = r;
            } else {
                TDCEntry *e = (TDCEntry*)malloc(sizeof(TDCEntry));
                if (!e) return false;
                e->tdc = r->tdc;
                e->rec = r;
                HASH_ADD_INT(_tdc_hash, tdc, e);
            }
        }

        // -------- SCALER --------
        if (scaler_pos > 0) {
            ScalerEntry *existing = NULL;
            HASH_FIND_INT(_scaler_hash, &r->scaler, existing);

            if (existing) {
                existing->rec = r;
            } else {
                ScalerEntry *e = (ScalerEntry*)malloc(sizeof(ScalerEntry));
                if (!e) return false;
                e->scaler = r->scaler;
                e->rec = r;
                HASH_ADD_INT(_scaler_hash, scaler, e);
            }
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
    /* ---------------- constructor ---------------- */
    ParamReader()
        : _adc_hash(NULL), _tdc_hash(NULL), _scaler_hash(NULL),
          records(NULL), rec_cnt(0), rec_cap(0), filename(NULL),
          max_adc_suffix(-1), max_tdc_suffix(-1), max_scaler_suffix(-1),
          adc_maxhits(0), tdc_maxhits(0), scaler_maxhits(0),
          adc_pos(0), tdc_pos(0), scaler_pos(0),
          _emptyFile(true)
    {}

    /* ---------------- init ---------------- */
    void init(const char *fname,
              int adc_pos_=0,   int adc_maxhits_=0,
              int tdc_pos_=0,   int tdc_maxhits_=0,
              int scaler_pos_=0,int scaler_maxhits_=0)
    {
        freeHashes();

        for (int i = 0; i < rec_cnt; ++i)
            free(records[i]);
        free(records);

        _adc_hash = NULL;
	    _tdc_hash = NULL;
	    _scaler_hash = NULL;
	   
	    records = NULL;
        rec_cnt = rec_cap = 0;

        filename = fname;

        max_adc_suffix = max_tdc_suffix = max_scaler_suffix = -1;

        adc_maxhits = adc_maxhits_;
        tdc_maxhits = tdc_maxhits_;
        scaler_maxhits = scaler_maxhits_;

        adc_pos = adc_pos_;
        tdc_pos = tdc_pos_;
        scaler_pos = scaler_pos_;

        _emptyFile = (!fname || fname[0] == '\0');
    }

    /* ---------------- destructor ---------------- */
    ~ParamReader() {
        freeHashes();
        for (int i = 0; i < rec_cnt; ++i)
            free(records[i]);
        free(records);
    }

    /* ---------------- read file ---------------- */
    bool readFile() {
        freeHashes();

        for (int i = 0; i < rec_cnt; ++i)
            free(records[i]);
        free(records);

        records = NULL;
        rec_cnt = rec_cap = 0;

        max_adc_suffix = max_tdc_suffix = max_scaler_suffix = -1;

        if (!filename || filename[0] == '\0') {
            _emptyFile = true;
            return true;
        }

        FILE *fp = fopen(filename, "r");
        if (!fp) {
            printf("Error: cannot open \"%s\"\n", filename);
            _emptyFile = false;
            return false;
        }

        _emptyFile = false;

        char line[1024];
        int elementLine = 0;

        while (fgets(line, sizeof(line), fp)) {

            if (strncmp(line, "Element:", 8) != 0)
                continue;

            char *tok = strtok(line, " \t\r\n");
            if (!tok || strcmp(tok, "Element:") != 0)
                continue;

            char *tokens[25];
            int tokenCount = 0;

            while ((tok = strtok(NULL, " \t\r\n")) && tokenCount < 25)
                tokens[tokenCount++] = tok;

            int adc_val=-1, tdc_val=-1, scaler_val=-1;
            int adc_suf=-1, tdc_suf=-1, scaler_suf=-1;

            if (adc_pos > 0 && tokenCount >= adc_pos) {
                adc_val = parseIntPrefix(tokens[adc_pos - 1]);
                adc_suf = parseSuffix(tokens[adc_pos - 1]);
            }
            if (tdc_pos > 0 && tokenCount >= tdc_pos) {
                tdc_val = parseIntPrefix(tokens[tdc_pos - 1]);
                tdc_suf = parseSuffix(tokens[tdc_pos - 1]);
            }
            if (scaler_pos > 0 && tokenCount >= scaler_pos) {
                scaler_val = parseIntPrefix(tokens[scaler_pos - 1]);
                scaler_suf = parseSuffix(tokens[scaler_pos - 1]);
            }

            Record *rec = (Record*)malloc(sizeof(Record));
            if (!rec) {
                fclose(fp);
                return false;
            }

            rec->id = elementLine++;
            rec->adc = adc_val;
            rec->tdc = tdc_val;
            rec->scaler = scaler_val;
            rec->adc_suffix = adc_suf;
            rec->tdc_suffix = tdc_suf;
            rec->scaler_suffix = scaler_suf;

            ensureCapacity();
            records[rec_cnt++] = rec;

            if (adc_suf > max_adc_suffix) max_adc_suffix = adc_suf;
            if (tdc_suf > max_tdc_suffix) max_tdc_suffix = tdc_suf;
            if (scaler_suf > max_scaler_suffix) max_scaler_suffix = scaler_suf;

            if (!insertIntoHashes(rec)) {
                fclose(fp);
                return false;
            }
        }

        fclose(fp);
        return true;
    }

    /* ---------------- lookups ---------------- */
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

    /* ---------------- info ---------------- */
    int getNumberOfElements() const { return rec_cnt; }

    int getADCMaxHits() const {
        return std::max(max_adc_suffix + 1, adc_maxhits);
    }

    int getTDCMaxHits() const {
        return std::max(max_tdc_suffix + 1, tdc_maxhits);
    }

    int getSCALERMaxHits() const {
        return std::max(max_scaler_suffix + 1, scaler_maxhits);
    }
};

#endif
