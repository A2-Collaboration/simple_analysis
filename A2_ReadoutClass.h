#ifndef A2_READOUTCLASS_H
#define A2_READOUTCLASS_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <arpa/inet.h>

#include <vector>
#include <algorithm>
#include <cstdio>
#include <stdexcept>

#include <unistd.h>


// for getch()
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
// end for getch()

#include "ModuleIndex.h"     // for module names
#include "Rtypes.h"          
#include "ConfigManager.h"   // for reading in the channel number/detector type of the id

using namespace std;

// =========================================================
//   Marker values used inside AcquDAQ data files
// ========================================================= 

#define EHeadBuff      0x10101010
#define EDataBuff      0x70707070
#define EEndEvent      0xFFFFFFFF
#define EEndBuff       0x30303030
#define EScalerBuffer  0xFEFEFEFE
#define EEPICSBuffer   0xFDFDFDFD
#define EReadError     0xEFEFEFEF

// =========================================================
//   Run Header Structure (from Mk2Format.py)
// ========================================================= 

// Array sizes
enum { EMk2SizeTime = 32, EMk2SizeComment = 256, EMk2SizeFName = 128,
       EMk2SizeDesc = 256 };

enum ArrayName {
        TAGGER_TDC, TAGGER_SCALER, MWPC_W_TDC, MWPC_S_ADC,
        PID_ADC, PID_TDC, CB_ADC, CB_TDC,
        VETO_ADC, VETO_TDC, BAF2_S_ADC, BAF2_S_TDC, BAF2_L_ADC, BAF2_L_TDC,
        PBWO4_ADC, PBWO4_TDC, PBWO4_S_ADC, PBWO4_S_TDC,
        ARRAY_COUNT};
static const char * SUBSYSTEM_NAMES[] = {"TAGGER_TDC", "TAGGER_SCALER", 
        "MWPC_W_TDC", "MWPC_S_ADC", "PID_ADC", "PID_TDC", "CB_ADC", "CB_TDC",
        "VETO_ADC", "VETO_TDC", "BAF2_S_ADC", "BAF2_S_TDC", "BAF2_L_ADC", "BAF2_L_TDC",
        "PBWO4_ADC", "PBWO4_TDC", "PBWO4_S_ADC", "PBWO4_S_TDC"};
        
// ======================================================
// Array2D: Channel-major storage (hits contiguous per channel)
// ======================================================
class Array2D {
    std::vector<unsigned int> data;
    size_t hits{}, channels{};
    bool valid{false};

    // Channel-major indexing: hits are contiguous for each channel
    size_t index(size_t hidx, size_t ch) const {
        return ch * hits + hidx;
    }

public:
    Array2D() = default;
    Array2D(size_t h, size_t c) { init(h, c); }

    unsigned int get(size_t ch, size_t hidx) const {
        if (!valid || ch >= channels || hidx >= hits) {
            fprintf(stderr, "Warning: Accessing invalid element ch=%zu h=%zu\n", ch, hidx);
            return 0;
        }
        return data[index(hidx, ch)];
    }

    void set(size_t ch, size_t hidx, unsigned int val) {
        if (!valid || ch >= channels || hidx >= hits) {
            fprintf(stderr, "Warning: Setting invalid element ch=%zu h=%zu\n", ch, hidx);
            return;
        }
        data[index(hidx, ch)] = val;
    }

    unsigned int& operator()(size_t ch, size_t hidx) {
        if (!valid || ch >= channels || hidx >= hits)
            throw std::out_of_range("Array2D access");
        return data[index(hidx, ch)];
    }

    const unsigned int& operator()(size_t ch, size_t hidx) const {
        if (!valid || ch >= channels || hidx >= hits)
            throw std::out_of_range("Array2D access");
        return data[index(hidx, ch)];
    }

    size_t get_hits() const { return valid ? hits : 0; }
    size_t get_channels() const { return valid ? channels : 0; }

    void clear() {
        std::fill(data.begin(), data.end(), 0u);  // zero all elements
    }
   void init(size_t c, size_t h) {
        hits = h;
        channels = c;
        data.assign(h * c, 0u);  // allocate and zero
        valid = true;
    }

    bool is_valid() const { return valid; }
};

