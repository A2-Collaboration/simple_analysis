#ifndef A2_READOUTCLASS_H
#define A2_READOUTCLASS_H

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <arpa/inet.h>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <unistd.h>
#include <termios.h>
#include "ModuleIndex.h"
#include "Rtypes.h"
#include "ConfigManager.h"
#include <time.h>

#define EHeadBuff      0x10101010
#define EDataBuff      0x70707070
#define EEndEvent      0xffffffff
#define EEndBuff       0x30303030
#define EScalerBuffer  0xfefefefe
#define EEPICSBuffer   0xfdfdfdfd
#define EReadError     0xefefefef

enum {
  EMk2SizeTime    = 32,
  EMk2SizeComment = 256,
  EMk2SizeFName   = 128,
  EMk2SizeDesc    = 256
};

enum ArrayName {
  TAGGER_TDC, TAGGER_SCALER,
  MWPC_W_TDC, MWPC_S_ADC,
  PID_ADC, PID_TDC,
  CB_ADC, CB_TDC,
  VETO_ADC, VETO_TDC,
  BAF2_S_ADC, BAF2_S_TDC,
  BAF2_L_ADC, BAF2_L_TDC,
  PBWO4_ADC, PBWO4_TDC,
  PBWO4_S_ADC, PBWO4_S_TDC,
  ARRAY_COUNT
};

static const char *SUBSYSTEM_NAMES[] = {
  "TAGGER_TDC", "TAGGER_SCALER",
  "MWPC_W_TDC", "MWPC_S_ADC", "PID_ADC", "PID_TDC",
  "CB_ADC", "CB_TDC",
  "VETO_ADC", "VETO_TDC",
  "BAF2_S_ADC", "BAF2_S_TDC",
  "BAF2_L_ADC", "BAF2_L_TDC",
  "PBWO4_ADC", "PBWO4_TDC",
  "PBWO4_S_ADC", "PBWO4_S_TDC"
};

enum EepicsType {
  EepicsBYTE, EepicsSTRING, EepicsSHORT,
  EepicsLONG, EepicsFLOAT, EepicsDOUBLE, EepicsNULL
};

const char *epicsTypeName[] = {
  "epicsBYTE", "epicsSTRING", "epicsSHORT",
  "epicsLONG", "epicsFLOAT", "epicsDOUBLE", nullptr
};

enum EepicsTypeSize {
  ESizeBYTE   = 1,
  ESizeSTRING = 40,
  ESizeSHORT  = 2,
  ESizeLONG   = 8,
  ESizeFLOAT  = 4,
  ESizeDOUBLE = 8
};

struct AcquMk2Info_t {
  UInt_t fMk1;
  UInt_t fMk2;
  Char_t fTime[EMk2SizeTime];
  Char_t fDescription[EMk2SizeDesc];
  Char_t fRunNote[EMk2SizeComment];
  Char_t fOutFile[EMk2SizeFName];
  Int_t fRun;
  Int_t fNModule;
  Int_t fNADCModule;
  Int_t fNScalerModule;
  Int_t fNADC;
  Int_t fNScaler;
  Int_t fRecLen;
};

struct ModuleHead {
  int32_t fID;
  int32_t fIndex;
  int32_t fModuleType;
  int32_t fMinChannel;
  int32_t fNChannel;
  int32_t fNScalerChannels;
  int32_t fNBits;
};

struct EventHead {
  uint32_t evNo;
  uint32_t evLen;
  uint16_t adcInd;
  uint16_t adcCnt;
};

struct EpicsHead {
  char epics[32];
  uint32_t time;
  uint16_t index;
  uint16_t period;
  uint16_t id;
  uint16_t nchan;
  uint16_t len;
};

struct EpicsChan {
  char pvname[32];
  uint16_t bytes;
  uint16_t nelem;
  uint16_t type;
};

int getch(void);

class Array2D {
  std::vector<long int> data;
  size_t hits{};
  size_t channels{};
  bool valid{false};
  size_t own_name=0;
  std::vector<size_t> next_hit;

  size_t index(size_t hidx, size_t ch) const { return ch * hits + hidx; }

public:
  Array2D() = default;
  explicit Array2D(size_t name, size_t c, size_t h) { init(name, c, h); }

  void init(size_t name, size_t c, size_t h) {
    channels = c;
    hits     = h;
    own_name = name;
    data.assign(c * h, 0u);
    next_hit.assign(c, 0);
    valid = true;
    printf("Array2D_init: %s, ch: %ld, max_hits: %ld\n", SUBSYSTEM_NAMES[name], c, h);
  }

  void clear() {
    std::fill(data.begin(), data.end(), 0u);
    std::fill(next_hit.begin(), next_hit.end(), 0);
  }

