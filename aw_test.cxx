/**************************************

pdrexler@uni-mainz.de

**************************************/

// no root output atm

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <locale.h>

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
  time_t start_t, end_t; 
  double diff_t;

 // verboselvl: 0: Status, critical  10: +info 20: +debug
  int verboselvl=0;
   
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
    if(strstr(argv[n],"-v")!=NULL){  // set verbose level
      n++;  
      printf("Verbose level!\n");
      if(n<argc){
        verboselvl=atoi(argv[n]);
      }
      else{
        printf("Missing verbose level!\n");
        return(-1);
      }
    }

  } 

  randit(1);  // initialisation of random number, +-0.5, use: double nr=randit(); 
  setlocale(LC_NUMERIC, "en_US.UTF-8"); // sepparators in numbers

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
/*  for(int n=0; n<ARRAY_COUNT; n++) {
	// exclude from test: if(n=TAGGER_SCALER) continue;
	if(!detectors.is_active(n)) {
	  printf("%s subsystem is missing!\n", SUBSYSTEM_NAMES[n]);
	  return(-1);
	}
        }*/
  // ROOT init:
  if(strlen(outputfile)<5){
    printf("No acceptable output file: %s\n", outputfile);
    return(-1);
  }
  TFile hfile(outputfile,"RECREATE","NTEC analysis");
  hfile.SetCompressionLevel(1);

  // ******* begin initialisation histograms **************
  printf("booking histograms\n");
 
  char name[100];
  int BINS=4050;
  int RANGE=4050;

  int no_of_ch;

  hfile.mkdir ("unsort");
  hfile.mkdir ("tagger");
  hfile.mkdir ("tagger_scaler");
  hfile.mkdir ("cb");

  hfile.cd ("tagger");
  no_of_ch=detectors.get_channels(D_TAGGER_TDC);
  TH1D *pTAGGER_TDC[no_of_ch];
  for (Int_t i=0; i<no_of_ch; i++){  
    sprintf(name,"TAGGER_TDC_%03d",i);
    pTAGGER_TDC[i]=new TH1D(name,"",20001,-20000,20000);
  }

  hfile.cd ("tagger_scaler");
  no_of_ch=detectors.get_channels(D_TAGGER_SCALER);
  TH1D *pTAGGER_acc=new TH1D("Tagger_acc","", no_of_ch+1, 0, no_of_ch);
  TH1D *pTAGGER_SCALER_acc=new TH1D("Tagger_Scaler_acc","", no_of_ch+1, 0, no_of_ch);
  TH1D *pTAGGER_TAGGEFF_NONCOMPENSATED = new TH1D("Tagger_TaggEff_noncompensated","", no_of_ch+1, 0, no_of_ch);
  TH1D *pTAGGER_TAGGEFF = new TH1D("Tagger_TaggEff","", no_of_ch+1, 0, no_of_ch);
