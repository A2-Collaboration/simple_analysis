/**************************************

pdrexler@uni-mainz.de

**************************************/

// no root output atm


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "A2_ReadoutClass.h"

#include "TMath.h"
#include "TFile.h"
#include "TH1.h"
#include "TH2.h"
#include "TF1.h"
#include "TProfile.h"
#include "TNtuple.h"
#include "TRandom.h"
#include "TTree.h"

void calibrate(void);
double randit(int ini=0);


int ELG[16], EHG[16];

Read_A2_class detectors;

int main(int argc, char *argv[])
{
  char inputfile[200];
  char configfile[200];
  char outputfile[200];
  unsigned int no_of_events=0, do_no_of_events=0;
  int verboselvl=0;
 // verboselvl: 0: Status, critical  10: +info 20: +debug
  time_t start_t, end_t; 
  double diff_t;
   
  if(argc<=1){ 
    printf("\nusage: %s -i datfile.dat -c configfile -o  rootfile.root [-n no_of_events]\n",
          argv[0]);
    return(-1);
  }
  
  for(int n=0; n<argc; n++){
    if(strstr(argv[n],"-i")!=NULL){  // set inputfile
      n++;  
      if(n<argc){
        strcpy(inputfile, argv[n]);
        printf("Input file: %s\n", inputfile);
      }
    }
    if(strstr(argv[n],"-c")!=NULL){  // set configfile
      n++;  
      if(n<argc){
        strcpy(configfile, argv[n]);
        printf("Config file: %s\n", configfile);
      }
    }
    if(strstr(argv[n],"-o")!=NULL){  // set inputfile
      n++;  
      if(n<argc){
        strcpy(outputfile, argv[n]);
        printf("Output file: %s\n", outputfile);
      }
    }
    if(strstr(argv[n],"-n")!=NULL){  // set stop after # of counts
      n++;  
      printf("Max count enabled!\n");
      if(n<argc){
        do_no_of_events=1;
        no_of_events=atoi(argv[n]);
      }
      else{
        printf("Missing max. number of events!\n");
        return(-1);
      }
    }
  } 

  randit(1);  // initialisation of random number, +-0.5, use: double nr=randit(); 


  time(&start_t);
  // the hard working functions are invoked here:
  // loads correlation id -> channel  for all detectors in initfile
  // prepares the readout one (including allocating memory for eventbuffer)
  detectors.init(inputfile, configfile, verboselvl);

// test if subsystems are active:
//     TAGGER_TDC, TAGGER_SCALER, MWPC_W_TDC, MWPC_S_ADC,
//     PID_ADC, PID_TDC, CB_ADC, CB_TDC,
//     VETO_ADC, VETO_TDC, BAF2_S_ADC, BAF2_S_TDC, BAF2_L_ADC, BAF2_L_TDC,
//     PBWO4_ADC, PBWO4_TDC, PBWO4_S_ADC, PBWO4_S_TDC,
  for(int n=0; n<ARRAY_COUNT; n++) {
	// exclude from test: if(n=TAGGER_SCALER) continue;
	if(!detectors.is_active(n)) {
	  printf("%s subsystem is missing!\n", SUBSYSTEM_NAMES[n]);
	  return(-1);
	}
  }
   
		

	 // ROOT init:
 
  /*
  TFile hfile(outputfile,"RECREATE","NTEC analysis");
  hfile.SetCompressionLevel(1);
  */

  // ******* begin initialisation histograms **************
  /*  
  char name[100];
  int BINS=4050;
  int RANGE=4050;

  TH1D *pENERGY_LG[16];
  for (Int_t i=0; i<16; i++){  
    sprintf(name,"ENERGY_LG_%02d",i);
    pENERGY_LG[i]=new TH1D(name,"",BINS-0,0,RANGE);
  }
  TH1D *pENERGY_HG[16];
  for (Int_t i=0; i<16; i++){  
    sprintf(name,"ENERGY_HG_%02d",i);
    pENERGY_HG[i]=new TH1D(name,"",BINS-0,0,RANGE);
  }
*/
   
   /*  hfile.mkdir ("unsort");
  hfile.mkdir ("ped_substr");
  hfile.mkdir ("sort");

  hfile.cd ("sort");
*/
  // ******* end initialisation histograms **************
  // ******* end root init
 
  int m=1; 
  unsigned int noe=0;
  do{ // begining of the readout loop 
    m=detectors.read_one_event();  // reads one full event into internal buffer, !=0 if there is any data
    noe++;
	if(m!=1) printf("\nEnd of file reached.\n");
    // if(noe>=1) m=0;  // exit after x events analysed
    if(noe%10000==0) printf("Analysing event: %i\n",noe);

    if(m==1){  // not eof for these files
      calibrate(); // example for calibration of energy to MeV
      
/******* begin filling histograms **************/
// valid detector subsystems:
//     TAGGER_TDC, TAGGER_SCALER, MWPC_W_TDC, MWPC_S_ADC,
//     PID_ADC, PID_TDC, CB_ADC, CB_TDC,
//     VETO_ADC, VETO_TDC, BAF2_S_ADC, BAF2_S_TDC, BAF2_L_ADC, BAF2_L_TDC,
//     PBWO4_ADC, PBWO4_TDC, PBWO4_S_ADC, PBWO4_S_TDC,

/*
      for(int i=0; i<16;i++){   // sort in energy
        if(ELG[i]>0) pENERGY_LG[i]->Fill(ELG[i]);
        if(EHG[i]>0) pENERGY_HG[i]->Fill(EHG[i]);
      }
*/

/******* end filling histograms ****************/
    } 
    if(do_no_of_events==1){
      if(noe>=no_of_events){
		 m=0;  // end after noe_o_events
		 printf("Number of events reached!\n");
	  }
	}	 
  }while(m==1);  // end of readout loop
   
  /*
  printf("Writing root file...\n");	
  hfile.Write();
  hfile.Close();
  printf("Root file written!\n");
  */
  time(&end_t);
  diff_t = difftime(end_t, start_t);
  printf("Execution time = %.0f s (%.1f min or %.1f h)\n", diff_t, diff_t/60.,diff_t/60./60.);
  printf("\nEnd of the neverending story!\n");
}

void calibrate(void){ // example for calibration of energy to MeV
//  double smear=randit();  // randit smear for channels, +-0.5ch
}