  long int get(size_t ch, size_t hidx) const {
    if (!valid || ch >= channels || hidx >= hits) {
      fprintf(stderr, "Warning %s: Accessing invalid element ch=%zu h=%zu\n", SUBSYSTEM_NAMES[own_name], ch, hidx);
      return 0;
    }
    return data[index(hidx, ch)];
  }

  const long int& operator()(size_t ch, size_t hidx) const {
    if (!valid || ch >= channels || hidx >= hits)
      throw std::out_of_range("Array2D access (const)");
    return data[index(hidx, ch)];
  }

  long int& operator()(size_t ch, size_t hidx) {
    if (!valid || ch >= channels || hidx >= hits)
      throw std::out_of_range("Array2D access");
    return data[index(hidx, ch)];
  }

  void set(size_t ch, long int val) {
    if (!valid || ch >= channels) {
      fprintf(stderr, "Warning %s: Trying to write to invalid channel %zu\n", SUBSYSTEM_NAMES[own_name], ch);
      return;
    }
    size_t hidx = next_hit[ch];
    if (hidx >= hits) {
      fprintf(stderr, "Warning %s: Channel %zu overflow (capacity %zu hits)\n", SUBSYSTEM_NAMES[own_name], ch, hits);
      exit(0);
      return;
    }
    data[index(hidx, ch)] = val;
    next_hit[ch]++;
  }

  void set_at(size_t ch, size_t hidx, long int val) {
    if (!valid || ch >= channels || hidx >= hits) {
      fprintf(stderr, "Warning %s: Setting invalid element ch=%zu h=%zu\n", SUBSYSTEM_NAMES[own_name], ch, hidx);
      return;
    }
    data[index(hidx, ch)] = val;
  }

  size_t get_hits_max() const { return valid ? hits : 0; }
  size_t get_channels() const { return valid ? channels : 0; }
  size_t get_hits(size_t ch) const {
    if (!valid || ch >= channels) return 0;
    return next_hit[ch];
  }
  bool is_valid() const { return valid; }
};

inline void fread_or_die(void *dest, size_t size, size_t count, FILE *fp, const std::string &context) {
  size_t got = std::fread(dest, size, count, fp);
  if (got != count) {
    if (std::feof(fp))
      printf("Incomplete event: End of file in %s\n", context.c_str());
    else
      throw std::runtime_error(context + ": I/O error (" + std::strerror(errno) + ")");
  }
}

class Read_A2_class {
protected:
  char filename[100];
  char configfilename[100];
  FILE *in = nullptr;
  FILE *conf_in = nullptr;
  unsigned long no_of_int_in_file;
  unsigned long zero_offset;
  int data[64];
  ConfigManager cfg;
  unsigned long long events = 0;
  int wie_oft = 20000000;
  AcquMk2Info_t headerinfo;
  ModuleHead *modulesinfo = nullptr;
  EventHead eventheaderinfo;
  EpicsHead epicsheaderinfo;
  EpicsChan epicschan;
  int verboselvl = 0;
  time_t start_t, end_t; 

public:
  Read_A2_class() = default;
  ~Read_A2_class() {
    if (in != nullptr) fclose(in);
    if (conf_in != nullptr) fclose(conf_in);
    if (modulesinfo) free(modulesinfo);
  }

  int init(const char* _file, const char* configfile, int verboselvl_ = 20);
  int read_one_event(void);
  double get_value(int channel);

  Array2D& tagger_tdc()      { return data_arrays[TAGGER_TDC]; }
  Array2D& tagger_scaler()   { return data_arrays[TAGGER_SCALER]; }
  Array2D& mwpc_w_tdc()      { return data_arrays[MWPC_W_TDC]; }
  Array2D& mwpc_s_adc()      { return data_arrays[MWPC_S_ADC]; }
  Array2D& pid_adc()         { return data_arrays[PID_ADC]; }
  Array2D& pid_tdc()         { return data_arrays[PID_TDC]; }
  Array2D& cb_adc()          { return data_arrays[CB_ADC]; }
  Array2D& cb_tdc()          { return data_arrays[CB_TDC]; }
  Array2D& veto_adc()        { return data_arrays[VETO_ADC]; }
  Array2D& veto_tdc()        { return data_arrays[VETO_TDC]; }
  Array2D& baf2_s_adc()      { return data_arrays[BAF2_S_ADC]; }
  Array2D& baf2_s_tdc()      { return data_arrays[BAF2_S_TDC]; }
  Array2D& baf2_l_adc()      { return data_arrays[BAF2_L_ADC]; }
  Array2D& baf2_l_tdc()      { return data_arrays[BAF2_L_TDC]; }
  Array2D& pbwo4_adc()       { return data_arrays[PBWO4_ADC]; }
  Array2D& pbwo4_tdc()       { return data_arrays[PBWO4_TDC]; }
  Array2D& pbwo4_s_adc()     { return data_arrays[PBWO4_S_ADC]; }
  Array2D& pbwo4_s_tdc()     { return data_arrays[PBWO4_S_TDC]; }

