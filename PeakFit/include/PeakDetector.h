#pragma once
#include <vector>

class TH1;

class PeakDetector
{
public:
    std::vector<double> FindPeaks(TH1* h, double threshold = 0.1);
};