/*
// id definitions
enum { isNONE=0,
       isTagger_Tdc=1, isTagger_Scaler=2,
       isMWPC_W_Tdc=3, isMWPC_S_Adc=4,
       isPID_Adc=5, isPID_Tdc=6,
       isCB_Adc=7, isCB_Tdc=8,
       isTAPSVeto_Adc=9, isTAPSVeto_Tdc=10,
       isTAPS_S_Adc=11, isTAPS_S_Tdc=12,
       isTAPS_L_Adc=13, isTAPS_L_Tdc=14,
       isTAPSPWO_Adc=15, isTAPSPWO_Tdc=16,
       isScaler=17, isESip=18};
*/

enum  EepicsType{              EepicsBYTE,    EepicsSTRING,     EepicsSHORT,    EepicsLONG,    EepicsFLOAT,    EepicsDOUBLE, EepicsNULL};
const char *epicsTypeName[] = {"epicsBYTE",   "epicsSTRING",    "epicsSHORT",   "epicsLONG",   "epicsFLOAT",   "epicsDOUBLE",      NULL};
enum  EepicsTypeSize{           ESizeBYTE = 1, ESizeSTRING = 40, ESizeSHORT = 2, ESizeLONG = 8, ESizeFLOAT = 4, ESizeDOUBLE = 8};


struct AcquMk2Info_t {	       	      // 1st part of header buffer
  UInt_t fMk1;                        // Mark as Mk1 or Mk2 data
  UInt_t fMk2;                        // Mark as Mk2 data
  Char_t fTime[EMk2SizeTime];	      // run start time (ascii)
  Char_t fDescription[EMk2SizeDesc];  // description of experiment
  Char_t fRunNote[EMk2SizeComment];   // particular run note
  Char_t fOutFile[EMk2SizeFName];     // output file
  Int_t fRun;		              // run number
  Int_t fNModule;		      // total no. modules
  Int_t fNADCModule;	       	      // no. ADC modules
  Int_t fNScalerModule;	              // no. scaler modules
  Int_t fNADC;		              // no. ADC's read out
  Int_t fNScaler;	    	      // no. scalers readout
  Int_t fRecLen;		      // maximum buffer length = record len
};

// =========================================================
//   Module description
// ========================================================= 
struct  ModuleHead {
    int32_t fID;
    int32_t fIndex;
    int32_t fModuleType;
    int32_t fMinChannel;
    int32_t fNChannel;
    int32_t fNScalerChannels;
    int32_t fNBits;
};


// =========================================================
//   Event header
// ========================================================= 
struct EventHead {
    uint32_t evNo;
    uint32_t evLen;
    uint16_t adcInd;
    uint16_t adcCnt;

};

struct EpicsHead{
    char epics[32];
    uint32_t time;
    uint16_t index;
    uint16_t period;
    uint16_t id;
    uint16_t nchan;
    uint16_t len;
};

struct EpicsChan{
    char pvname[32];
    uint16_t bytes;
    uint16_t nelem;
    uint16_t type;
};

int getch(void);

class Read_A2_class{
  protected:
    char filename[100];
    char configfilename[100];
    FILE * in, * conf_in;
    unsigned long no_of_int_in_file;
    int data[64];
    ConfigManager cfg;
    long unsigned int events=0; // event counter
    int wie_oft=1000000;  // how often status report    
    struct AcquMk2Info_t headerinfo;
    struct ModuleHead *modulesinfo;
    struct EventHead eventheaderinfo;
    struct EpicsHead epicsheaderinfo;
    struct EpicsChan epicschan; 
    int verboselvl;
 public:
    Read_A2_class(){}
    ~Read_A2_class(){
      if(in!=NULL) fclose(in);
    }
    int init(char * file, char * configfile, int verboselvl_=20);
    int read_one_event(void);
    double get_value(int channel);