  const Array2D& tagger_tdc() const      { return data_arrays[TAGGER_TDC]; }
  const Array2D& tagger_scaler() const   { return data_arrays[TAGGER_SCALER]; }
  const Array2D& mwpc_w_tdc() const      { return data_arrays[MWPC_W_TDC]; }
  const Array2D& mwpc_s_adc() const      { return data_arrays[MWPC_S_ADC]; }
  const Array2D& pid_adc() const         { return data_arrays[PID_ADC]; }
  const Array2D& pid_tdc() const         { return data_arrays[PID_TDC]; }
  const Array2D& cb_adc() const          { return data_arrays[CB_ADC]; }
  const Array2D& cb_tdc() const          { return data_arrays[CB_TDC]; }
  const Array2D& veto_adc() const        { return data_arrays[VETO_ADC]; }
  const Array2D& veto_tdc() const        { return data_arrays[VETO_TDC]; }
  const Array2D& baf2_s_adc() const      { return data_arrays[BAF2_S_ADC]; }
  const Array2D& baf2_s_tdc() const      { return data_arrays[BAF2_S_TDC]; }
  const Array2D& baf2_l_adc() const      { return data_arrays[BAF2_L_ADC]; }
  const Array2D& baf2_l_tdc() const      { return data_arrays[BAF2_L_TDC]; }
  const Array2D& pbwo4_adc() const       { return data_arrays[PBWO4_ADC]; }
  const Array2D& pbwo4_tdc() const       { return data_arrays[PBWO4_TDC]; }
  const Array2D& pbwo4_s_adc() const     { return data_arrays[PBWO4_S_ADC]; }
  const Array2D& pbwo4_s_tdc() const     { return data_arrays[PBWO4_S_TDC]; }

  bool is_active(int name) const {
    return data_arrays[name].is_valid();
  }

  long int get(ArrayName name, size_t ch, size_t hidx) const {
    if (static_cast<int>(name) < 0 || name >= ARRAY_COUNT) {
      fprintf(stderr, "Read_A2_class::get – invalid ArrayName %d\n", static_cast<int>(name));
      return 0;
    }
    return data_arrays[name].get(ch, hidx);
  }

  int get_hits(ArrayName name, size_t ch) const {
    if (static_cast<int>(name) < 0 || name >= ARRAY_COUNT) {
      fprintf(stderr, "Read_A2_class::get_hits – invalid ArrayName %d\n", static_cast<int>(name));
      return 0;
    }
    return data_arrays[name].get_hits(ch);
  }

  int get_channels(ArrayName name) const {
    if (static_cast<int>(name) < 0 || name >= ARRAY_COUNT) {
      fprintf(stderr, "Read_A2_class::get_channels – invalid ArrayName %d\n", static_cast<int>(name));
      return 0;
    }
    return data_arrays[name].get_channels();
  }

  const Array2D& array(ArrayName name) const {
    if (static_cast<int>(name) < 0 || name >= ARRAY_COUNT) {
      static Array2D dummy;
      return dummy;
    }
    return data_arrays[name];
  }

  Array2D& array(ArrayName name) {
    if (static_cast<int>(name) < 0 || name >= ARRAY_COUNT) {
      static Array2D dummy;
      return dummy;
    }
    return data_arrays[name];
  }

private:
  Array2D data_arrays[ARRAY_COUNT]{};
  int read_one_dataword(unsigned int &dataword);
  void undo_read_one_dataword(void);
  int read_header(void);
  int read_event_header(void);
  void read_module_definitions(void);
  void decode_epics(void);
  void decode_scaler(void);
  void decode_adc(unsigned int dataword);
};

