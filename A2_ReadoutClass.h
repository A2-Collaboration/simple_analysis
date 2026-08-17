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
  std::vector<uint32_t> data;
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
    printf("Array2D_init: %s, ch: %ld, max_hits: %ld\n", getDetectorName(name), c, h);
  }

  void clear() {
    std::fill(data.begin(), data.end(), 0u);
    std::fill(next_hit.begin(), next_hit.end(), 0);
  }

  uint32_t get(size_t ch, size_t hidx) const {
    if (!valid || ch >= channels || hidx >= hits) {
      fprintf(stderr, "Warning %s: Accessing invalid element ch=%zu h=%zu\n", getDetectorName(own_name), ch, hidx);
      return 0;
    }
    return data[index(hidx, ch)];
  }

  const uint32_t & operator()(size_t ch, size_t hidx) const {
    if (!valid || ch >= channels || hidx >= hits)
      throw std::out_of_range("Array2D access (const)");
    return data[index(hidx, ch)];
  }

  uint32_t & operator()(size_t ch, size_t hidx) {
    if (!valid || ch >= channels || hidx >= hits)
      throw std::out_of_range("Array2D access");
    return data[index(hidx, ch)];
  }

  void set(size_t ch, long int val) {
    if (!valid || ch >= channels) {
      fprintf(stderr, "Warning %s: Trying to write to invalid channel %zu\n", getDetectorName(own_name), ch);
      return;
    }
    size_t hidx = next_hit[ch];
    if (hidx >= hits) {
      fprintf(stderr, "Warning %s: Channel %zu overflow (capacity %zu hits)\n", getDetectorName(own_name), ch, hits);
      return;
    }
    data[index(hidx, ch)] = val;
    next_hit[ch]++;
  }

  void set_at(size_t ch, size_t hidx, long int val) {
    if (!valid || ch >= channels || hidx >= hits) {
      fprintf(stderr, "Warning %s: Setting invalid element ch=%zu h=%zu\n", getDetectorName(own_name), ch, hidx);
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
  double clock_scaler=0;
  double inhibit_scaler=0;

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
  void set_verboselevel(int verboselvl_ = 20){verboselvl = verboselvl_;}
  double get_clock_scaler(void){ return clock_scaler;}
  double get_inhibit_scaler(void){ return inhibit_scaler;}
  
  Array2D& tagger_tdc()      { return data_arrays[D_TAGGER_TDC]; }
  Array2D& tagger_scaler()   { return data_arrays[D_TAGGER_SCALER]; }
  Array2D& mwpc_w_tdc()      { return data_arrays[D_MWPC_W_TDC]; }
  Array2D& mwpc_s_adc()      { return data_arrays[D_MWPC_S_ADC]; }
  Array2D& pid_adc()         { return data_arrays[D_PID_ADC]; }
  Array2D& pid_tdc()         { return data_arrays[D_PID_TDC]; }
  Array2D& cb_adc()          { return data_arrays[D_CB_ADC]; }
  Array2D& cb_tdc()          { return data_arrays[D_CB_TDC]; }
  Array2D& veto_adc()        { return data_arrays[D_VETO_ADC]; }
  Array2D& veto_tdc()        { return data_arrays[D_VETO_TDC]; }
  
  Array2D& baf2_s_n_adc()    { return data_arrays[D_BAF2_S_N_ADC]; }
  Array2D& baf2_s_s_adc()    { return data_arrays[D_BAF2_S_S_ADC]; }
  Array2D& baf2_l_n_adc()    { return data_arrays[D_BAF2_L_N_ADC]; }
  Array2D& baf2_l_s_adc()    { return data_arrays[D_BAF2_L_N_ADC]; }
  Array2D& baf2_tdc()        { return data_arrays[D_BAF2_TDC]; }
  
  Array2D& pbwo4_adc()       { return data_arrays[D_PBWO4_ADC]; }
  Array2D& pbwo4_s_adc()       { return data_arrays[D_PBWO4_S_ADC]; }
  Array2D& pbwo4_tdc()       { return data_arrays[D_PBWO4_TDC]; }

  const Array2D& tagger_tdc() const    { return data_arrays[D_TAGGER_TDC]; }
  const Array2D& tagger_scaler() const { return data_arrays[D_TAGGER_SCALER]; }
  const Array2D& mwpc_w_tdc() const    { return data_arrays[D_MWPC_W_TDC]; }
  const Array2D& mwpc_s_adc() const    { return data_arrays[D_MWPC_S_ADC]; }
  const Array2D& pid_adc() const       { return data_arrays[D_PID_ADC]; }
  const Array2D& pid_tdc() const       { return data_arrays[D_PID_TDC]; }
  const Array2D& cb_adc() const        { return data_arrays[D_CB_ADC]; }
  const Array2D& cb_tdc() const        { return data_arrays[D_CB_TDC]; }
  const Array2D& veto_adc() const      { return data_arrays[D_VETO_ADC]; }
  const Array2D& veto_tdc() const      { return data_arrays[D_VETO_TDC]; }
  const Array2D& baf2_s_n_adc() const  { return data_arrays[D_BAF2_S_N_ADC]; }
  const Array2D& baf2_s_s_tdc() const  { return data_arrays[D_BAF2_S_S_ADC]; }
  const Array2D& baf2_l_n_adc() const  { return data_arrays[D_BAF2_L_N_ADC]; }
  const Array2D& baf2_l_s_adc() const  { return data_arrays[D_BAF2_L_S_ADC]; }
  const Array2D& baf2_tdc() const      { return data_arrays[D_BAF2_TDC]; }
  const Array2D& pbwo4_adc() const     { return data_arrays[D_PBWO4_ADC]; }
  const Array2D& pbwo4_s_adc() const   { return data_arrays[D_PBWO4_S_ADC]; }
  const Array2D& pbwo4_tdc() const     { return data_arrays[D_PBWO4_TDC]; }

  bool is_active(int name) const {
    return data_arrays[name].is_valid();
  }

  uint32_t get(int name, size_t ch, size_t hidx) const {
    if (static_cast<int>(name) < 0 || name >= N_DETECTORS) {
      fprintf(stderr, "Read_A2_class::get – invalid ArrayName %d\n", static_cast<int>(name));
      return 0;
    }
    return data_arrays[name].get(ch, hidx);
  }

  const std::array<int,12>& getNeighbours(int det, int element) const{
    return cfg.getNeighbours(det, element);
  }
  
  int getNeighbour(int det, int element, int neighbourNo) const{
    return cfg.getNeighbour(det, element, neighbourNo);
  }
  
  int get_hits(int name, size_t ch) const {
    if (static_cast<int>(name) < 0 || name >= N_DETECTORS) {
      fprintf(stderr, "Read_A2_class::get_hits – invalid ArrayName %d\n", static_cast<int>(name));
      return 0;
    }
    return data_arrays[name].get_hits(ch);
  }

  int get_channels(int name) const {
    if (static_cast<int>(name) < 0 || name >= N_DETECTORS) {
      fprintf(stderr, "Read_A2_class::get_channels – invalid ArrayName %d\n", static_cast<int>(name));
      return 0;
    }
    return data_arrays[name].get_channels();
  }

  const Array2D& array(int name) const {
    if (static_cast<int>(name) < 0 || name >= N_DETECTORS) {
      static Array2D dummy;
      return dummy;
    }
    return data_arrays[name];
  }

  Array2D& array(int name) {
    if (static_cast<int>(name) < 0 || name >= N_DETECTORS) {
      static Array2D dummy;
      return dummy;
    }
    return data_arrays[name];
  }

private:
  Array2D data_arrays[N_DETECTORS]{};
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
  time(&start_t);

  data_arrays[D_TAGGER_TDC]   .init(D_TAGGER_TDC,    cfg.getNoCh(D_TAGGER_TDC),    cfg.getMaxHits(D_TAGGER_TDC));
  data_arrays[D_TAGGER_SCALER].init(D_TAGGER_SCALER, cfg.getNoCh(D_TAGGER_SCALER), cfg.getMaxHits(D_TAGGER_SCALER));
  data_arrays[D_MWPC_W_TDC]   .init(D_MWPC_W_TDC,    cfg.getNoCh(D_MWPC_W_TDC),    cfg.getMaxHits(D_MWPC_W_TDC));
  data_arrays[D_MWPC_S_ADC]   .init(D_MWPC_S_ADC,    cfg.getNoCh(D_MWPC_S_ADC),    cfg.getMaxHits(D_MWPC_S_ADC));
  data_arrays[D_PID_ADC]      .init(D_PID_ADC,       cfg.getNoCh(D_PID_ADC),       cfg.getMaxHits(D_PID_ADC));
  data_arrays[D_PID_TDC]      .init(D_PID_TDC,       cfg.getNoCh(D_PID_TDC),       cfg.getMaxHits(D_PID_TDC));
  data_arrays[D_CB_ADC]       .init(D_CB_ADC,        cfg.getNoCh(D_CB_ADC),        cfg.getMaxHits(D_CB_ADC));
  data_arrays[D_CB_TDC]       .init(D_CB_TDC,        cfg.getNoCh(D_CB_TDC),        cfg.getMaxHits(D_CB_TDC));
//  data_arrays[D_VETO_ADC]     .init(D_VETO_ADC,      cfg.getNoCh(D_VETO_ADC),    cfg.getMaxHits(D_VETO_ADC));
//  data_arrays[D_VETO_TDC]     .init(D_VETO_TDC,      cfg.getNoCh(D_VETO_TDC),    cfg.getMaxHits(D_VETO_TDC));
//  data_arrays[BAF2_S_S_ADC]   .init(BAF2_S_S_ADC,    cfg.getNoCh(D_BAF2_S_S_ADC),  cfg.getMaxHits(D_BAF2_S_S_ADC));
//  data_arrays[BAF2_S_N_ADC]   .init(BAF2_S_N_ADC,    cfg.getNoCh(D_BAF2_S_N_ADC),  cfg.getMaxHits(D_BAF2_S_N_ADC));
//  data_arrays[BAF2_L_S_ADC]   .init(BAF2_L_S_ADC,    cfg.getNoCh(D_BAF2_L_S_ADC),  cfg.getMaxHits(D_BAF2_L_S_ADC));
//  data_arrays[BAF2_L_N_ADC]   .init(BAF2_L_N_ADC,    cfg.getNoCh(D_BAF2_L_N_ADC),  cfg.getMaxHits(D_BAF2_L_N_ADC));
//  data_arrays[D_BAF2_TDC]     .init(D_BAF2_TDC,     cfg.getNoCh(D_BAF2_TDC),    cfg.getMaxHits(D_BAF2_TDC));
//  data_arrays[PBWO4_ADC]    .init(PBWO4_ADC,     cfg.getNoCh(D_PBWO4_ADC),   cfg.getMaxHits(D_PBWO4_ADC));
//  data_arrays[PBWO4_S_ADC]  .init(PBWO4_S_ADC,   cfg.getNoCh(D_PBWO4_S_ADC), cfg.getMaxHits(D_PBWO4_S_ADC));
//  data_arrays[PBWO4_TDC]    .init(PBWO4_TDC,     cfg.getNoCh(D_PBWO4_TDC),   cfg.getMaxHits(D_PBWO4_TDC));
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
  
  for (size_t i = 0; i < N_DETECTORS; ++i)
    data_arrays[i].clear();
  
  cfg.reset_ref_data();       // clear reference data
  if(read_event_header()==EEndBuff) return 0;
 
  int datablock_count = 0;
//  int oldvl;
  do {
    rv = read_one_dataword(dataword);
    if(rv==0){
      printf("EoF read one event\n");
      return 0;
    }
    switch (dataword) {
      case EScalerBuffer:
		// oldvl=verboselvl;
		// verboselvl=100;
        if (verboselvl >= 20) printf("Scaler buffer: 0x%x\n", dataword);
        decode_scaler();
		// verboselvl=oldvl;
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


  int id;
  long int ref_data, data, diff;
// Tagger TDC
  for(int ch=0; ch<cfg.getNoCh(D_TAGGER_TDC); ch++){
    id=cfg.getBackId(D_TAGGER_TDC, ch);
    ref_data=cfg.get_ref_data(id);
    for(int hit=0; hit<(int)data_arrays[D_TAGGER_TDC].get_hits(ch); hit++){
      if(verboselvl>=20 && hit==0) printf("Ch: %d, ID: %d ref_data: %ld\n", ch, id, ref_data);
	    data=data_arrays[D_TAGGER_TDC].get(ch, hit);
      diff=data-ref_data+10000;
      if(ref_data!=UINT_MAX){  // valid refdata found
        if(diff<0) printf("Referenced time data is smaller 0! %ld", diff);
        data_arrays[D_TAGGER_TDC].set_at(ch, hit, diff);
	    }
	     else{
        if(verboselvl>=0) printf("No ref data found for Ch: %d, hit %d, ID: %d\n", ch, hit, id);
		    data_arrays[D_TAGGER_TDC].set_at(ch, hit, 0);
	    }
	  }
  }
//  exit(0);
  // end substract TDC reference
  
  if (verboselvl >= 20) printf("\n %i data blocks in event\n", datablock_count);
  if (verboselvl >= 20) printf("\n");
  ++noe;
  return rv;
}

int Read_A2_class::read_event_header(void) {
  fread_or_die(&eventheaderinfo, sizeof(eventheaderinfo), 1, in, "Read_A2_class::read_event_header");
  events += sizeof(eventheaderinfo) / 4;
  if(eventheaderinfo.evNo==eventheaderinfo.evLen) return eventheaderinfo.evLen; // should be EEndBuff
  if (verboselvl >= 20) {
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
  if (verboselvl >= 20) printf("ROD: 0x%08x (%u):  ", dataword, dataword);
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
  unsigned int id, value;
  int current_vbl=verboselvl;
  int ch;
  
  if (verboselvl >= 20) printf("Scaler block detected\n");
  while(1){
//    verboselvl=100;
    if(read_one_dataword(value)==0){
      printf("EoF in scaler event\n");
      break;
    }
    if(read_one_dataword(id)==0){
      printf("EoF in scaler event\n");
      break;
    }
    verboselvl=current_vbl;
    if(id == 0xfefefefe) {
      if(verboselvl >= 20) printf("****** End of scaler block \n");
      if(verboselvl >= 11) printf("Tagger scaler data ch 10: %d, ch 100: %d, ch 200: %d\n", 
                                      tagger_scaler().get(10, 0), tagger_scaler().get(100, 0),
									  tagger_scaler().get(200, 0));
      break;
    }
    
    if (verboselvl >= 20) printf("Scaler %4u  value %10u\n", id, value);
    if(id==191 && verboselvl >=11){ printf("Clock scaler   %4u  value %'10u\n", id, value); clock_scaler+=value;}
    if(id==190 && verboselvl >=11){ printf("Inhibit scaler %4u  value %'10u\n", id, value); inhibit_scaler+=value;}
    ch = cfg.getChannel(D_TAGGER_SCALER, id);
    if (ch >= 0) {
      //      if(ch>=140 && ch<=150) printf("******** TaggerScaler Ch %3i (id %4i) value %i (0x%x)\n", ch, id, value, value);
      //if(ch==1 || ch==13 || ch==21) printf("******** TaggerScaler Ch %3i (id %4i) value %i (0x%x)\n", ch, id, value, value);
      if(id==2079){
        //  printf("Stuck bit (0x4000) in 2079, fill value&0xffffbfff\n");
        value=value&0xffffbfff;
      }
      
	    tagger_scaler().set(ch, value&0xffffbfff);
      if (verboselvl >= 20)
        printf("   Tagger Scaler hit: %4i (ch %3i) V %10u\n", id, ch, value);
    }
     else if (verboselvl >= 20) {
      printf("   Unknown id: %4i, value: %10u\n", id, value);
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
    if (verboselvl >= 10)
      printf("  %i, PV: %s, bytes %u, elements=%u, type=%u, var: ",
             i, pvname, bytes, nelem, type);
    switch (type) {
      case EepicsBYTE:
        fread_or_die(&varB, ESizeBYTE, 1, in, "Read_A2_class::decode_epics – BYTE payload");
        if (verboselvl >= 10) printf("%i\n", static_cast<int>(varB));
        break;
      case EepicsSTRING:
        fread_or_die(varStr, ESizeSTRING, 1, in, "Read_A2_class::decode_epics – STRING payload");
        if (verboselvl >= 10) printf("%s\n", varStr);
        break;
      case EepicsSHORT:
        fread_or_die(&varS, ESizeSHORT, 1, in, "Read_A2_class::decode_epics – SHORT payload");
        if (verboselvl >= 10) printf("%i\n", static_cast<int>(varS));
        break;
      case EepicsLONG:
        fread_or_die(&varL, ESizeLONG, 1, in, "Read_A2_class::decode_epics – LONG payload");
        if (verboselvl >= 10) printf("%ld\n", varL);
        break;
      case EepicsFLOAT:
        fread_or_die(&varF, ESizeFLOAT, 1, in, "Read_A2_class::decode_epics – FLOAT payload");
        if (verboselvl >= 10) printf("%f\n", varF);
        break;
      case EepicsDOUBLE:
        fread_or_die(&varD, ESizeDOUBLE, 1, in, "Read_A2_class::decode_epics – DOUBLE payload");
        if (verboselvl >= 10) printf("%f\n", varD);
        break;
      default:
        if (verboselvl >= 10)
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
  int id, value;
  int ch;
  id    = dataword & 0xffff;
  value = (dataword >> 16) & 0xffff;

  
  if (verboselvl>= 20) printf("Data: id: %5i, value %5i \t", id, value);

  // test if id is an reference tdc channel and store it if it matches
  cfg.store_ref_data(id, value);

  ch = cfg.getChannel(D_TAGGER_TDC, id);
  if (ch >= 0) {
    if (verboselvl>= 20) printf("   TAGGER TDC hit: %4i (ch %3i) V %5i", id, ch, value);
    tagger_tdc().set(ch, value);
  }
  
  ch = cfg.getChannel(D_MWPC_W_TDC, id);
  if(ch >= 0){
    if(verboselvl>= 20) printf("   MWPC TDC hit: %4i (ch %3i) V %5i", id, ch, value);
    mwpc_w_tdc().set(ch, value);
  }
  ch = cfg.getChannel(D_MWPC_S_ADC, id);
  if(ch >=0){
    if(verboselvl>= 20) printf("   MWPC ADC hit: %4i (ch %3i) V %5i", id, ch, value);
    mwpc_s_adc().set(ch, value);
  }

  ch = cfg.getChannel(D_PID_ADC, id);
  if(ch >= 0){
    if(verboselvl>= 20) printf("   PID ADC hit: %4i (ch %3i) V %5i", id, ch, value);
    pid_adc().set(ch, value);
  }
  ch = cfg.getChannel(D_PID_TDC, id);
  if(ch >= 0){
    if(verboselvl>=20) printf("   PID TDC hit: %4i (ch %3i) V %5i", id, ch, value);
    pid_tdc().set(ch, value);
  }
  ch = cfg.getChannel(D_CB_ADC, id);
  if(ch >= 0){
    if(verboselvl>=20) printf("   CB ADC hit: %4i (ch %3i) V %5i", id, ch, value);
    cb_adc().set(ch, value);
  }
  ch = cfg.getChannel(D_CB_TDC, id);
  if(ch >= 0){
    if(verboselvl>=20) printf("   CB TDC hit: %4i (ch %3i) V %5i", id, ch, value);
    cb_tdc().set(ch, value);
  }

  ch = cfg.getChannel(D_VETO_ADC, id);
  if(ch >= 0){
    if(verboselvl>=20) printf("   VETO ADC hit: %4i (ch %3i) V %5i", id, ch, value);
    veto_adc().set(ch, value);
  }
  ch = cfg.getChannel(D_VETO_TDC, id);
  if(ch >= 0){
    if(verboselvl>=20) printf("   VETO TDC hit: %4i (ch %3i) V %5i", id, ch, value);
    veto_tdc().set(ch, value);
  }
  
  ch = cfg.getChannel(D_PBWO4_ADC, id);
  if(ch >= 0){
    if(verboselvl>=20) printf("   PWO ADC hit: %4i (ch %3i) V %5i", id, ch, value);
    pbwo4_adc().set(ch, value);
  }
  ch = cfg.getChannel(D_PBWO4_TDC, id);
  if(ch >= 0){
    if(verboselvl>=20) printf("   PWO TDC hit: %4i (ch %3i) V %5i", id, ch, value);
    pbwo4_tdc().set(ch, value);
  }
  ch = cfg.getChannel(D_PBWO4_S_ADC, id);
  if(ch >= 0){
    if(verboselvl>=20) printf("   PWO sens ADC hit: %4i (ch %3i) V %5i", id, ch, value);
    pbwo4_s_adc().set(ch, value);
  }
  ch = cfg.getChannel(D_PBWO4_TDC, id);
  if(ch >= 0){
    if(verboselvl>=20) printf("   PWO sens TDC hit: %4i (ch %3i) V %5i", id, ch, value);
    pbwo4_tdc().set(ch, value);
  }
    
  ch = cfg.getChannel(D_BAF2_S_N_ADC, id);
  if(ch >= 0){
    if(verboselvl>=20) printf("   BaF2 short nonsens ADC hit: %4i (ch %3i) V %5i", id, ch, value);
    baf2_s_n_adc().set(ch, value);
  }
  ch = cfg.getChannel(D_BAF2_L_N_ADC, id);
  if(ch >= 0){
    if(verboselvl>=20) printf("   BaF2 long snonens ADC hit: %4i (ch %3i) V %5i", id, ch, value);
    baf2_l_s_adc().set(ch, value);
  }
  ch = cfg.getChannel(D_BAF2_S_S_ADC, id);
  if(ch >= 0){
    if(verboselvl>=20) printf("   BaF2 short sens ADC hit: %4i (ch %3i) V %5i", id, ch, value);
    baf2_s_s_adc().set(ch, value);
  }
  ch = cfg.getChannel(D_BAF2_L_S_ADC, id);
  if(ch >= 0){
    if(verboselvl>=20) printf("   BaF2 long sens ADC hit: %4i (ch %3i) V %5i", id, ch, value);
    baf2_l_s_adc().set(ch, value);
  }
  ch = cfg.getChannel(D_BAF2_TDC, id);
  if(ch >= 0){
    if(verboselvl>=20) printf("   BaF2 long TDC hit: %4i (ch %3i) V %5i", id, ch, value);
    baf2_tdc().set(ch, value);
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
