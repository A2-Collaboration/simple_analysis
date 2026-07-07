#pragma once
#include <string>

class TH1;

class CSVWriter
{
public:
    CSVWriter(const std::string& file);

    void Write(const std::string& hist, TH1* h, double* params, int n);

private:
    std::string fname;
};