int Read_A2_class::init(const char* _file, const char* configfile, int verboselvl_) {
  verboselvl = verboselvl_;
  strncpy(filename, _file, sizeof(filename) - 1);
  filename[sizeof(filename) - 1] = '\0';
  if (!cfg.readConfig(configfile, verboselvl)) {
    printf("Configuration failed\n");
    return EXIT_FAILURE;
  }
  if (verboselvl >= 10) cfg.printSummary();
  time(&start_t);

  data_arrays[TAGGER_TDC]   .init(TAGGER_TDC,    cfg.getnoCh(D_TAGGER), cfg.getTDCMaxHits(D_TAGGER));
  data_arrays[TAGGER_SCALER].init(TAGGER_SCALER, cfg.getnoCh(D_TAGGER), cfg.getSCALERMaxHits(D_TAGGER));
  data_arrays[MWPC_W_TDC]   .init(MWPC_W_TDC,    cfg.getnoCh(D_MWPC_W), cfg.getTDCMaxHits(D_MWPC_W));
  data_arrays[MWPC_S_ADC]   .init(MWPC_S_ADC,    cfg.getnoCh(D_MWPC_S), cfg.getADCMaxHits(D_MWPC_S));
  data_arrays[PID_ADC]      .init(PID_ADC,       cfg.getnoCh(D_PID),     cfg.getADCMaxHits(D_PID));
  data_arrays[PID_TDC]      .init(PID_TDC,       cfg.getnoCh(D_PID),     cfg.getTDCMaxHits(D_PID));
  data_arrays[CB_ADC]       .init(CB_ADC,        cfg.getnoCh(D_CB),      cfg.getADCMaxHits(D_CB));
  data_arrays[CB_TDC]       .init(CB_TDC,        cfg.getnoCh(D_CB),      cfg.getTDCMaxHits(D_CB));
  data_arrays[VETO_ADC]     .init(VETO_ADC,      cfg.getnoCh(D_VETO),    cfg.getADCMaxHits(D_VETO));
  data_arrays[VETO_TDC]     .init(VETO_TDC,      cfg.getnoCh(D_VETO),    cfg.getTDCMaxHits(D_VETO));
  data_arrays[BAF2_S_ADC]   .init(BAF2_S_ADC,    cfg.getnoCh(D_BAF2_S),  cfg.getADCMaxHits(D_BAF2_S));
  data_arrays[BAF2_S_TDC]   .init(BAF2_S_TDC,    cfg.getnoCh(D_BAF2_S),  cfg.getTDCMaxHits(D_BAF2_S));
  data_arrays[BAF2_L_ADC]   .init(BAF2_L_ADC,    cfg.getnoCh(D_BAF2_L),  cfg.getADCMaxHits(D_BAF2_L));
  data_arrays[BAF2_L_TDC]   .init(BAF2_L_TDC,    cfg.getnoCh(D_BAF2_L),  cfg.getTDCMaxHits(D_BAF2_L));
  data_arrays[PBWO4_ADC]    .init(PBWO4_ADC,     cfg.getnoCh(D_PBWO4),   cfg.getADCMaxHits(D_PBWO4));
  data_arrays[PBWO4_TDC]    .init(PBWO4_TDC,     cfg.getnoCh(D_PBWO4),   cfg.getTDCMaxHits(D_PBWO4));
  data_arrays[PBWO4_S_ADC]  .init(PBWO4_S_ADC,   cfg.getnoCh(D_PBWO4_S), cfg.getADCMaxHits(D_PBWO4_S));
  data_arrays[PBWO4_S_TDC]  .init(PBWO4_S_TDC,   cfg.getnoCh(D_PBWO4_S), cfg.getTDCMaxHits(D_PBWO4_S));
  in = fopen(filename, "rb");
  if (in == nullptr) {
    printf("%s doesn't exist!!!\n", filename);
    exit(-2);
  }
  fseek(in, 0, SEEK_END);
  no_of_int_in_file = ftell(in) / 4;
  fseek(in, 0, SEEK_SET);
  return read_header();
}

int Read_A2_class::read_header(void) {
  fread_or_die(&headerinfo, sizeof(headerinfo), 1, in, "Read_A2_class::read_header – run header");
  if (headerinfo.fMk1 == headerinfo.fMk2) {
    if (verboselvl >= 10)
      printf("MK2 data format!: 0x%x\n", headerinfo.fMk2);
  } else {
    printf("MK1 data format not supported\n");
    exit(-2);
  }
  if (verboselvl >= 10) {
    printf("MK1: 0x%x\n", headerinfo.fMk1);
    printf("MK2: 0x%x\n", headerinfo.fMk2);
    printf("Time: %s", headerinfo.fTime);
    printf("Descr: %s", headerinfo.fDescription);
    printf("Note: %s", headerinfo.fRunNote);
    printf("Filename: %s\n", headerinfo.fOutFile);
    printf("Run no: %i\n", headerinfo.fRun);
    printf("No. modules: %i\n", headerinfo.fNModule);
    printf("No ADC mod: %i\n", headerinfo.fNADCModule);
    printf("No SCALER mod: %i\n", headerinfo.fNScalerModule);
    printf("ADCs: %i\n", headerinfo.fNADC);
    printf("Scaler: %i\n", headerinfo.fNScaler);
    printf("RecLen: %i\n", headerinfo.fRecLen);
  }
  read_module_definitions();
  return 0;
}