        // ---------------- Accessors ----------------
    Array2D& tagger_tdc()      { return arrays[TAGGER_TDC]; }
    Array2D& tagger_scaler()   { return arrays[TAGGER_SCALER]; }
    Array2D& mwpc_w_tdc()      { return arrays[MWPC_W_TDC]; }
    Array2D& mwpc_s_adc()      { return arrays[MWPC_S_ADC]; }
    Array2D& pid_adc()         { return arrays[PID_ADC]; }
    Array2D& pid_tdc()         { return arrays[PID_TDC]; }
    Array2D& cb_adc()          { return arrays[CB_ADC]; }
    Array2D& cb_tdc()          { return arrays[CB_TDC]; }
    Array2D& veto_adc()        { return arrays[VETO_ADC]; }
    Array2D& veto_tdc()        { return arrays[VETO_TDC]; }
    Array2D& baf2_s_adc()      { return arrays[BAF2_S_ADC]; }
    Array2D& baf2_s_tdc()      { return arrays[BAF2_S_TDC]; }
    Array2D& baf2_l_adc()      { return arrays[BAF2_L_ADC]; }
    Array2D& baf2_l_tdc()      { return arrays[BAF2_L_TDC]; }
    Array2D& pbwo4_adc()       { return arrays[PBWO4_ADC]; }
    Array2D& pbwo4_tdc()       { return arrays[PBWO4_TDC]; }
    Array2D& pbwo4_s_adc()     { return arrays[PBWO4_S_ADC]; }
    Array2D& pbwo4_s_tdc()     { return arrays[PBWO4_S_TDC]; }

    const Array2D& tagger_tdc() const      { return arrays[TAGGER_TDC]; }
    const Array2D& tagger_scaler() const   { return arrays[TAGGER_SCALER]; }
    const Array2D& mwpc_w_tdc() const      { return arrays[MWPC_W_TDC]; }
    const Array2D& mwpc_s_adc() const      { return arrays[MWPC_S_ADC]; }
    const Array2D& pid_adc() const         { return arrays[PID_ADC]; }
    const Array2D& pid_tdc() const         { return arrays[PID_TDC]; }
    const Array2D& cb_adc() const          { return arrays[CB_ADC]; }
    const Array2D& cb_tdc() const          { return arrays[CB_TDC]; }
    const Array2D& veto_adc() const        { return arrays[VETO_ADC]; }
    const Array2D& veto_tdc() const        { return arrays[VETO_TDC]; }
    const Array2D& baf2_s_adc() const      { return arrays[BAF2_S_ADC]; }
    const Array2D& baf2_s_tdc() const      { return arrays[BAF2_S_TDC]; }
    const Array2D& baf2_l_adc() const      { return arrays[BAF2_L_ADC]; }
    const Array2D& baf2_l_tdc() const      { return arrays[BAF2_L_TDC]; }
    const Array2D& pbwo4_adc() const       { return arrays[PBWO4_ADC]; }
    const Array2D& pbwo4_tdc() const       { return arrays[PBWO4_TDC]; }
    const Array2D& pbwo4_s_adc() const     { return arrays[PBWO4_S_ADC]; }
    const Array2D& pbwo4_s_tdc() const     { return arrays[PBWO4_S_TDC]; }

    bool is_active(int name) const {
        return arrays[name].is_valid();
    }

  private:
    Array2D arrays[ARRAY_COUNT]{};
    int read_one_dataword(unsigned int &dataword);
    void undo_read_one_dataword(void);
    int read_header(void);
    int read_event_header(void);
    void read_module_definitions(void);
    void decode_epics(void);
    void decode_scaler(void);
    void decode_adc(unsigned int dataword);
};

