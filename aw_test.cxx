/**************************************

pdrexler@uni-mainz.de
peter.drexler@exp2.physik.uni-giessen.de

**************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "a2_class.h"

#include "TMath.h"
#include "TFile.h"
#include "TH1.h"
#include "TH2.h"
#include "TF1.h"
#include "TProfile.h"
#include "TNtuple.h"
#include "TRandom.h"
#include "TTree.h"

void convert(void);
void extract(void);
void calibrate(void);
double randit(int ini=0);


int ELG[16], EHG[16];

Read_A2_class detector;

int main(int argc, char *argv[])
{
  char inputfile[200];
  //  char outputfile[200];
  unsigned int no_of_events=0, do_no_of_events=0;
    
  if(argc<=1){ 
    printf("No datafile set!!!\n");
    printf("\nusage: %s datfile.dat rootfile.root [-n no_of_events] [-r (realign_first_noe)] [-m (multiple files starting with file)]\n",
          argv[0]);
    return(-1);
  }
  else strcpy(inputfile, argv[1]);
  /*  if(argc<=2){ 
    printf("No outputfile!!!\n");
    printf("\nusage: %s datfile.dat rootfile.root [-n no_of_events] [-r (realign_first_noe)] [-m (multiple files starting with file)]\n",
          argv[0]);
    return(-1);
    } else strcpy(outputfile, argv[2]);
  */
  
  for(int n=0; n<argc; n++){
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

  randit(1);
  detector.init(inputfile);

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
  /******* end initialisation histograms **************/
  
  int m=1; 
  unsigned int noe=0;
  do{
    m=detector.read_one_event();  // reads one full event into internal buffer, !=0 if there is any data

    noe++; 
    if(noe>=1) m=0;  // exit after x events analysed
    if(noe%10000==0) printf("Analysing event: %i\n",noe);

    if(m==1){  // not eof for these files
      convert();   // converts the internal data to data sort into arrays
      calibrate(); // example for calibration of energy to MeV
      
/******* begin filling histograms **************/
// valid variables:
//   ELG[0-15]
//   EHG[0-15]
/*
      for(int i=0; i<16;i++){   // sort in energy
        if(ELG[i]>0) pENERGY_LG[i]->Fill(ELG[i]);
        if(EHG[i]>0) pENERGY_HG[i]->Fill(EHG[i]);
      }
*/
/******* end filling histograms ****************/
    } 
    if(do_no_of_events==1)
        if(noe>=no_of_events) m=0;
  }while(m==1);
  /*
  printf("Writing root file...\n");	
  hfile.Write();
  hfile.Close();
  printf("Root file written!\n");
  */
  printf("\nEnd of the neverending story!\n");
}

void calibrate(void){ // example for calibration of energy to MeV

}


void convert(void){  // converts the internal data to data sort into arrays
  /*
    functions used in this example, imbeded in the ReadSystem_class class:

    unsigned int get_value(int board, int channel, int event);                     // used for all boards but FADCs
    int get_multihitno(int board, int channel); // V1190/V1290 TDCs: get the number of entries for that channel
    int get_ref_channel(int board);             // V1190/V1290 TDCs: get the reference time channel
    unsigned int get_fall_or_width(int board, int channel, int event, int iwidth); // for V1190/V1290 TDCs
    unsigned int get_trace(int board, int channel, unsigned int *array);           // for FADCs
   */

 
  // ***** Energy: V965 HG/LG

  for(int channel=0; channel<16; channel++ ){  
    //    ELG[channel]=detector.get_value(energy_module, channel*2, 0);
    // EHG[channel]=detector.get_value(energy_module, (channel*2)+1, 0);
  }
  // ***** END Energy: V965 HG/LG


}  // convert


void extract_test(void){ // for FADC: extract time/energy information from trace
  // no fadc, nothing to do here
  // for an example, look at ./old_aw/aw_proto60.cxx
} //   extract_test(void);


double randit(int ini){  // for smearing out energy channels, +-0,5ch
  if(ini==1) srand(time(NULL));
  
  return (((rand()%100) -50.) /100.);
}