void Read_A2_class::read_module_definitions(void) {
  unsigned int dataword;
  modulesinfo = (ModuleHead*) malloc(sizeof(ModuleHead) * headerinfo.fNModule);
  fread_or_die(modulesinfo, sizeof(ModuleHead), headerinfo.fNModule, in, "Read_A2_class::read_module_definitions – module list");
  if (verboselvl >= 10) {
    printf("\nModule definitions:\n");
    for (int i = 0; i < headerinfo.fNModule; ++i) {
      if (i != 0 && modulesinfo[i].fIndex == 0) printf("\n");
      printf("%2i Module %2u %5u  %10s  Channels %3d\n",
             i,
             modulesinfo[i].fIndex,
             modulesinfo[i].fID,
             getModuleNamekExpModules(modulesinfo[i].fID),
             modulesinfo[i].fNChannel);
    }
  }
  int zerocounter = -1;
  int old_verboselvl = verboselvl;
  verboselvl = 0;
  do{
    if(read_one_dataword(dataword)==0){
      printf("EoF in zero suppression\n");
      break;
    }
    ++zerocounter;
  }while(dataword == 0);
  zero_offset=ftell(in)/4;
  verboselvl = old_verboselvl;
  if (verboselvl >= 20)
    printf("%i zeroes omitted (%f module headers)\n",
           zerocounter,
           static_cast<double>(zerocounter) / sizeof(ModuleHead));
}

int Read_A2_class::read_one_event(void) {
  unsigned int dataword;
  static int no_reads = 0;
  static int noe = 0;
  int rv;
  if (verboselvl >= 20)
    printf("\n************* Start reading one event:  *******************\n");
  for (size_t i = 0; i < ARRAY_COUNT; ++i)
    data_arrays[i].clear();
  cfg.reset_ref_data();       // clear reference data
  
  read_event_header();
  int datablock_count = 0;
  do {
    rv = read_one_dataword(dataword);
    if(rv==0){
      printf("EoF read one event\n");
      return 0;
    }
    switch (dataword) {
      case EScalerBuffer:
        if (verboselvl >= 20) printf("Scaler buffer\n");
        decode_scaler();
        break;
      case EEPICSBuffer:
        if (verboselvl >= 20) printf("Epics buffer\n");
        decode_epics();
        break;
      case EEndEvent:
        if (verboselvl >= 20) printf("EndEvent\n");
        break;
      case EEndBuff:
        if (verboselvl >= 20) printf("EndBuff\n");
        return 0;
      case EReadError:
        if (verboselvl >= 10) printf("Read Error\n");
        break;
      default:
        if (verboselvl >= 20) printf("normal data  ");
        ++datablock_count;
        decode_adc(dataword);
        break;
    }
    no_reads += 4;
  } while (dataword != EEndEvent);

  // substract TDC reference
 // TAGGER_TDC,  MWPC_W_TDC, PID_TDC,
// CB_TDC, PBWO4_TDC, PBWO4_S_TDC,
/*
  data_arrays[TAGGER_TDC]   .init(TAGGER_TDC,    cfg.getnoCh(D_TAGGER), cfg.getTDCMaxHits(D_TAGGER));
  data_arrays[TAGGER_SCALER].init(TAGGER_SCALER, cfg.getnoCh(D_TAGGER), cfg.getSCALERMaxHits(D_TAGGER));
  data_arrays[MWPC_W_TDC]   .init(MWPC_W_TDC,    cfg.getnoCh(D_MWPC_W), cfg.getTDCMaxHits(D_MWPC_W));
  data_arrays[MWPC_S_ADC]   .init(MWPC_S_ADC,    cfg.getnoCh(D_MWPC_S), cfg.getADCMaxHits(D_MWPC_S));
  data_arrays[PID_ADC]      .init(PID_ADC,       cfg.getnoCh(D_PID),     cfg.getADCMaxHits(D_PID));
  data_arrays[PID_TDC]      .init(PID_TDC,       cfg.getnoCh(D_PID),     cfg.getTDCMaxHits(D_PID));
  data_arrays[CB_ADC]       .init(CB_ADC,        cfg.getnoCh(D_CB),      cfg.getADCMaxHits(D_CB));
  data_arrays[CB_TDC]       .init(CB_TDC,        cfg.getnoCh(D_CB),      cfg.getTDCMaxHits(D_CB));
  data_arrays[VETO_ADC]     .init(VETO_ADC,      cfg.getnoCh(D_VETO),    cfg.getADCMaxHits(D_VETO));
  data_arrays[VETO_TDC]     .init(VETO_TDC,      cfg.getnoCh(D_VETO),    cfg.getTDCMaxHits(D_VETO));
  data_arrays[BAF2_S_ADC]   .init(BAF2_S_ADC,    cfg.getnoCh(D_BAF2_S),  cfg.getADCMaxHits(D_BAF2_S));
  data_arrays[BAF2_S_TDC]   .init(BAF2_S_TDC,    cfg.getnoCh(D_BAF2_S),  cfg.getTDCMaxHits(D_BAF2_S));
  data_arrays[BAF2_L_ADC]   .init(BAF2_L_ADC,    cfg.getnoCh(D_BAF2_L),  cfg.getADCMaxHits(D_BAF2_L));
  data_arrays[BAF2_L_TDC]   .init(BAF2_L_TDC,    cfg.getnoCh(D_BAF2_L),  cfg.getTDCMaxHits(D_BAF2_L));
  data_arrays[PBWO4_ADC]    .init(PBWO4_ADC,     cfg.getnoCh(D_PBWO4),   cfg.getADCMaxHits(D_PBWO4));
  data_arrays[PBWO4_TDC]    .init(PBWO4_TDC,     cfg.getnoCh(D_PBWO4),   cfg.getTDCMaxHits(D_PBWO4));
  data_arrays[PBWO4_S_ADC]  .init(PBWO4_S_ADC,   cfg.getnoCh(D_PBWO4_S), cfg.getADCMaxHits(D_PBWO4_S));
  data_arrays[PBWO4_S_TDC]  .init(PBWO4_S_TDC,   cfg.getnoCh(D_PBWO4_S), cfg.getTDCMaxHits(D_PBWO4_S));
*/


  int id;
  int ref_data, data, diff;
// Tagger TDC

  for(int ch=0; ch<get_channels(TAGGER_TDC); ch++){
    id=cfg.get(D_TAGGER).getTDC_id(ch);
	ref_data=cfg.get_ref_data(id);
    if(verboselvl>=20) printf("Ch: %d, ID: %d ref_data: %d\n", ch, id, ref_data);
    for(size_t h=0; h<data_arrays[TAGGER_TDC].get_hits(ch); h++){
      data=data_arrays[TAGGER_TDC].get(ch, h);
      diff=data-ref_data;
      data_arrays[TAGGER_TDC].set_at(ch, h, diff);
    }
  }
//  exit(0);
  // end substract TDC reference
  
  if (verboselvl >= 10) printf("\n %i data blocks in event\n", datablock_count);
  if (verboselvl >= 20) printf("\n");
  ++noe;
  return rv;
}

