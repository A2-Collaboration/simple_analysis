/*********************************************************************
 * paramreadout.cxx
 *
 * Minimal driver that uses the header‑only DataReader class.
 *
 * Features:
 *   - Constructs a DataReader with the file name supplied on the CLI
 *   - Calls readFile() to build the internal structures
 *   - Runs a simple prompt:
 *         a <value>   - lookup by ADC
 *         t <value>   - lookup by TDC
 *         s <value>   - lookup by Scaler
 *         m           - show maximum M‑suffixes for ADC, TDC and Scaler
 *         f           - show maximum M‑suffix for ADC
 *         d           - show maximum M‑suffix for TDC
 *         c           - show maximum M‑suffix for Scaler
 *         q           - quit
 *
 * All I/O is done with printf / sscanf / fgets (no iostreams).
 *********************************************************************/

#include "paramreader.h"      // Header only implementation
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  // strcmp, strdup, strlen, memmove 
#include <ctype.h>           // isspace()

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
       isESip=17};

enum { qADC, qTDC, qScaler };
   

const char *DetectorName[]={"NONE", "TAGGER", "MWPC_W", "MWPC_S", "PID", "CB", "VETO", "PBWO4", "PBWO4_S", "BAF2_S", "BAF2_L", "SCALER", "", "", ""};
enum                       {D_NONE, D_TAGGER, D_MWPC_W, D_MWPC_S, D_PID, D_CB, D_VETO, D_PBWO4, D_PBWO4_S, D_BAF2_S, D_BAF2_L, D_SCALER};

ParamReader pr[15];  

/* --------------------------------------------------------------
 * Trim leading and trailing whitespace in‑place.
 * -------------------------------------------------------------- */
static void trim(char *s){
   char *p = s;
   while (*p && isspace((unsigned char)*p)) ++p;
   if (p != s) memmove(s, p, strlen(p) + 1);
   char *end = s + strlen(s) - 1;
   while (end >= s && isspace((unsigned char)*end)) *end-- = '\0';   
}

char *my_strupr(char *s){
  char *p = s;
  while (*p){
    *p = toupper((unsigned char)*p);
    p++;
  }
  return s;
}


int config_readout(char * cfg_file){
  char name[64];
  int  pos_adc, n_adc;
  int  pos_tdc, n_tdc;
  int  pos_scaler, n_scaler;
  char filename[256];
  char line[1024];
  int  in_use[15]={0};

   
  FILE *fp = fopen(cfg_file, "r");
  if (!fp){
    printf("Error: cannot open config file \"%s\"\n", cfg_file);
    return false;
  }

  while (fgets(line, sizeof(line), fp)){
    trim(line); // remove additional spaces
    if(line[0] == '\0') continue;          /* empty line          */
    if(line[0] == '#')  continue;          /* comment line        */
     // Expected layout: name pos_adc n_adc pos_tdc n_tdc pos_scaler n_scaler filename
//    printf("Config line: %s\n", line);
    int fields = sscanf(line, "%63s %d %d %d %d %d %d %255s",
							  name, &pos_adc, &n_adc, &pos_tdc, &n_tdc, &pos_scaler, &n_scaler, filename);
//	printf("Name: %s\n", name);
    if(fields != 8){
      printf("Warning: malformed line (got %d fields) – skipping: %s\n", fields, line);
      continue;
    }
	for(int n=0; n<15; n++){ 
	  if(strcmp(DetectorName[n], my_strupr(name)) == 0){ // find the corresponding detector
	    printf("Found %s and inserted in as detector %i\n", DetectorName[n], n);
	    pr[n] = ParamReader(filename,pos_adc, n_adc, pos_tdc, n_tdc, pos_scaler, n_scaler); // and fill
        if(!pr[n].readFile()){  // read in the data, abort if failure
		  printf("Can't read file: %s\nAborting\n", filename);
		  return EXIT_FAILURE;
	    }		 
        in_use[n]=1; n=15;  // set it as in use and end search if detector was found
	  }
	}
  }
	 
  for(int n=0; n<15; n++){
	if((strlen(DetectorName[n])>0) && (in_use[n]==0)){
	  printf("Detector %s is not in use!\n", DetectorName[n]); 
      pr[n] = ParamReader("",0,0,0,0,0,0); // create "empty" object, returns -2 when accessed
	}
  }
  return 1;
}
int get_detector_id(int subsystem, int which, int value){
  if(which==qADC) return pr[subsystem].findADC(value);
  if(which==qTDC) return pr[subsystem].findTDC(value);
  if(which==qScaler) return pr[subsystem].findScaler(value);
  return -2;
}