int Read_A2_class::init(char * file, char * configfile, int verboselvl_){
  int rv;
  verboselvl=verboselvl_;
   
  strncpy(filename, file, 99);

  if (!cfg.readConfig(configfile, verboselvl)) {
    printf("Configuration failed\n");  // get the parameters from the config file
    return EXIT_FAILURE;
  }
  if(verboselvl>=10) cfg.printSummary();
  
  // ******************* create data arrays due to the information from cfg:
  // arrays[ detectorpart ].init(no_of_ch, maxno_of_hits);
    arrays[TAGGER_TDC].init(    cfg.getnoCh(D_TAGGER),  cfg.getTDCMaxHits(D_TAGGER));
    arrays[TAGGER_SCALER].init( cfg.getnoCh(D_TAGGER),  cfg.getSCALERMaxHits(D_TAGGER));
    arrays[MWPC_W_TDC].init(    cfg.getnoCh(D_MWPC_W),  cfg.getTDCMaxHits(D_MWPC_W));
    arrays[MWPC_S_ADC].init(    cfg.getnoCh(D_MWPC_S),  cfg.getADCMaxHits(D_MWPC_S));
    arrays[PID_ADC].init(       cfg.getnoCh(D_PID),     cfg.getADCMaxHits(D_PID));
    arrays[PID_TDC].init(       cfg.getnoCh(D_PID),     cfg.getTDCMaxHits(D_PID));
    arrays[CB_ADC].init(        cfg.getnoCh(D_CB),      cfg.getADCMaxHits(D_CB));
    arrays[CB_TDC].init(        cfg.getnoCh(D_CB),      cfg.getTDCMaxHits(D_CB));
    arrays[VETO_ADC].init(      cfg.getnoCh(D_VETO),    cfg.getADCMaxHits(D_VETO));
    arrays[VETO_TDC].init(      cfg.getnoCh(D_VETO),    cfg.getTDCMaxHits(D_VETO));
    arrays[BAF2_S_ADC].init(    cfg.getnoCh(D_BAF2_S),  cfg.getADCMaxHits(D_BAF2_S));
    arrays[BAF2_S_TDC].init(    cfg.getnoCh(D_BAF2_S),  cfg.getTDCMaxHits(D_BAF2_S));
    arrays[BAF2_L_ADC].init(    cfg.getnoCh(D_BAF2_L),  cfg.getADCMaxHits(D_BAF2_L));
    arrays[BAF2_L_TDC].init(    cfg.getnoCh(D_BAF2_L),  cfg.getTDCMaxHits(D_BAF2_L));
    arrays[PBWO4_ADC].init(     cfg.getnoCh(D_PBWO4),   cfg.getADCMaxHits(D_PBWO4));
    arrays[PBWO4_TDC].init(     cfg.getnoCh(D_PBWO4),   cfg.getTDCMaxHits(D_PBWO4));
    arrays[PBWO4_S_ADC].init(   cfg.getnoCh(D_PBWO4_S), cfg.getADCMaxHits(D_PBWO4_S));
    arrays[PBWO4_S_TDC].init(   cfg.getnoCh(D_PBWO4_S), cfg.getTDCMaxHits(D_PBWO4_S));

   
  in = fopen(filename, "rb");	
  if(in==NULL){ 
    printf("%s doesn't exist!!!\n", filename);
    exit(-2);
  }
  fseek(in, 0, 2); // set read pointer to end of file
  no_of_int_in_file=ftell(in)/4; // 32bit=4byte long data values, lenght of header trail included

  fseek(in, 0, 0); //set read pointer to beginning of file
  rv=read_header();
  return rv;
}