int Read_A2_class::read_event_header(void) {
  fread_or_die(&eventheaderinfo, sizeof(eventheaderinfo), 1, in, "Read_A2_class::read_event_header");
  events += sizeof(eventheaderinfo) / 4;
  if (verboselvl >= 10) {
    printf("  Event: %u, len: %u, adcInd: %i, adcCnt: %i\n\n",
           eventheaderinfo.evNo,
           eventheaderinfo.evLen,
           eventheaderinfo.adcInd,
           eventheaderinfo.adcCnt);
  }
  return 0;
}

int Read_A2_class::read_one_dataword(unsigned int &dataword) {
  int rv = fread(&dataword, sizeof(dataword), 1, in);
  events++;
  if (rv == 0) return 0;
  if (verboselvl >= 20) printf("ROD: 0x%08x:  ", dataword);
  if (events % wie_oft == 0){
    double percent=static_cast<double>(events) / (no_of_int_in_file-zero_offset) * 100.0;
    time(&end_t);

    unsigned long diff_t = (unsigned long) difftime(end_t, start_t);    
    int h= diff_t/60/60;
    int m= (diff_t-h*60)/60;
    int s= (diff_t-h*60*60-m*60);

    unsigned long diff_t2 = diff_t/(percent/100);
    int h2= diff_t2/60/60;
    int m2= (diff_t2-h2*60)/60;
    int s2= (diff_t2-h2*60*60-m2*60);
  
    printf("%3.1f%% read in %02d:%02d:%02d  (estimated time: %02d:%02d:%02d)\n", percent, h, m, s, h2, m2, s2);
    //if(percent>97.0) verboselvl=20;
  }
  return rv;
}

void Read_A2_class::undo_read_one_dataword(void) {
  unsigned int dummy;
  fseek(in, -static_cast<long>(sizeof(dummy)), SEEK_CUR);
}

void Read_A2_class::decode_scaler(void) {
  unsigned int index, value;
  int ch;
  if (verboselvl >= 10) printf("Scaler block detected\n");
  for (int i = 0; i < 2000; ++i) {
    if(read_one_dataword(value)==0){
      printf("EoF in scaler event\n");
      break;
    }
    if(read_one_dataword(index)==0){
      printf("EoF in scaler event\n");
      break;
    }
    if(index == 0xfefefefe) {
      if (verboselvl >= 20) printf("****** End of tagger block \n");
      break;
    }
    if (verboselvl >= 20) printf("Scaler %4u  value %10u", index, value);
    ch = cfg.get(D_TAGGER).getScaler_ch(index);
    if (ch >= 0) {
      if (verboselvl >= 20)
        printf("   Tagger Scaler hit: %4i (ch %3i) V %10u\n", index, ch, value);
    } else if (verboselvl >= 20) {
      printf("   Unknown id: %4i, value: %10u\n", index, value);
    }
  }
}

