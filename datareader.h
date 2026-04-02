/*********************************************************************
 *  datareader.h
 *
 *  One-header implementation of the DataReader class.
 *
 *  * Reads lines that start with the literal ôElement:ö.
 *  * For each such line it
 *      û assigns a sequential id (0,1,2,à)
 *      û extracts two integer parameters **plus** the numeric ôM-suffixö
 *        that may follow each parameter:
 *          ò first_param   = numeric prefix of the *first* token after
 *                           ôElement:ö   (e.g. ô3015M1ö ? 3015)
 *          ò first_suffix  = numeric suffix after the æMÆ in that token
 *                           (e.g. ô3015M1ö ? 1,  ô2000ö ? û1)
 *          ò second_param  = numeric prefix of the *sixth* token after
 *                           ôElement:ö   (e.g. ô2032M0ö ? 2032)
 *          ò second_suffix = numeric suffix after the æMÆ in that token
 *                           (e.g. ô2032M0ö ? 0,  ô0.00ö ? û1)
 *  * Stores each line in a dynamically-grown C-array of Record*.
 *  * Maintains two uthash hash tables:
 *          first_hash  : first_param  ? Record*
 *          second_hash : second_param ? Record*
 *  * Public API
 *        û bool   readFile()          // parse the supplied file
 *        û Record* findFirst(int)    // lookup by first_param
 *        û Record* findSecond(int)   // lookup by second_param
 *        û int    getNumberOfElements() const   // number of stored records
 *        û int    getMaxFirstSuffix() const
 *        û int    getMaxSecondSuffix() const
 *
 *  The implementation deliberately uses only C-style I/O (printf/sscanf/fgets)
 *  and manual memory handling, as required by the original specification.
 *
 *  Compile example (GNU extensions required by uthash; no ûstd=c++11):
 *
 *      g++ -Wall -Wextra -O2 -o prog main.cpp
 *********************************************************************/

#ifndef DATAREADER_H
#define DATAREADER_H

/* -----------------------------------------------------------------
 *  Required C headers (still usable from C++)
 * ----------------------------------------------------------------- */
#include <cstdio>    // printf, fprintf, fgets, sscanf
#include <cstdlib>   // malloc, free, strtol, EXIT_FAILURE, EXIT_SUCCESS
#include <cstring>   // strcmp, strncmp, strchr
#include <cctype>    // isspace
#include <limits>    // std::numeric_limits û used for overflow clamping

/* -----------------------------------------------------------------
 *  uthash û single-header hash-table library.
 *  Place uthash.h in the same folder as this file.
 * ----------------------------------------------------------------- */
#include "uthash.h"

/* -----------------------------------------------------------------
 *  Record û one line of the input file.
 * ----------------------------------------------------------------- */
typedef struct {
    int id;                // Sequential identifier (0,1,2,à)
    int first_param;       // Numeric prefix of tokená1
    int second_param;      // Numeric prefix of tokená6
    int first_suffix;      // Numeric part after æMÆ in tokená1 (-1 = none)
    int second_suffix;     // Numeric part after æMÆ in tokená6 (-1 = none)
} Record;

/* -----------------------------------------------------------------
 *  FirstParamEntry û hash entry: first_param ? Record*.
 * ----------------------------------------------------------------- */
typedef struct {
    int first_param;      // key
    Record *rec;          // value (pointer into the Record array)
    UT_hash_handle hh;    // required uthash bookkeeping field
} FirstParamEntry;

/* -----------------------------------------------------------------
 *  SecondParamEntry û hash entry: second_param ? Record*.
 * ----------------------------------------------------------------- */
typedef struct {
    int second_param;     // key
    Record *rec;          // value
    UT_hash_handle hh;    // required uthash bookkeeping field
} SecondParamEntry;

/* =================================================================
 *  DataReader û public class (implementation lives entirely in this
 *               header; the interactive loop can be in a separate .cpp)
 * ================================================================= */
class DataReader {
private:
    /* ------------------- internal state -------------------------- */
    FirstParamEntry  *first_hash;        // hash: first_param  ? Record*
    SecondParamEntry *second_hash;       // hash: second_param ? Record*
    Record          **records;           // C-style dynamic array of Record*
    int               rec_cnt;           // number of records stored
    int               rec_cap;           // current capacity of the array
    const char       *filename;          // file supplied by the user
    int               max_first_suffix;  // maximum suffix seen for first_param
    int               max_second_suffix; // maximum suffix seen for second_param

