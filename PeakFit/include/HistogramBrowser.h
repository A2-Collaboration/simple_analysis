#pragma once
#include <vector>
#include <string>

class TFile;
class TH1;

class HistogramBrowser
{
public:
    HistogramBrowser(TFile* f);

    TH1* Current();
    bool Next();
    bool Prev();

    std::string Name();

private:
    std::vector<std::string> names;
    int idx = 0;
    TFile* file;
};