void Read_A2_class::decode_epics(void) {
  if (verboselvl >= 10) printf("  EPICS block detected\n");
  fread_or_die(&epicsheaderinfo, sizeof(epicsheaderinfo), 1, in, "Read_A2_class::decode_epics – EPICS block header");
  events += sizeof(epicsheaderinfo) / 4;
  if (verboselvl >= 10) printf("  Channels: %u\n", epicsheaderinfo.nchan);
  char pvname[64];
  uint16_t bytes, nelem, type;
  char varB;
  char varStr[ESizeSTRING];
  short varS;
  long varL;
  float varF;
  double varD;
  uint16_t scrap[20];
  for (int i = 0; i < epicsheaderinfo.nchan; ++i) {
    fread_or_die(pvname, 1, 32, in, "Read_A2_class::decode_epics – PV name");
    fread_or_die(&bytes,   2, 1, in, "Read_A2_class::decode_epics – bytes field");
    fread_or_die(&nelem,   2, 1, in, "Read_A2_class::decode_epics – nelem field");
    fread_or_die(&type,    2, 1, in, "Read_A2_class::decode_epics – type field");
    events += 32 / 4;
    if (verboselvl >= 20)
      printf("  %i, PV: %s, bytes %u, elements=%u, type=%u, var: ",
             i, pvname, bytes, nelem, type);
    switch (type) {
      case EepicsBYTE:
        fread_or_die(&varB, ESizeBYTE, 1, in, "Read_A2_class::decode_epics – BYTE payload");
        if (verboselvl >= 20) printf("%i\n", static_cast<int>(varB));
        break;
      case EepicsSTRING:
        fread_or_die(varStr, ESizeSTRING, 1, in, "Read_A2_class::decode_epics – STRING payload");
        if (verboselvl >= 20) printf("%s\n", varStr);
        break;
      case EepicsSHORT:
        fread_or_die(&varS, ESizeSHORT, 1, in, "Read_A2_class::decode_epics – SHORT payload");
        if (verboselvl >= 20) printf("%i\n", static_cast<int>(varS));
        break;
      case EepicsLONG:
        fread_or_die(&varL, ESizeLONG, 1, in, "Read_A2_class::decode_epics – LONG payload");
        if (verboselvl >= 20) printf("%ld\n", varL);
        break;
      case EepicsFLOAT:
        fread_or_die(&varF, ESizeFLOAT, 1, in, "Read_A2_class::decode_epics – FLOAT payload");
        if (verboselvl >= 20) printf("%f\n", varF);
        break;
      case EepicsDOUBLE:
        fread_or_die(&varD, ESizeDOUBLE, 1, in, "Read_A2_class::decode_epics – DOUBLE payload");
        if (verboselvl >= 20) printf("%f\n", varD);
        break;
      default:
        if (verboselvl >= 20)
          printf("\nWARNING: Unknown epics data type: %u for EPICS channel %s\n",
                 type, pvname);
    }
  }
  if (epicsheaderinfo.nchan % 2 == 1) {
    if (verboselvl >= 20) printf("  odd prevention\n");
    fread_or_die(scrap, 2, 1, in, "Read_A2_class::decode_epics – alignment padding");
  }
}

