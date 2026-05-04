#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <unordered_map>
#include <vector>
#include <string>
#include <climits>
#include <cctype>
#include <algorithm>

// ============================================================
// DetectorType
// ============================================================
// This is an enum: a list of named integer constants.
//
// Instead of using numbers like 0,1,2,... we use names
// so the code is easier to read and less error-prone.
enum DetectorType {
  D_NONE = 0,

  // Each detector type corresponds to a hardware subsystem
  D_TAGGER_TDC,
  D_TAGGER_SCALER,
  D_MWPC_W_TDC,
  D_MWPC_S_ADC,
  D_PID_ADC,
  D_PID_TDC,
  D_CB_ADC,
  D_CB_TDC,
  D_VETO_ADC,
  D_VETO_TDC,
  D_BAF2_S_N_ADC,
  D_BAF2_S_S_ADC,
  D_BAF2_L_N_ADC,
  D_BAF2_L_S_ADC,
  D_BAF2_TDC,
  D_PBWO4_ADC,
  D_PBWO4_S_ADC,
  D_PBWO4_TDC,
  D_SCALER,

  N_DETECTORS // total number of detector types
};

// ============================================================
// Helper function: detector name -> string
// ============================================================
// This is used only for printing/logging.
// Example: D_CB_ADC -> "CB_ADC"
inline const char* getDetectorName(int det) {
  switch (det) {
    case D_TAGGER_TDC: return "TAGGER_TDC";
    case D_TAGGER_SCALER: return "TAGGER_SCALER";
    case D_MWPC_W_TDC: return "MWPC_W_TDC";
    case D_MWPC_S_ADC: return "MWPC_S_ADC";
    case D_PID_ADC: return "PID_ADC";
    case D_PID_TDC: return "PID_TDC";
    case D_CB_ADC: return "CB_ADC";
    case D_CB_TDC: return "CB_TDC";
    case D_VETO_ADC: return "VETO_ADC";
    case D_VETO_TDC: return "VETO_TDC";
    case D_BAF2_S_N_ADC: return "BAF2_S_N_ADC";
    case D_BAF2_S_S_ADC: return "BAF2_S_S_ADC";
    case D_BAF2_L_N_ADC: return "BAF2_L_N_ADC";
    case D_BAF2_L_S_ADC: return "BAF2_L_S_ADC";
    case D_BAF2_TDC: return "BAF2_TDC";
    case D_PBWO4_ADC: return "PBWO4_ADC";
    case D_PBWO4_S_ADC: return "PBWO4_S_ADC";
    case D_PBWO4_TDC: return "PBWO4_TDC";
    case D_SCALER: return "SCALER";
    default: return "UNKNOWN";
  }
}

// ============================================================
// DetectorInfo
// ============================================================
// This struct stores ALL configuration for one detector.
//
// Think of it as a "configuration block" for a detector.
struct DetectorInfo {

  // How many lines to skip in the .dat file before reading useful data
  int offset = 0;

  // Which column in the file contains the detector ID
  int column = 0;

  // Number of channels for this detector
  int detch = 0;

  // Maximum number of hits allowed per channel
  int maxMultihit = 0;

  // Special field used for data interpretation (DAQ-specific meaning)
  int whereisdata = 0;

  // Maps:
  // ID -> channel index
  std::unordered_map<int, int> idToCh;

  // channel index -> ID (reverse lookup)
  std::unordered_map<int, int> chToId;

  // List of .dat files used for this detector
  std::vector<std::string> datFiles;
};

// ============================================================
// ConfigManager
// ============================================================
// This class reads configuration files and provides lookup functions.
//
// It connects:
//   - detector IDs
//   - channels
//   - reference timing channels
class ConfigManager {

private:

  // Controls how much debug output is printed
  int verboselvl = 0;

  // Reference system:
  // Each detector may have a reference channel used for timing corrections

  std::vector<int> ref_id;                 // reference IDs
  std::vector<unsigned int> ref_data;      // stored values for references

public:

  // ============================================================
  // Main storage
  // ============================================================

  // Detector configurations indexed by detector type
  std::map<int, DetectorInfo> detectors;

  // For a given ID, which detectors use it
  std::unordered_map<int, std::vector<int>> idToDetector;

  // Mapping: channel -> reference ID
  std::unordered_map<int, int> tdcRefMap;

  // ============================================================
  // Read config file
  // ============================================================
  // This function loads a configuration file line by line.
  int readConfig(const char* filename, int verboselvl_=0) {

    FILE* file = fopen(filename, "r");
    if (!file) {
      printf("Cannot open config: %s\n", filename);
      return 0;
    }

    verboselvl = verboselvl_;

    // Clear old reference data before loading new config
    ref_id.clear();
    ref_data.clear();

    char line[512];

    // Read file line-by-line
    while (fgets(line, sizeof(line), file)) {

      // Skip empty or comment lines
      if (line[0] == '\0' || line[0] == '#')
        continue;

      char word[50];

      // Read first word in the line
      if (sscanf(line, "%49s", word) != 1)
        continue;

      // Try to interpret line as detector definition
      int det = detectorFromString(word);

      if (det != D_NONE) {

        int offset, detch, column, maxMultihit;
        char datFile[256];

        // Parse detector line
        if (sscanf(line, "%*s %d %d %d %d %255s",
                   &offset, &detch, &column, &maxMultihit, datFile) == 5) {

          DetectorInfo &info = detectors[det];

          info.offset = offset;
          info.detch = detch;
          info.column = column;
          info.maxMultihit = maxMultihit;

          // Store file name (a detector can have multiple files)
          info.datFiles.push_back(datFile);

          // Read mapping from file
          readDatFile(det, datFile);
        }

        continue;
      }

      // ============================================================
      // Handle reference definition
      // ============================================================
      if (strcmp(word, "TDC_REF") == 0) {

        int start, stop, ref;

        if (sscanf(line, "%*s %d %d %d", &start, &stop, &ref) == 3) {

          // Assign same reference ID to a range of channels
          for (int id = start; id <= stop; ++id)
            tdcRefMap[id] = ref;

          // Store reference bookkeeping
          ref_id.push_back(ref);
          ref_data.push_back(0);
        }
      }
    }

    fclose(file);
    return 1;
  }

