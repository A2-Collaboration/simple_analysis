#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <arpa/inet.h>
#include "Rtypes.h"

#include <unistd.h>

// for getch()
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
// end for getch()

#include "ModuleIndex.h"     // for module names
#include "CB_elements.h"     // to convert id to channel CB
#include "TAGGER_elements.h" // to convert id to channel Tagger

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

// id definitions
enum { isNONE, isCB_Adc, isCB_Tdc, isTagger_Tdc, isTagger_Scaler};


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
    FILE * in;
    int no_of_int_in_file;
    int data[64];
    unsigned int events=0; // event counter
    int wie_oft=1000000;  // how often status report    
    struct AcquMk2Info_t headerinfo;
    struct ModuleHead *modulesinfo;
    struct EventHead eventheaderinfo;
    struct EpicsHead epicsheaderinfo;
    struct EpicsChan epicschan; 
    int verboselvl=20;
 public:
    Read_A2_class(){}
    ~Read_A2_class(){
      if(in!=NULL) fclose(in);
    }
    int init(char * file);
    int read_one_event(void);
    double get_value(int channel);

  private:
    void decode_adc(unsigned int dataword);
    int read_one_dataword(unsigned int &dataword);
    void undo_read_one_dataword(void);
    int read_header(void);
    int read_event_header(void);
    void read_module_definitions(void);
    void decode_epics(void);
    void decode_scaler(void);
    int  decode_id(int id, int is_scaler_event=0);
};

void  Read_A2_class::read_module_definitions(void){
  //  int rv;
  unsigned int dataword;
  
  modulesinfo = (struct ModuleHead *) malloc(sizeof(ModuleHead) * headerinfo.fNModule);
  fread(modulesinfo, sizeof(ModuleHead), headerinfo.fNModule, in);

  if(verboselvl>=10){
    printf("\nModule definitions:\n");
    for (int i = 0; i < (int) headerinfo.fNModule; i++){
      if(i!=0 && modulesinfo[i].fIndex==0) printf("\n"); // new crate
      printf("%2i Module %2u %5u  %10s  Channels %3d\n",
             i, modulesinfo[i].fIndex, modulesinfo[i].fID, getModuleNamekExpModules(modulesinfo[i].fID), modulesinfo[i].fNChannel);
    }
  }
  int zerocounter=-1;
  int cur_verboselvl=verboselvl;
  verboselvl=0;  // ignore 0s
  do{
    read_one_dataword(dataword);
    if(verboselvl>=100) printf("In zero suppression: 0x%08x\n", dataword);   
    zerocounter++;
  }while(dataword==0);
  printf("%i zeroes omitted (%f module headers)\n", zerocounter, (double) zerocounter/sizeof(ModuleHead));
  printf("thrown away Dataword: 0x%08x:  ", dataword);
  
  verboselvl=cur_verboselvl;
  
  // not needed?  undo_read_one_dataword(); // I want to read out the header of the event in the eventloop again;
}

