#include "PeakDetector.h"
#include <TH1.h>

std::vector<double> PeakDetector::FindPeaks(TH1* h, double thr)
{
    std::vector<double> peaks;
    if(!h) return peaks;

    int n = h->GetNbinsX();

    for(int i=2;i<n;i++)
    {
        double p0 = h->GetBinContent(i-1);
        double p1 = h->GetBinContent(i);
        double p2 = h->GetBinContent(i+1);

        if(p1 > p0 && p1 > p2 && p1 > thr * h->GetMaximum())
        {
            peaks.push_back(h->GetXaxis()->GetBinCenter(i));
        }
    }

    return peaks;
}