  // ============================================================
  // Read .dat file
  // ============================================================
  // Each detector has a .dat file describing channel mapping
  void readDatFile(int det, const char* datFile) {

    FILE* file = fopen(datFile, "r");
    if (!file) {
      printf("Cannot open dat file: %s\n", datFile);
      return;
    }

    char line[512];

    int ch = 0;       // channel index
    int skipped = 0;  // number of skipped lines

    while (fgets(line, sizeof(line), file)) {

      // Skip header lines
      if (skipped < detectors[det].offset) {
        ++skipped;
        continue;
      }

      // Stop if we reached expected number of channels
      if (ch == detectors[det].detch)
        break;

      // Only process valid lines
      if (strncmp(line, "Element:", 8) == 0) {

        int whereisdata = 0; // parsed extra info from ID string

        // Split line into tokens (words)
        char* tokens[50];
        int token_count = 0;

        char* token = strtok(line, " \t");
        while (token && token_count < 50) {
          tokens[token_count++] = token;
          token = strtok(NULL, " \t");
        }

        // Safety check
        if (detectors[det].column >= token_count)
          continue;

        // Extract ID string from chosen column
        char idStr[50];
        snprintf(idStr, sizeof(idStr), "%s",
                 tokens[detectors[det].column]);

        int len = strlen(idStr);

        // ------------------------------------------------------------
        // Extract numeric ID prefix (e.g. "123A" -> 123)
        // ------------------------------------------------------------
        int id = 0;
        int i = 0;
        while (i < len && isdigit(idStr[i])) {
          id = id * 10 + (idStr[i] - '0');
          i++;
        }

        // ------------------------------------------------------------
        // Extract trailing digits (custom encoding)
        // ------------------------------------------------------------
        int j = len - 1;
        int mult = 1;
        while (j >= 0 && isdigit(idStr[j])) {
          whereisdata += (idStr[j] - '0') * mult;
          mult *= 10;
          j--;
        }

        // Store mappings
        detectors[det].idToCh[id] = ch;
        detectors[det].chToId[ch] = id;
        detectors[det].whereisdata = whereisdata;

        // Track which detectors use this ID
        auto &vec = idToDetector[id];
        if (std::find(vec.begin(), vec.end(), det) == vec.end())
          vec.push_back(det);

        ++ch;
      }
    }

    fclose(file);
  }

  // ============================================================
  // SAFE LOOKUP FUNCTIONS (never crash)
  // ============================================================

  int getChannel(int det, int id) const {
    auto dit = detectors.find(det);
    if (dit == detectors.end()) return -1;

    const auto& map = dit->second.idToCh;
    auto it = map.find(id);

    return (it != map.end()) ? it->second : -1;
  }

  int getMaxHits(int det) const {
    auto dit = detectors.find(det);
    if (dit == detectors.end()) return -1;
    return dit->second.maxMultihit;
  }

  int getDataSource(int det) const {
    auto dit = detectors.find(det);
    if (dit == detectors.end()) return -1;
    return dit->second.whereisdata;
  }

  int getNoCh(int det) const {
    auto dit = detectors.find(det);
    if (dit == detectors.end()) return -1;
    return dit->second.detch;
  }

  int getBackId(int det, int ch) const {
    auto dit = detectors.find(det);
    if (dit == detectors.end()) return -1;

    const auto& map = dit->second.chToId;
    auto it = map.find(ch);

    return (it != map.end()) ? it->second : -1;
  }

  int getRefId(int ch) const {
    auto it = tdcRefMap.find(ch);
    return (it != tdcRefMap.end()) ? it->second : -1;
  }

  // ============================================================
  // Reference data handling
  // ============================================================

  void store_ref_data(int id, unsigned int data) {
    for (size_t i = 0; i < ref_id.size(); ++i) {
      if (ref_id[i] == id) {
        ref_data[i] = data;
        return;
      }
    }
  }

  unsigned int get_ref_data(int id) const {

    int refid = getRefId(id);
    if (refid == -1) return UINT_MAX;

    for (size_t i = 0; i < ref_id.size(); ++i) {
      if (ref_id[i] == refid)
        return ref_data[i];
    }

    return UINT_MAX;
  }

  void reset_ref_data() {
    for (auto &v : ref_data)
      v = 0;
  }

  // ============================================================
  // Convert string to detector enum
  // ============================================================
  int detectorFromString(const char* word) const {

#define CMP(x) if (strcmp(word, #x) == 0) return D_##x;

    CMP(TAGGER_TDC)
    CMP(TAGGER_SCALER)
    CMP(MWPC_W_TDC)
    CMP(MWPC_S_ADC)
    CMP(PID_ADC)
    CMP(PID_TDC)
    CMP(CB_ADC)
    CMP(CB_TDC)
    CMP(VETO_ADC)
    CMP(VETO_TDC)
    CMP(BAF2_S_N_ADC)
    CMP(BAF2_S_S_ADC)
    CMP(BAF2_L_N_ADC)
    CMP(BAF2_L_S_ADC)
    CMP(BAF2_TDC)
    CMP(PBWO4_ADC)
    CMP(PBWO4_S_ADC)
    CMP(PBWO4_TDC)
    CMP(SCALER)

    return D_NONE;
  }
};

#endif