int Read_A2_class::read_header(void){  
  fread((char*) &headerinfo, sizeof(headerinfo), 1, in); // Mark as Mk1 or Mk2 data
  events+=sizeof(headerinfo)/4;
   
  if(headerinfo.fMk1==headerinfo.fMk2){  // if both Mk headers are the same
    if(verboselvl>=10) printf("MK2 data format!: 0x%x\n", headerinfo.fMk2); 
  }	
   else{
    printf("MK1 data format not supported\n");
	exit(-2);
  }

  if(verboselvl>=10){
    printf("MK1: 0x%x\n", headerinfo.fMk1);  
    printf("MK2: 0x%x\n", headerinfo.fMk2);  
    printf("Time: %s",headerinfo.fTime);
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

void  Read_A2_class::read_module_definitions(void){
  //  int rv;
  unsigned int dataword;
  
  modulesinfo = (struct ModuleHead *) malloc(sizeof(ModuleHead) * headerinfo.fNModule);
  fread(modulesinfo, sizeof(ModuleHead), headerinfo.fNModule, in);
  events+=sizeof(ModuleHead)/4;
  if(verboselvl>=10){
    printf("\nModule definitions:\n");
    for (int i = 0; i < (int) headerinfo.fNModule; i++){
      if(i!=0 && modulesinfo[i].fIndex==0) printf("\n"); // new crate
      printf("%2i Module %2u %5u  %10s  Channels %3d\n",
             i, modulesinfo[i].fIndex, modulesinfo[i].fID, getModuleNamekExpModules(modulesinfo[i].fID), modulesinfo[i].fNChannel);
    }
  }
  int zerocounter=-1;
  int previous_verboselvl=verboselvl;  // save current verboselvl
  verboselvl=0;  // skip printing out zero suppression
  do{
    read_one_dataword(dataword);
    if(verboselvl>=50) printf("In zero suppression: 0x%08x\n", dataword);   
    zerocounter++;
  }while(dataword==0);
  verboselvl=previous_verboselvl; // restore previous verboselvl
  if(verboselvl>=20) printf("%i zeroes omitted (%f module headers)\n", zerocounter, (double) zerocounter/sizeof(ModuleHead));
}


int Read_A2_class::read_one_event(void){
  unsigned int dataword;// current data word
  static int no_reads=0;
  static int noe=0;
  int rv;

  if(verboselvl>=20) printf("\n************* Start reading one event:  *******************\n");

  // Zero all arrays
  for (size_t i = 0; i < ARRAY_COUNT; ++i) {
    arrays[i].clear();
  }

  read_event_header();             // read event header

  int datablock_count=0;
  do{
    rv=read_one_dataword(dataword);  // read type ov event
    switch(dataword){
      case EScalerBuffer:
        if(verboselvl>=20) printf("Scaler buffer\n");
        decode_scaler();
        break;
      case EEPICSBuffer:
        if(verboselvl>=20) printf("Epics buffer\n");
        decode_epics();
      break;
      case EEndEvent: // end of event
        if(verboselvl>=20) printf("EndEvent\n");
        break;
      case EEndBuff:  // end of readoutfile
        if(verboselvl>=20) printf("EndBuff\n");
        return 0;
      case EReadError:
       if(verboselvl>=10)  printf("Read Error\n");
        break;
      default:
        if(verboselvl>=20) printf("normal data  ");
        datablock_count++;
        decode_adc(dataword);
    }
    no_reads+=4;
  }while(dataword!=EEndEvent);
  if(verboselvl>=10) printf("\n %i data blocks in event\n", datablock_count);
  if(verboselvl>=20) printf("\n");
  noe++;
 
  return rv;
} // Read_A2_class::read_one_event


int Read_A2_class::read_event_header(void){
  fread((char*) &eventheaderinfo, sizeof(eventheaderinfo), 1, in);
  events+=sizeof(eventheaderinfo)/4;

  if(verboselvl>=10){
    printf("  Event: %u, len: %u, adcInd: %i, adcCnt: %i\n\n",
           eventheaderinfo.evNo, eventheaderinfo.evLen,
           eventheaderinfo.adcInd, eventheaderinfo.adcCnt);
  }
  return 0;
} // int Read_A2_class::read_event_header(void){


int Read_A2_class::read_one_dataword(unsigned int &dataword){
  int rv;

  rv=fread((char*) &dataword, sizeof(dataword), 1, in);
  events++; 
  if(rv==0) return 0;
  if(verboselvl>=20) printf("ROD: 0x%08x:  ", dataword);

  if(events%wie_oft==0){
    printf("%3.1f%% read in.\n", (double) events/no_of_int_in_file*100.0);
  }
   
  return rv;
} // int Read_A2_class::read_one_dataword(unsigned int &dataword){



void Read_A2_class::decode_scaler(void){
  unsigned int index, value;
  // int ists=0;
  int ch;
  if(verboselvl>=10) printf("Scaler block detected\n");
  
  for(int i=0; i<2000; i++){
    read_one_dataword(value);
    read_one_dataword(index);
    if(index==0xfefefefe){
      if(verboselvl>=20) printf("****** End of tagger block \n");
      break;
    }
    if(verboselvl>=20) printf("Scaler %4u  value %10u", index, value);
	ch=cfg.get(D_TAGGER).findScaler(index);
    if(ch>=0){ // Tagger scaler
      if(verboselvl>=20) printf("   Tagger Scaler hit: %4i (ch %3i) V %10u\n", index, ch, value);
// save scaler here

      // ists=1;
    }
     else if(verboselvl>=20) printf("   Unknown id: %4i, value: %10u\n", index, value);
  }
  // if(ists==1) exit(0);
}  // void Read_A2_class::decode_scaler(void){


void Read_A2_class::decode_epics(void){
  if(verboselvl>=10) printf("  EPICS block detected\n");
  fread((char*) &epicsheaderinfo, sizeof(epicsheaderinfo), 1, in);
  events+=sizeof(epicsheaderinfo); 

  if(verboselvl>=10) printf("  Channels: %u\n", epicsheaderinfo.nchan);

  char pvname[64];
  uint16_t scrap[20];
  uint16_t bytes;
  uint16_t nelem;
  uint16_t type;

  char   varB;
  char   varStr[ESizeSTRING];
  short  varS;
  long   varL;
  float  varF;
  double varD;
	
  for (int i = 0; i < epicsheaderinfo.nchan; i++){
    fread((char*) pvname, 1, 32, in);
    fread((char*) &bytes, 2, 1, in);
    fread((char*) &nelem, 2, 1, in);
    fread((char*) &type, 2, 1, in);
    events+=32/4; 
  
    if(verboselvl>=20) printf("  %i, PV: %s, bytes %u, elements=%u, type=%u, var: ",
                                  i, pvname, bytes, nelem, type);

    switch(type){                                //work out type and print formatted as appropriate
      case EepicsBYTE:
        fread(&varB, ESizeBYTE, 1, in);
        events++; 
	    if(verboselvl>=20) printf("%i\n", (int) varB);
      break;
      case EepicsSTRING:
        fread(varStr, ESizeSTRING, 1, in);
        events++; 
	    if(verboselvl>=20) printf("%s\n", varStr);
      break;
      case EepicsSHORT:
        fread(&varS, ESizeSHORT, 1, in);
        events++; 
	    if(verboselvl>=20) printf("%i\n", (int) varS);
      break;
      case EepicsLONG:
        fread(&varL, ESizeLONG, 1, in);
        events++; 
	    if(verboselvl>=20) printf("%ld\n", varL);
      break;
      case EepicsFLOAT:
        fread(&varF, ESizeFLOAT, 1, in);
        events++; 
	    if(verboselvl>=20) printf("%f\n", varF);
      break;
      case EepicsDOUBLE:
        fread(&varD, ESizeDOUBLE, 1, in);
        events++; 
	    if(verboselvl>=20) printf("%f\n", varD);
      break;
      default:
        if(verboselvl>=20) printf("\nWARNING: Unknown epics data type: %u for EPICS channel %s\n", type, pvname);
      break;
    }
  }
  if( (epicsheaderinfo.nchan%2) == 1){ // we read out 32bit words, it will get misaligned if we read out
                                      // odd 16 bit words in the epics event
    if(verboselvl>=20) printf("  odd prevention\n");
    fread((char*) scrap, 2, 1, in); 
  }
} // void Read_A2_class::decode_epics(void){


void Read_A2_class::decode_adc(unsigned int dataword){
  unsigned short id, value;

  id   = (dataword      ) & 0xffff; // use the lower 16 bit of the data word
  value = (dataword >> 16) & 0xffff; // shift 16 bits to the right and use the remaining 16 bit
 
  if(verboselvl>=20) printf("Data: id: %5i, value %5i \t", id, value);

  // tagger scalers are in the scaler block, not here in the normal data
  
 // TODO:
/*
        // Fill CB_ADC (channel-major access is now cache-friendly)
        for (size_t ch = 0; ch < arrays[CB_ADC].get_channels(); ++ch)
            for (size_t h = 0; h < arrays[CB_ADC].get_hits(); ++h)
                arrays[CB_ADC].set(ch, h, static_cast<unsigned int>(ch  + h));
  */
  
  // test if the id is in the tagger tdc range (ch>=0), then save it.
  // same for all other detector components
  int ch=cfg.get(D_TAGGER).findTDC(id);
  if(ch>=0){ // Tagger TDC
    if(verboselvl>=20) printf("   CB TDC hit: %4i (ch %3i) V %5i", id, ch, value);
  }
  // MWPC:  Wires have only TDC values; Strips have ADC values only
  ch=cfg.get(D_MWPC_W).findTDC(id);
  if(ch>=0){ // MWPC wires TDC
    if(verboselvl>=20) printf("   MWPC TDC hit: %4i (ch %3i) V %5i", id, ch, value);
  }
  ch=cfg.get(D_MWPC_S).findADC(id);
  if(ch>=0){ // MWPC strips ADC
    if(verboselvl>=20) printf("   MWPC ADC hit: %4i (ch %3i) V %5i", id, ch, value);
  }
  // PID
  ch=cfg.get(D_PID).findADC(id);
  if(ch>=0){ // PID ADC
    if(verboselvl>=20) printf("   PID ADC hit: %4i (ch %3i) V %5i", id, ch, value);
  }
  ch=cfg.get(D_PID).findTDC(id);
  if(ch>=0){ // PID TDC
    if(verboselvl>=20) printf("   PID TDC hit: %4i (ch %3i) V %5i", id, ch, value);
  }
  // CB
  ch=cfg.get(D_CB).findADC(id);
  if(ch>=0){ // CB ADC
    if(verboselvl>=20) printf("   CB ADC hit: %4i (ch %3i) V %5i", id, ch, value);
  }
  ch=cfg.get(D_CB).findTDC(id);
  if(ch>=0){ // CB TDC
    if(verboselvl>=20) printf("   CB TDC hit: %4i (ch %3i) V %5i", id, ch, value);
  }
  // TAPS Veto
  ch=cfg.get(D_VETO).findADC(id);
  if(ch>=0){ // VETO ADC
    if(verboselvl>=20) printf("   VETO ADC hit: %4i (ch %3i) V %5i", id, ch, value);
  }
  ch=cfg.get(D_VETO).findTDC(id);
  if(ch>=0){ // VETO TDC
    if(verboselvl>=20) printf("   VETO TDC hit: %4i (ch %3i) V %5i", id, ch, value);
  }
  // TAPS PWO has only one TDC in each channel, data should be duplicated, using norma channel after debug
  ch=cfg.get(D_PBWO4).findADC(id);
  if(ch>=0){ // PWO ADC
    if(verboselvl>=20) printf("   PWO ADC hit: %4i (ch %3i) V %5i", id, ch, value);
  }
  ch=cfg.get(D_PBWO4).findTDC(id);
  if(ch>=0){ // PWO TDC
    if(verboselvl>=20) printf("   PWO TDC hit: %4i (ch %3i) V %5i", id, ch, value);
  }
  ch=cfg.get(D_PBWO4_S).findADC(id);
  if(ch>=0){ // PWO ADC sensitive
    if(verboselvl>=20) printf("   PWO sens ADC hit: %4i (ch %3i) V %5i", id, ch, value);
  }
  ch=cfg.get(D_PBWO4_S).findTDC(id);
  if(ch>=0){ // PWO TDC sensitive
    if(verboselvl>=20) printf("   PWO sens TDC hit: %4i (ch %3i) V %5i", id, ch, value);
  }
  // TAPS has only one TDC for LG,LGS,SG,SGS (Date duplicated). Taking TDC value from BaF2_L after debugging
  ch=cfg.get(D_BAF2_S).findADC(id);
  if(ch>=0){ // TAPS short gate ADC
    if(verboselvl>=20) printf("   BaF2 short ADC hit: %4i (ch %3i) V %5i", id, ch, value);
  }
  ch=cfg.get(D_BAF2_L).findADC(id);
  if(ch>=0){ // TAPS long gate ADC
    if(verboselvl>=20) printf("   BaF2 long ADC hit: %4i (ch %3i) V %5i", id, ch, value);
  }
  ch=cfg.get(D_BAF2_L).findTDC(id);
  if(ch>=0){ // TAPS long gate TDC
    if(verboselvl>=20) printf("   BaF2 long TDC hit: %4i (ch %3i) V %5i", id, ch, value);
  }
  ch=cfg.get(D_BAF2_S).findTDC(id);
  if(ch>=0){ // TAPS short gate TDC
    if(verboselvl>=20) printf("   BaF2 short TDC hit: %4i (ch %3i) V %5i", id, ch, value);
  }


  if(id==400){
    if(verboselvl>=20) printf("   ESip hit: %4i V %5i", id, value);
  }
  if(verboselvl>=20) printf("\n");
} // void Read_A2_class::decode_adc(unsigned int dataword){


void Read_A2_class::undo_read_one_dataword(void){
  unsigned int dataword;
  fseek(in, -sizeof(dataword), SEEK_CUR);  // I want to read out the header of the event in the eventloop again;
} // Read_A2_class::undo_read_one_dataword(void){


int getch() {
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
  tcsetattr (STDIN_FILENO, TCSANOW, &tc_attrib);
  return ch;
}


double randit(int ini){
  // for smearing out energy channels, +-05ch
 if(ini==1) srand(time(NULL));
 return (((rand()%100) -50.) /100.);
}


#endif