    /* ------------------- helper: integer from token ------------- */
    /** Returns the leading integer of a token (e.g. ô3015M1ö ? 3015).
     *  Returns 0 if no leading digits are present.
     */
    static int parseIntPrefix(const char *tok) {
        if (!tok) return 0;
        char *endptr = NULL;
        long v = strtol(tok, &endptr, 10);
        if (endptr == tok) return 0;                     // no digits at all
        if (v > std::numeric_limits<int>::max())
            return std::numeric_limits<int>::max();
        if (v < std::numeric_limits<int>::min())
            return std::numeric_limits<int>::min();
        return (int)v;
    }

    /** Returns the integer that follows an æMÆ (or æmÆ) in a token.
     *  Example: ô3015M1ö ? 1,  ô2032M0ö ? 0,  ô2.000ö ? û1.
     *  Returns û1 if no æMÆ is present.
     */
    static int parseSuffix(const char *tok) {
        if (!tok) return -1;
        const char *m = strchr(tok, 'M');
        if (!m) m = strchr(tok, 'm');
        if (!m) return -1;
        return parseIntPrefix(m + 1);
    }

    /* ------------------- ensure the Record array can grow ------- */
    /** Grows the `records` pointer array when needed.
     *  Capacity is doubled each time (starting from 128).
     */
    void ensureCapacity() {
        if (rec_cnt < rec_cap) return;           // already enough room
        int new_cap = rec_cap ? rec_cap * 2 : 128;
        Record **new_arr = (Record **)malloc(new_cap * sizeof(Record *));
        if (!new_arr) {
            fprintf(stderr,
                    "Error: malloc failed while resizing records (requested %d entries)\n",
                    new_cap);
            exit(EXIT_FAILURE);
        }
        for (int i = 0; i < rec_cnt; ++i) new_arr[i] = records[i];
        if (records) free(records);
        records = new_arr;
        rec_cap = new_cap;
    }

    /* ------------------- insert a Record into both hashes -------- */
    /** Returns true on success, false on allocation failure.
     *  In case the second allocation fails the first hash entry is
     *  removed and freed to avoid a leak.
     */
    bool insertIntoHashes(Record *r) {
        /* ---- first_hash ---- */
        FirstParamEntry *f = (FirstParamEntry *)malloc(sizeof(*f));
        if (!f) {
            perror("malloc FirstParamEntry");
            return false;
        }
        f->first_param = r->first_param;
        f->rec = r;
        HASH_ADD_INT(first_hash, first_param, f);   // key = first_param

        /* ---- second_hash ---- */
        SecondParamEntry *s = (SecondParamEntry *)malloc(sizeof(*s));
        if (!s) {
            perror("malloc SecondParamEntry");
            HASH_DEL(first_hash, f);
            free(f);
            return false;
        }
        s->second_param = r->second_param;
        s->rec = r;
        HASH_ADD_INT(second_hash, second_param, s); // key = second_param

        return true;
    }

    /* ------------------- free both hash tables ----------------- */
    /** Walks each hash table, removes every entry, and frees its memory. */
    void freeHashes() {
        FirstParamEntry *f, *tmp_f;
        HASH_ITER(hh, first_hash, f, tmp_f) {
            HASH_DEL(first_hash, f);
            free(f);
        }
        first_hash = NULL;

        SecondParamEntry *s, *tmp_s;
        HASH_ITER(hh, second_hash, s, tmp_s) {
            HASH_DEL(second_hash, s);
            free(s);
        }
        second_hash = NULL;
    }

public:
    /* ------------------- ctor / dtor --------------------------- */
    explicit DataReader(const char *fname)
        : first_hash(NULL), second_hash(NULL),
          records(NULL), rec_cnt(0), rec_cap(0), filename(fname),
          max_first_suffix(-1), max_second_suffix(-1) {}

    ~DataReader() {
        freeHashes();                         // hash entries first
        for (int i = 0; i < rec_cnt; ++i)     // then each Record struct
            free(records[i]);
        if (records) free(records);           // finally the pointer array
    }