int Read_A2_class::init(char * file){
  int rv;
  strncpy(filename, file, 99);

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
  if(headerinfo.fMk1==headerinfo.fMk2){  // if both Mk headers are the same
    printf("MK2 data format!: 0x%x\n", headerinfo.fMk2); 
  }	
   else{
    printf("MK1 data format not supported\n");
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
 
int Read_A2_class::read_one_event(void){
  unsigned int dataword;// current data word
  static int no_reads=0;
  static int noe=0;
  int rv;

  if(verboselvl>=20) printf("\n************* Start reading one event:  *******************\n");

  /*  rv=read_one_dataword(dataword);  // read type of event
  if(dataword!=EDataBuff)
    printf("Data block start not normal: 0x%x (0x70707070)\n,", dataword);
  */
  read_event_header();             // read event header
  int enable_counter=1;
  int counter=0;
  do{
    rv=read_one_dataword(dataword);  // read type ov event
    if(enable_counter==1) counter++;
    if(counter>=100)exit(0);
    switch(dataword){
      case EScalerBuffer:
        printf("Scaler buffer\n");
        decode_scaler();
        enable_counter=1;
        break;
      case EEPICSBuffer:
        printf("Epics buffer\n");
        decode_epics();
      break;
      case EEndEvent: // end of event
        printf("EndEvent\n");
        break;
      case EEndBuff:  // end of readoutfile
        printf("EndBuff\n");
        return 0;
      case EReadError:
        printf("Read Error\n");
        break;
      default:
        if(verboselvl>=20) printf("Datablock  ");
        decode_adc(dataword);
    }
    no_reads+=4;
  }while(dataword!=EEndEvent);
  noe++;
  printf("\n");
   
  return rv;
} // Read_A2_class::read_one_event


int Read_A2_class::read_event_header(void){
  fread((char*) &eventheaderinfo, sizeof(eventheaderinfo), 1, in);

  if(verboselvl>=10){
    printf("  Event: %u, len: %u, adcInd: %i, adcCnt: %i\n\n",
           eventheaderinfo.evNo, eventheaderinfo.evLen,
           eventheaderinfo.adcInd, eventheaderinfo.adcCnt);
  }
  return 0;
}

int Read_A2_class::read_one_dataword(unsigned int &dataword){
  int rv;

  rv=fread((char*) &dataword, sizeof(dataword), 1, in);
  if(rv==0) exit(0);
  if(verboselvl>=20) printf("ROD: 0x%08x:  ", dataword);
  events++; 
  if(events%wie_oft==0) printf("\n%i read in.",  events);
  return rv;
}

void Read_A2_class::decode_scaler(void){
  unsigned int index, value;
  printf("Scaler block detected\n");
  
  for(int i=0; i<2000; i++){
    read_one_dataword(value);
    read_one_dataword(index);
    printf("Scaler %u  value %u\n", index, value);
    if(decode_id(index)==isTagger_Scaler){ // Scaler
      printf("   Tagger Scaler hit: %i (ch %i) V %i\n", index, TAGGER_SCALER_to_n(index), value);
    }
     else printf("   Unknown id: %i, value: %i", index, value);
    if(index==0xfefefefe){
      printf("******\n");
      break;
    }
  }
  verboselvl=100;
}

void Read_A2_class::decode_epics(void){
    printf("\nEPICS block detected\n");
    fread((char*) &epicsheaderinfo, sizeof(epicsheaderinfo), 1, in);

    printf("Channels: %u\n", epicsheaderinfo.nchan);


    char pvname[64];
    uint16_t scrap[20];
    uint16_t bytes;
    uint16_t nelem;
    uint16_t type;

    for (int i = 0; i < epicsheaderinfo.nchan; i++){
      fread((char*) pvname, 1, 32, in);
      fread((char*) &bytes, 2, 1, in);
      fread((char*) &nelem, 2, 1, in);
      fread((char*) &type, 2, 1, in);
      fread((char*) scrap, 2, 4, in);
	   
	   
      printf("%i, PV: %s, bytes %u, elements=%u, type=%u\n",
             i, pvname, bytes, nelem, type);
      printf("scrap 0: %u, scrap 1: %u, scrap 2: %u, scrap 3: %u",
             scrap[0],scrap[1],scrap[2],scrap[3]);
	}

/*    for (int i = 0; i < epicsheaderinfo.nchan; i++){
      fread((char*) &epicschan, sizeof(epicschan), 1, in);
      printf("%i, PV: %s  elements=%u\n",
             i, epicschan.pvname, epicschan.nelem);
    }*/
   
}

void Read_A2_class::decode_adc(unsigned int dataword){
  unsigned short adc, value;
  int what;

  adc   = (dataword      ) & 0xffff;
  value = (dataword >> 16) & 0xffff;
  what=decode_id(adc);

  if(verboselvl>=20){
    printf("Data: id: %i, value %i\n", adc, value);
    if(what==isCB_Tdc){ // TDC
      printf("   TDC hit: %i (ch %i) V %i\n", adc, CB_TDC_to_n(adc), value);
    }
    if(what==isCB_Adc){ // ADC
      printf("   ADC hit: %i (ch %i) V %i\n",  adc, CB_ADC_to_n(adc), value);
    }
    if(what==isTagger_Tdc){ // TDC
      printf("   Tagger TDC hit: %i (ch %i) V %i\n", adc, TAGGER_TDC_to_n(adc), value);
    }
  }     
}

int Read_A2_class::decode_id(int id, int is_scaler_event){
  if(is_scaler_event==1){  // scaler event -> tagger scaler, not CB tdc
     //Scaler blocks are segmented: 2000-2059, 2064-2095, 2288-2383, 2576-2267, 2864-2923, 2928-2955      
    if(id>=2000 && id<=2059) return isTagger_Scaler;
    if(id>=2064 && id<=2095) return isTagger_Scaler;
    if(id>=2288 && id<=2383) return isTagger_Scaler;
    if(id>=2576 && id<=2267) return isTagger_Scaler;
    if(id>=2864 && id<=2923) return isTagger_Scaler;
    if(id>=2928 && id<=2955) return isTagger_Scaler;
  }
  if(id>=2032 && id<=2751) return isCB_Tdc; // CB TDC
  if(id>=3000 && id<=3719) return isCB_Adc; // CB ADC
  if(id>=800 && id<=1181) return isTagger_Tdc; // Tagger TDC

  return isNONE;  // none of these above
}

void Read_A2_class::undo_read_one_dataword(void){
  unsigned int dataword;
  fseek(in, -sizeof(dataword), SEEK_CUR);  // I want to read out the header of the event in the eventloop again;
}

double Read_A2_class::get_value(int channel){
  /*  if(channel>=0 && channel<max_channels){
    return data[channel];
  }
   else{
    printf("Channels out of range!\n");
    return(-1);
    }*/
    return(-1);
}


int getch () {
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