void Read_A2_class::decode_adc(unsigned int dataword) {
  long int id, value;
  int ch;
  id    = dataword & 0xffff;
  value = (dataword >> 16) & 0xffff;

  
  if (verboselvl>= 20) printf("Data: id: %5i, value %5i \t", id, value);

  // test if id is an reference tdc channel and store it if it matches
  cfg.store_ref_data(id, value);

  ch = cfg.get(D_TAGGER).getTDC_ch(id);
  if (ch >= 0) {
    if (verboselvl>= 20) printf("   TAGGER TDC hit: %4i (ch %3i) V %5i", id, ch, value);
    tagger_tdc().set(ch, value);
  }
  
  ch = cfg.get(D_MWPC_W).getTDC_ch(id);
  if(ch >= 0){
    if(verboselvl>= 20) printf("   MWPC TDC hit: %4i (ch %3i) V %5i", id, ch, value);
    mwpc_w_tdc().set(ch, value);
  }
  ch = cfg.get(D_MWPC_S).getADC_ch(id);
  if(ch >=0){
    if(verboselvl>= 20) printf("   MWPC ADC hit: %4i (ch %3i) V %5i", id, ch, value);
    mwpc_s_adc().set(ch, value);
  }

  ch = cfg.get(D_PID).getADC_ch(id);
  if(ch >= 0){
    if(verboselvl>= 20) printf("   PID ADC hit: %4i (ch %3i) V %5i", id, ch, value);
    pid_adc().set(ch, value);
  }
  ch = cfg.get(D_PID).getTDC_ch(id);
  if(ch >= 0){
    if(verboselvl>=20) printf("   PID TDC hit: %4i (ch %3i) V %5i", id, ch, value);
    pid_tdc().set(ch, value);
  }
  ch = cfg.get(D_CB).getADC_ch(id);
  if(ch >= 0){
    if(verboselvl>=20) printf("   CB ADC hit: %4i (ch %3i) V %5i", id, ch, value);
    cb_adc().set(ch, value);
  }
  ch = cfg.get(D_CB).getTDC_ch(id);
  if(ch >= 0){
    if(verboselvl>=20) printf("   CB TDC hit: %4i (ch %3i) V %5i", id, ch, value);
    cb_tdc().set(ch, value);
  }

  ch = cfg.get(D_VETO).getADC_ch(id);
  if(ch >= 0){
    if(verboselvl>=20) printf("   VETO ADC hit: %4i (ch %3i) V %5i", id, ch, value);
    veto_adc().set(ch, value);
  }
  ch = cfg.get(D_VETO).getTDC_ch(id);
  if(ch >= 0){
    if(verboselvl>=20) printf("   VETO TDC hit: %4i (ch %3i) V %5i", id, ch, value);
    veto_tdc().set(ch, value);
  }
  
  ch = cfg.get(D_PBWO4).getADC_ch(id);
  if(ch >= 0){
    if(verboselvl>=20) printf("   PWO ADC hit: %4i (ch %3i) V %5i", id, ch, value);
    pbwo4_adc().set(ch, value);
  }
  ch = cfg.get(D_PBWO4).getTDC_ch(id);
  if(ch >= 0){
    if(verboselvl>=20) printf("   PWO TDC hit: %4i (ch %3i) V %5i", id, ch, value);
    pbwo4_tdc().set(ch, value);
  }
  ch = cfg.get(D_PBWO4_S).getADC_ch(id);
  if(ch >= 0){
    if(verboselvl>=20) printf("   PWO sens ADC hit: %4i (ch %3i) V %5i", id, ch, value);
    pbwo4_s_adc().set(ch, value);
  }
  ch = cfg.get(D_PBWO4_S).getTDC_ch(id);
  if(ch >= 0){
    if(verboselvl>=20) printf("   PWO sens TDC hit: %4i (ch %3i) V %5i", id, ch, value);
    pbwo4_s_tdc().set(ch, value);
  }
    
  ch = cfg.get(D_BAF2_S).getADC_ch(id);
  if(ch >= 0){
    if(verboselvl>=20) printf("   BaF2 short ADC hit: %4i (ch %3i) V %5i", id, ch, value);
    baf2_s_adc().set(ch, value);
  }
  ch = cfg.get(D_BAF2_L).getADC_ch(id);
  if(ch >= 0){
    if(verboselvl>=20) printf("   BaF2 long ADC hit: %4i (ch %3i) V %5i", id, ch, value);
    baf2_l_adc().set(ch, value);
  }
  ch = cfg.get(D_BAF2_L).getTDC_ch(id);
  if(ch >= 0){
    if(verboselvl>=20) printf("   BaF2 long TDC hit: %4i (ch %3i) V %5i", id, ch, value);
    baf2_l_tdc().set(ch, value);
  }
  ch = cfg.get(D_BAF2_S).getTDC_ch(id);
  if(ch >= 0){
    if(verboselvl>=20) printf("   BaF2 short TDC hit: %4i (ch %3i) V %5i", id, ch, value);
    baf2_s_tdc().set(ch, value);
  }
  
  if(id == 400 && verboselvl >= 20)
    printf("   ESip hit: %4i V %5i", id, value);

  if(verboselvl >= 20) printf("\n");
}

int getch(void) {
  int ch;
  struct termios tc_attrib;
  if (tcgetattr(STDIN_FILENO, &tc_attrib))
    return -1;
  tcflag_t lflag = tc_attrib.c_lflag;
  tc_attrib.c_lflag &= ~ICANON & ~ECHO;
  if (tcsetattr(STDIN_FILENO, TCSANOW, &tc_attrib))
    return -1;
  ch = getchar();
  tc_attrib.c_lflag = lflag;
  tcsetattr(STDIN_FILENO, TCSANOW, &tc_attrib);
  return ch;
}

double randit(int ini) {
  if (ini == 1) srand(time(NULL));
  return (((rand() % 100) - 50.) / 100.);
}

#endif // A2_READOUTCLASS_H