    /* ------------------- read the input file ------------------ */
    /** Parses the file given at construction time.
     *  Returns true on success, false on any fatal error (file-open,
     *  malloc failure, à).  Malformed lines are skipped with a warning.
     */
    bool readFile() {
        /* ---- Reset previous state ------------------------------------ */
        freeHashes();
        for (int i = 0; i < rec_cnt; ++i) free(records[i]);
        if (records) free(records);
        records = NULL;
        rec_cnt = 0;
        rec_cap = 0;
        max_first_suffix  = -1;
        max_second_suffix = -1;

        /* ---- Open the file ------------------------------------------- */
        FILE *fp = fopen(filename, "r");
        if (!fp) {
            fprintf(stderr, "Error: cannot open \"%s\"\n", filename);
            return false;
        }

        char line[1024];
        int lineId = 0;          // sequential id for each accepted line
        bool success = true;

        /* ---- Process the file line-by-line --------------------------- */
        while (fgets(line, sizeof(line), fp)) {
            /* Keep only lines that start with "Element:" */
            if (strncmp(line, "Element:", 8) != 0) continue;

            /* Tokenise the first ten whitespace-separated fields */
            char t1[64], t2[64], t3[64], t4[64], t5[64],
                 t6[64], t7[64], t8[64], t9[64], t10[64] = {0};

            int n = sscanf(line,
                "Element: %63s %63s %63s %63s %63s %63s %63s %63s %63s %63s",
                t1, t2, t3, t4, t5, t6, t7, t8, t9, t10);

            if (n < 2) {
                fprintf(stderr,
                        "Warning: line %d has too few fields (expected at least 2) û skipping\n",
                        lineId + 1);
                continue;
            }

            /* Extract numeric parameters and their ôM-suffixesö */
            int first_param   = parseIntPrefix(t1);
            int first_suffix  = parseSuffix(t1);
            int second_param  = parseIntPrefix(t6);
            int second_suffix = parseSuffix(t6);

            /* Allocate a new Record */
            Record *r = (Record *)malloc(sizeof(*r));
            if (!r) {
                perror("malloc Record");
                success = false;
                break;
            }
            r->id            = lineId++;
            r->first_param   = first_param;
            r->second_param  = second_param;
            r->first_suffix  = first_suffix;
            r->second_suffix = second_suffix;

            /* Store it in the dynamic array (grow if necessary) */
            ensureCapacity();
            records[rec_cnt++] = r;

            /* Update max-suffix trackers */
            if (first_suffix > max_first_suffix)   max_first_suffix   = first_suffix;
            if (second_suffix > max_second_suffix) max_second_suffix  = second_suffix;

            /* Insert the Record into both hash tables */
            if (!insertIntoHashes(r)) {
                /* Allocation inside insertIntoHashes failed û clean up */
                free(r);
                rec_cnt--;               // remove the partially-added entry
                success = false;
                break;
            }
        }

        fclose(fp);

        /* ---- Final reporting ------------------------------------------ */
        if (success) {
            printf("Loaded %d records from \"%s\"\n", rec_cnt, filename);
        } else {
            for (int i = 0; i < rec_cnt; ++i) free(records[i]);
            rec_cnt = 0;
            freeHashes();
        }

        return success;
    }

    /* ------------------- lookup by first_param ---------------- */
    /** Returns a pointer to the Record whose first_param equals `fp`,
     *  or NULL if no such record exists.
     */
    Record *findFirst(int fp) {
        FirstParamEntry *out = NULL;
        ({ HASH_FIND_INT(first_hash, &fp, out); });
        return out ? out->rec : NULL;
    }

    /* ------------------- lookup by second_param --------------- */
    /** Returns a pointer to the Record whose second_param equals `sp`,
     *  or NULL if no such record exists.
     */
    Record *findSecond(int sp) {
        SecondParamEntry *out = NULL;
        ({ HASH_FIND_INT(second_hash, &sp, out); });
        return out ? out->rec : NULL;
    }

    /* ------------------- public helpers ------------------------ */
    /** Number of records that have been successfully loaded. */
    int getNumberOfElements() const { return rec_cnt; }

    /** Maximum M-suffix seen for the first parameter (-1 = none). */
    int getMaxFirstSuffix() const { return max_first_suffix; }

    /** Maximum M-suffix seen for the second parameter (-1 = none). */
    int getMaxSecondSuffix() const { return max_second_suffix; }

    /** Return both suffixes together (convenient for callers). */
    struct MaxSuffixInfo {
        int maxFirst;
        int maxSecond;
    };
    MaxSuffixInfo getMaxSuffixInfo() const {
        MaxSuffixInfo info = { max_first_suffix, max_second_suffix };
        return info;
    }
};

#endif /* DATAREADER_H */