int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s -i <data-file> -c <config-file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    /* -------------------------------------------------------------
     * 1. Build the ParamReader object
     * ------------------------------------------------------------- */
    /* Default token positions (the same as the original implementation):conf
     *   ADC    - token 1
     *   TDC    - token 6
     *   Scaler - token 11
     * The “number” arguments are unused but required by the ctor,
     * so we pass 0 for each.
     */
    config_readout(argv[1]);

   
    /* -------------------------------------------------------------
     * 2. Parse the file – abort if we cannot read it
     * ------------------------------------------------------------- */
 
    /* -------------------------------------------------------------
     * 3. Print a short summary
     * ------------------------------------------------------------- */
    for(int m=0; m<15; m++){
	   int n = pr[m].getNumberOfElements();
       printf("%i %s Successfully loaded %d elements.\n", m, DetectorName[m], n);
    }
   
    int n = pr[D_CB].getNumberOfElements();
    printf("Successfully loaded %d elements.\n", n);
    int value=3015; int id = pr[D_CB].findADC(value);
    printf("id=%d (ADC=%d)\n", id, value);

    value=3015; id = get_detector_id(D_CB, qADC,  value);
    printf("id=%d (ADC=%d)\n", id, value);
   
    /* -------------------------------------------------------------
     * 4. Interactive query loop (pure C‑style I/O)
     * ------------------------------------------------------------- */
    char query[256];
    while (1) {
        printf("\nEnter query (a <ADC>, t <TDC>, s <Scaler>, "
               "m/f/d/c for max M-suffixes, q to quit): ");

        if (!fgets(query, sizeof query, stdin))
            break;                     /* EOF */

        /* Skip leading whitespace */
        char *p = query;
        while (*p && isspace((unsigned char)*p))
            ++p;

        /* Quit? */
        if (*p == 'q' || *p == 'Q')
            break;

        char cmd = *p;

        /* ----- ADC lookup ------------------------------------------------ */
        if (cmd == 'a' || cmd == 'A') {
            int value;
            if (sscanf(p + 1, "%d", &value) != 1) {
                printf("  Invalid number – try again.\n");
                continue;
            }
            int id = pr[D_CB].findADC(value);
            if (id >= 0) printf("  Found: id=%d (ADC=%d)\n", id, value);
             else printf("  ADC=%d not found.\n", value);
        }
        /* ----- TDC lookup ------------------------------------------------ */
        else if (cmd == 't' || cmd == 'T') {
            int value;
            if (sscanf(p + 1, "%d", &value) != 1) {
                printf("  Invalid number – try again.\n");
                continue;
            }
            int id = pr[D_CB].findTDC(value);
            if (id >= 0) printf("  Found: id=%d (TDC=%d)\n", id, value);
             else  printf("  TDC=%d not found.\n", value);
        }
        /* ----- Scaler lookup --------------------------------------------- */
        else if (cmd == 's' || cmd == 'S') {
            int value;
            if (sscanf(p + 1, "%d", &value) != 1) {
                printf("  Invalid number – try again.\n");
                continue;
            }
            int id = pr[D_CB].findScaler(value);
            if (id >= 0) printf("  Found: id=%d (Scaler=%d)\n", id, value);
             else printf("  Scaler=%d not found.\n", value);
        }
        /* ----- Max M suffixes -------------------------------------------- */
        else if (cmd == 'm' || cmd == 'M') {
            printf("  Max M suffixes: ADC=%d, TDC=%d, Scaler=%d\n",
                   pr[D_CB].getMaxADCSuffix(),
                   pr[D_CB].getMaxTDCSuffix(),
                   pr[D_CB].getMaxScalerSuffix());
        }
        else if (cmd == 'f' || cmd == 'F') {
            printf("  Max M-suffix for ADC: %d\n", pr[D_CB].getMaxADCSuffix());
        }
        else if (cmd == 'd' || cmd == 'D') {
            printf("  Max M-suffix for TDC: %d\n", pr[D_CB].getMaxTDCSuffix());
        }
        else if (cmd == 'c' || cmd == 'C') {
            printf("  Max M-suffix for Scaler: %d\n", pr[D_CB].getMaxScalerSuffix());
        }
        else {
            printf("  Unknown command – use a/t/s/m/f/d/c/q.\n");
        }
    }

    printf("\nAll resources freed – exiting\n");
    return EXIT_SUCCESS;
}
