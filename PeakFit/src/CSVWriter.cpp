#include "CSVWriter.h"
#include <fstream>
#include <TH1.h>

CSVWriter::CSVWriter(const std::string& file)
: fname(file) {}

void CSVWriter::Write(const std::string& hist, TH1*, double* p, int)
{
    std::ofstream f(fname, std::ios::app);
    if(!f) return;

    f << hist;
    for(int i=0;i<5;i++) f << "," << p[i];
    f << "\n";
}
