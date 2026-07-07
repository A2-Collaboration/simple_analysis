#include "CSVWriter.h"
#include <fstream>
#include <TH1.h>
#include <TF1.h>

void ExportFullReport(const std::string& file, TH1* h, TF1* f)
{
    std::ofstream out(file);
    if(!out) return;

    out << "# PeakFitScience Report\n";
    out << "Histogram," << h->GetName() << "\n";
    out << "Mean," << h->GetMean() << "\n";
    out << "RMS," << h->GetRMS() << "\n";

    out << "FitParams\n";
    for(int i=0;i<5;i++)
        out << i << "," << f->GetParameter(i) << "\n";
}
