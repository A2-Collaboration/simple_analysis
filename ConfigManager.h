#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <unordered_map>
#include <vector>
#include <array>
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

  N_DETECTORS
};


// ============================================================
// Helper function: detector name -> string
// ============================================================
// This is used only for printing/logging.
// Example: D_CB_ADC -> "CB_ADC"
inline const char* getDetectorName(int det)
{
  switch(det)
  {
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
struct DetectorInfo
{

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
  std::unordered_map<int,int> idToCh;


  // channel index -> ID (reverse lookup)
  std::unordered_map<int,int> chToId;


  // List of .dat files used for this detector
  std::vector<std::string> datFiles;



  // ============================================================
  // Neighbour information
  // ============================================================
  //
  // neighbours[element][n]
  //
  // Stores the neighbour channels/elements of one detector element.
  //
  // The first dimension is the detector element number.
  // The second dimension always has 12 possible neighbours.
  //
  // Example:
  //
  // Next-Neighbour: 13 1 0 2 3 5 ...
  //
  // creates:
  //
  // neighbours[1][0] = 0
  // neighbours[1][1] = 2
  // neighbours[1][2] = 3
  //
  // Unused entries are initialized to -1.
  //
  // ============================================================

  std::vector<std::array<int,12>> neighbours;

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
class ConfigManager
{

private:

  // Controls how much debug output is printed
  int verboselvl = 0;


  // Reference system:
  // Each detector may have a reference channel used for timing corrections

  std::vector<int> ref_id;
  std::vector<unsigned int> ref_data;



public:


  // ============================================================
  // Main storage
  // ============================================================

  // Detector configurations indexed by detector type
  std::map<int,DetectorInfo> detectors;


  // For a given ID, which detectors use it
  std::unordered_map<int,std::vector<int>> idToDetector;


  // Mapping: channel -> reference ID
  std::unordered_map<int,int> tdcRefMap;



  // ============================================================
  // Read config file
  // ============================================================

  int readConfig(const char* filename, int verboselvl_=0)
  {

    FILE* file = fopen(filename,"r");

    if(!file)
    {
      printf("Cannot open config: %s\n",filename);
      return 0;
    }


    verboselvl = verboselvl_;


    // Clear old reference data before loading new config
    ref_id.clear();
    ref_data.clear();


    char line[512];


    // Read file line-by-line
    while(fgets(line,sizeof(line),file))
    {

      // Skip empty or comment lines
      if(line[0]=='\0' || line[0]=='#')
        continue;


      char word[50];


      // Read first word in the line
      if(sscanf(line,"%49s",word)!=1)
        continue;


      int det = detectorFromString(word);


      if(det != D_NONE)
      {

        int offset;
        int detch;
        int column;
        int maxMultihit;

        char datFile[256];


        if(sscanf(line,
                  "%*s %d %d %d %d %255s",
                  &offset,
                  &detch,
                  &column,
                  &maxMultihit,
                  datFile)==5)
        {

          DetectorInfo &info = detectors[det];


          info.offset = offset;
          info.detch = detch;
          info.column = column;
          info.maxMultihit = maxMultihit;


          // Store file name (a detector can have multiple files)
          info.datFiles.push_back(datFile);


          // Read mapping from file
          readDatFile(det,datFile);

        }


        continue;

      }



      // ============================================================
      // Handle reference definition
      // ============================================================

      if(strcmp(word,"TDC_REF")==0)
      {

        int start;
        int stop;
        int ref;


        if(sscanf(line,"%*s %d %d %d",
                  &start,
                  &stop,
                  &ref)==3)
        {

          // Assign same reference ID to a range of channels
          for(int id=start; id<=stop; ++id)
            tdcRefMap[id]=ref;


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
  //
  // This function now also reads:
  //
  // Next-Neighbour:
  //
  // Example:
  //
  // Next-Neighbour: 13 1 0 2 3 5 6 4 146 145 144 576 577 579
  //
  // Meaning:
  //
  //   13  = number of entries including the element itself
  //   1   = detector element
  //   remaining numbers = neighbouring elements
  //
  // The neighbour array is stored as:
  //
  //   neighbours[element][0..11]
  //
  // Unused positions contain -1.
  //
  // ============================================================

void readDatFile(int det, const char* datFile)
{
    // ------------------------------------------------------------
    // Open detector description file
    // ------------------------------------------------------------

    FILE* file = fopen(datFile, "r");

    if (!file)
    {
        printf("Cannot open dat file: %s\n", datFile);
        return;
    }


    // Shortcut to the detector configuration

    DetectorInfo &info = detectors[det];


    // ============================================================
    // Allocate neighbour table
    //
    // One entry is created for every detector element.
    //
    // Each element has room for up to 12 neighbours.
    //
    // Initialise every neighbour slot with -1,
    // meaning "no neighbour assigned".
    // ============================================================

    info.neighbours.resize(info.detch);

    for (auto &n : info.neighbours)
        n.fill(-1);


    char line[512];


    // ============================================================
    // First pass through the file
    //
    // Only the Element: records are processed.
    //
    // The loop terminates after all detector channels have been
    // read, even if additional information follows afterwards.
    //
    // This keeps the original behaviour for detector mapping.
    // ============================================================

    int ch = 0;
    int skipped = 0;

    while (fgets(line, sizeof(line), file))
    {
        // --------------------------------------------------------
        // Skip header lines defined by the detector configuration.
        // --------------------------------------------------------

        if (skipped < info.offset)
        {
            ++skipped;
            continue;
        }


        // --------------------------------------------------------
        // Stop after the expected number of detector elements has
        // been processed.
        //
        // The neighbour information is read later during a second
        // pass through the file.
        // --------------------------------------------------------

        if (ch == info.detch)
            break;


        // Ignore everything except Element: lines.

        if (strncmp(line, "Element:", 8) != 0)
            continue;


        int whereisdata = 0;


        // --------------------------------------------------------
        // Split the complete line into whitespace-separated tokens.
        // --------------------------------------------------------

        char* tokens[50];
        int token_count = 0;

        char* token = strtok(line, " \t");

        while (token && token_count < 50)
        {
            tokens[token_count++] = token;
            token = strtok(NULL, " \t");
        }


        // Make sure the configured column exists.

        if (info.column >= token_count)
            continue;


        // --------------------------------------------------------
        // Extract the detector identifier string.
        //
        // Example:
        //
        // 123A45
        // --------------------------------------------------------

        char idStr[50];

        snprintf(idStr,
                 sizeof(idStr),
                 "%s",
                 tokens[info.column]);


        int len = strlen(idStr);


        // --------------------------------------------------------
        // Read the leading numeric part.
        //
        // Example:
        //
        // 123A45  -> 123
        // --------------------------------------------------------

        int id = 0;
        int i = 0;

        while (i < len && isdigit(idStr[i]))
        {
            id = id * 10 + (idStr[i] - '0');
            ++i;
        }


        // --------------------------------------------------------
        // Read the trailing numeric part.
        //
        // Example:
        //
        // 123A45 -> 45
        //
        // This DAQ-specific value indicates where the detector
        // data are stored.
        // --------------------------------------------------------

        int j = len - 1;
        int mult = 1;

        while (j >= 0 && isdigit(idStr[j]))
        {
            whereisdata += (idStr[j] - '0') * mult;
            mult *= 10;
            --j;
        }


        // --------------------------------------------------------
        // Store the mapping between detector IDs and internal
        // channel numbers.
        // --------------------------------------------------------

        info.idToCh[id] = ch;
        info.chToId[ch] = id;

        info.whereisdata = whereisdata;


        // --------------------------------------------------------
        // Remember which detector(s) contain this detector ID.
        //
        // Avoid inserting duplicate detector numbers.
        // --------------------------------------------------------

        auto &vec = idToDetector[id];

        if (std::find(vec.begin(),
                      vec.end(),
                      det) == vec.end())
        {
            vec.push_back(det);
        }


        ++ch;
    }


    // ============================================================
    // Second pass through the file
    //
    // Rewind to the beginning and read only the
    // Next-Neighbour: records.
    //
    // This allows neighbour definitions to appear anywhere in the
    // file, even after the Element: block.
    // ============================================================

    rewind(file);

    skipped = 0;

    while (fgets(line, sizeof(line), file))
    {
        // Skip detector-specific header lines.

        if (skipped < info.offset)
        {
            ++skipped;
            continue;
        }


        // Ignore all non-neighbour lines.

        if (strncmp(line, "Next-Neighbour:", 15) != 0)
            continue;


        // --------------------------------------------------------
        // Expected format:
        //
        // Next-Neighbour:
        //      N element neighbour1 neighbour2 ...
        //
        // N includes the element itself.
        // Therefore the number of stored neighbours is N-1.
        // --------------------------------------------------------

        int number;
        int element;

        char* token = strtok(line, " \t");

        // Skip keyword.

        token = strtok(NULL, " \t");

        if (!token)
            continue;

        number = atoi(token);


        // Detector element number.

        token = strtok(NULL, " \t");

        if (!token)
            continue;

        element = atoi(token);


        // Verify that the detector element exists.

        if (element < 0 ||
            element >= (int)info.neighbours.size())
        {
            continue;
        }


        // Maximum storage is limited to 12 neighbours.

        int nNeighbours = number - 1;

        if (nNeighbours > 12)
            nNeighbours = 12;


        // --------------------------------------------------------
        // Read neighbour list.
        // --------------------------------------------------------

        for (int i = 0; i < nNeighbours; ++i)
        {
            token = strtok(NULL, " \t");

            if (!token)
                break;

            info.neighbours[element][i] = atoi(token);
        }
    }


    // ------------------------------------------------------------
    // Finished.
    // ------------------------------------------------------------

    fclose(file);
}

  // ============================================================
  // Fast neighbour lookup
  // ============================================================
  //
  // Returns the complete 12-element neighbour array.
  //
  // No copy is made. A const reference is returned.
  //
  // Example:
  //
  // const auto &n = getNeighbours(D_CB_ADC,42);
  //
  // n[0], n[1], ... contain the neighbours.
  //
  // Invalid detector/element requests return an array filled
  // with -1.
  //
  // ============================================================

  const std::array<int,12>& getNeighbours(int det, int element) const
  {

    static const std::array<int,12> empty =
    {
      -1,-1,-1,-1,
      -1,-1,-1,-1,
      -1,-1,-1,-1
    };


    auto dit = detectors.find(det);


    if(dit == detectors.end())
      return empty;



    const DetectorInfo &info = dit->second;



    if(element < 0 ||
       element >= (int)info.neighbours.size())
    {
      return empty;
    }



    return info.neighbours[element];

  }


  // ============================================================
  // Get one neighbour directly
  //
  // det          : detector type
  // element      : detector element
  // neighbourNo  : neighbour index 0..11
  //
  // Returns:
  //   neighbour element number
  //   -1 if invalid or no neighbour exists
  //
  // ============================================================

  int getNeighbour(int det, int element, int neighbourNo) const{

    auto dit = detectors.find(det);

    if(dit == detectors.end())
      return -1;

    const DetectorInfo &info = dit->second;

    if(element < 0 ||
       element >= (int)info.neighbours.size())
      return -1;


    if(neighbourNo < 0 ||
       neighbourNo >= 12)
      return -1;

    return info.neighbours[element][neighbourNo];
  }


  // ============================================================
  // SAFE LOOKUP FUNCTIONS (never crash)
  // ============================================================


  int getChannel(int det, int id) const
  {

    auto dit = detectors.find(det);

    if(dit == detectors.end())
      return -1;


    const auto& map = dit->second.idToCh;


    auto it = map.find(id);


    return (it != map.end()) ? it->second : -1;

  }




  int getMaxHits(int det) const
  {

    auto dit = detectors.find(det);

    if(dit == detectors.end())
      return -1;


    return dit->second.maxMultihit;

  }




  int getDataSource(int det) const
  {

    auto dit = detectors.find(det);

    if(dit == detectors.end())
      return -1;


    return dit->second.whereisdata;

  }




  int getNoCh(int det) const
  {

    auto dit = detectors.find(det);

    if(dit == detectors.end())
      return -1;


    return dit->second.detch;

  }




  int getBackId(int det, int ch) const
  {

    auto dit = detectors.find(det);

    if(dit == detectors.end())
      return -1;


    const auto& map = dit->second.chToId;


    auto it = map.find(ch);


    return (it != map.end()) ? it->second : -1;

  }




  int getRefId(int ch) const
  {

    auto it = tdcRefMap.find(ch);


    return (it != tdcRefMap.end()) ? it->second : -1;

  }




  // ============================================================
  // Reference data handling
  // ============================================================


  void store_ref_data(int id, unsigned int data)
  {

    for(size_t i=0;i<ref_id.size();++i)
    {

      if(ref_id[i] == id)
      {

        ref_data[i] = data;

        return;

      }

    }

  }




  unsigned int get_ref_data(int id) const
  {

    int refid = getRefId(id);


    if(refid == -1)
      return UINT_MAX;



    for(size_t i=0;i<ref_id.size();++i)
    {

      if(ref_id[i] == refid)
        return ref_data[i];

    }


    return UINT_MAX;

  }




  void reset_ref_data()
  {

    for(auto &v : ref_data)
      v = 0;

  }




  // ============================================================
  // Convert string to detector enum
  // ============================================================


  int detectorFromString(const char* word) const
  {

#define CMP(x) if(strcmp(word,#x)==0) return D_##x;


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


#undef CMP


    return D_NONE;

  }


};


#endif