/*
  TH1D *pTAGGER_SCALER[no_of_ch];
  for (Int_t i=0; i<no_of_ch; i++){  
    sprintf(name,"TAGGER_SCALER_%03d",i);
    pTAGGER_SCALER[i]=new TH1D(name,"",20001,-20000,20000);
  }
*/
  hfile.cd ("cb");
  no_of_ch=detectors.get_channels(D_CB_ADC);
  TH1D *pCB_ADC[no_of_ch];
  for (Int_t i=0; i<no_of_ch; i++){  
    sprintf(name,"CB_ADC_%03d",i);
    pCB_ADC[i]=new TH1D(name,"",BINS-0,0,RANGE);
  }
 
  hfile.cd ("unsort");

  // ******* end initialisation histograms **************
  // ******* end root init


  unsigned int ho_98=0,   ho_99=0, ho_100=0, ho_101=0, ho_102=0;
   
  int m=1; 
  unsigned int noe=0;
  long int data;
  printf("Analysing event: %'d\n",noe);
  do{ // begining of the readout loop 
    m=detectors.read_one_event();  // reads one full event into internal buffer, !=0 if there is any data
    noe++;
    if(m!=1) printf("\nEnd of file reached.\n");
    if(noe%100000==0) printf("Analysing event: %'d\n",noe);
    // if(noe>=1) m=0;  // exit after x events analysed

 /*
 if(noe== 8212500) detectors.set_verboselevel(10);
    if(noe>= 8212590){ // 8212610
      printf("Analysing event: %'d\n",noe);
      detectors.set_verboselevel(20);
    }
*/    
    if(m==1){  // not eof for these files
      calibrate(); // example for calibration of energy to MeV
      
/******* begin filling histograms **************/
// Functions:
//   - get number of channels of detector subsystem:
//       detectors.get_channels(subsystem);
//   - get number of hits of detector subsystem and channel in this event:
//       detectors.get_hits(subsystem, channel);
//   - get value:
//       detectors.get(subsystem, channel, hit);

// example: detectors.get(CB_TDC, 12, 2);

//   valid detector subsystems:
//  D_TAGGER_TDC, D_TAGGER_SCALER, D_MWPC_W_TDC, D_MWPC_S_ADC,
//  D_PID_ADC, D_PID_TDC, D_CB_ADC, D_CB_TDC,
//  D_VETO_ADC, D_VETO_TDC, D_BAF2_S_N_ADC, D_BAF2_S_S_ADC,
//  D_BAF2_L_N_ADC, D_BAF2_L_S_ADC, D_BAF2_TDC,
//  D_PBWO4_ADC, D_PBWO4_S_ADC, D_PBWO4_TDC, D_SCALER,

      for(int ch=0; ch<detectors.get_channels(D_TAGGER_TDC); ch++){   // sort in taggertime
        for(int hit=0; hit<detectors.get_hits(D_TAGGER_TDC, ch); hit++){
          data=detectors.get(D_TAGGER_TDC, ch, hit);
          if(data>0){
            pTAGGER_TDC[ch]->Fill(data);
            pTAGGER_acc->Fill(ch);
          }
        }
      }
	  
      for(int ch=0; ch<detectors.get_channels(D_TAGGER_SCALER); ch++){   // sort in tagger scaler
        data=detectors.get(D_TAGGER_SCALER, ch, 0);
        if(data>0){
//          pTAGGER_SCALER[ch]->Fill(data);
          pTAGGER_SCALER_acc->Fill(ch, data);
		  if(ch== 98){ ho_98++; } 
		  if(ch== 99){ ho_99++; }
		  if(ch==100){ ho_100++; } 
		  if(ch==101){ ho_101++; } 
		  if(ch==102){ ho_102++; } 
 	    }
	  }
	  
      for(int ch=0; ch<detectors.get_channels(D_CB_ADC); ch++){   // sort in cb adc
        data=detectors.get(D_CB_ADC, ch, 1);
        if(data>0) pCB_ADC[ch]->Fill(data);
      }
    }
/******* end filling histograms ****************/
     
    if(do_no_of_events==1){
      if(noe>=no_of_events){
		    m=0;  // end after noe_o_events
		    printf("Number of events reached!\n");
	    }
	  }	 
  }while(m==1);  // end of readout loop


  pTAGGER_TAGGEFF_NONCOMPENSATED->Divide(pTAGGER_acc, pTAGGER_SCALER_acc, 1.0, 1.0, "B");
  

  double clock_scaler = detectors.get_clock_scaler();
  double inhibit_scaler = detectors.get_inhibit_scaler();
  
  double norm=1.0;
  pTAGGER_TAGGEFF->Add(pTAGGER_TAGGEFF_NONCOMPENSATED);
  pTAGGER_TAGGEFF->Scale(1.0/norm);

  printf("clock_scaler: %'.0f   inhibit_scaler: %'.0f\n", clock_scaler, inhibit_scaler);
  printf("HO_98: %u   HO_99: %u   HO_100: %u   HO_101: %u   HO_102: %u   \n", ho_98,  ho_99,  ho_100,  ho_101,  ho_102);
  printf("From histo: Tagger scaler 98: %.0f   Tagger scaler 99: %.0f   Tagger scaler 100: %.0f    Tagger scaler 101: %.0f   Tagger scaler 102: %.0f\n",
        pTAGGER_SCALER_acc->GetBinContent(98+1),pTAGGER_SCALER_acc->GetBinContent(99+1),pTAGGER_SCALER_acc->GetBinContent(100+1),
        pTAGGER_SCALER_acc->GetBinContent(101+1),pTAGGER_SCALER_acc->GetBinContent(102+1));
  // +1: root GetBinHisto 0: underflow bin, bin = 1; first bin with low-edge xlow INCLUDED		 

  printf("Writing root file...\n");	
  hfile.Write();
  hfile.Close();
  printf("Root file written!\n");
  
  time(&end_t);
  diff_t = difftime(end_t, start_t);
  printf("Execution time = %.0f s (%.1f min or %.1f h)\n", diff_t, diff_t/60.,diff_t/60./60.);
  printf("Events analysed: %d\n", noe);
  printf("\nEnd of the neverending story!\n");
}

void calibrate(void){ // example for calibration of energy to MeV
//  double smear=randit();  // randit smear for channels, +-0.5ch
}
