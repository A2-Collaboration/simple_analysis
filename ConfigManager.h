#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include "ParamReader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Enums (unchanged) */
enum { qADC, qTDC, qScaler };

enum {
    D_NONE, D_TAGGER, D_MWPC_W, D_MWPC_S, D_PID,
    D_CB, D_VETO, D_PBWO4, D_PBWO4_S,
    D_BAF2_S, D_BAF2_L, D_SCALER, N_DETECTORS
};

class ConfigManager {
public:
 
    ConfigManager() {
        for(int i = 0; i < N_DETECTORS; i++)
            in_use[i] = 0;
    }

    int readConfig(const char *cfg_file){
        char name[64];
        int pos_adc, n_adc, pos_tdc, n_tdc, pos_scaler, n_scaler;
        char filename[256];
        char line[1024];

        FILE *fp = fopen(cfg_file, "r");
        if (!fp){
            printf("Error: cannot open config file \"%s\"\n", cfg_file);
            return 0;
        }

        while (fgets(line, sizeof(line), fp)){
            trim(line);
            if(line[0]=='\0' || line[0]=='#') continue;

            int fields = sscanf(line,"%63s %d %d %d %d %d %d %255s",
                name,&pos_adc,&n_adc,&pos_tdc,&n_tdc,
                &pos_scaler,&n_scaler,filename);
            if(fields!=8){
                printf("Warning: malformed line – skipping: %s\n", line);
                continue;
            }
            my_strupr(name);
            int det = findDetector(name);
            if(det >= 0){
                printf("Found %s as detector %d\n", DetectorName[det], det);
                pr[det].init(filename,pos_adc,n_adc,pos_tdc,n_tdc,pos_scaler,n_scaler);
                if(!pr[det].readFile()){
                    printf("Can't read file: %s\n", filename);
                    fclose(fp);
                    return 0;
                }
                in_use[det] = 1;
            }
            else{
                printf("Warning: unknown detector '%s'\n", name);
            }
        }
        fclose(fp);
        for(int n=0; n<N_DETECTORS; n++){
            if(strlen(DetectorName[n])>0 && !in_use[n]){
                printf("Detector %s not in use\n", DetectorName[n]);
                pr[n].init("",0,0,0,0,0,0);
            }
        }
        return 1;
    }

    void printSummary(){
        for(int m=0; m<N_DETECTORS; m++){
            printf("%d %s loaded %d elements\n",
                m, DetectorName[m], pr[m].getNumberOfElements());
        }
    }

    ParamReader& get(int det){
        return pr[det];
    }

    int findDetector(const char *name){
        for(int i = 0; i < N_DETECTORS; i++){
            if(strcmp(DetectorName[i], name) == 0)
                return i;
        }
        return -1;
    }

    const char* getDetectorName(int det) const {
        if(det < 0 || det >= N_DETECTORS) return "INVALID";
        return DetectorName[det];
    }

private:
    ParamReader pr[N_DETECTORS];
    int in_use[N_DETECTORS];

    static inline const char *DetectorName[N_DETECTORS] = {
        "NONE","TAGGER","MWPC_W","MWPC_S","PID","CB","VETO",
        "PBWO4","PBWO4_S","BAF2_S","BAF2_L","SCALER"
    };

    void trim(char *s){
        char *p = s;
        while (*p && isspace((unsigned char)*p)) ++p;
        if (p != s) memmove(s, p, strlen(p) + 1);

        size_t len = strlen(s);
        if (len == 0) return;

        char *end = s + len - 1;
        while (end >= s && isspace((unsigned char)*end)) {
            *end = '\0';
            --end;
        }
    }

    char *my_strupr(char *s){
        char *p = s;
        while (*p){
            *p = toupper((unsigned char)*p);
            p++;
        }
        return s;
    }
};

#endif
