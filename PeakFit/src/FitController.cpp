#include "FitController.h"
#include "PeakDetector.h"

#include <TH1.h>
#include <TF1.h>


FitController::FitController()
{
   
    f = new TF1(
        "f",
        "[0]*exp(-0.5*((x-[1])/[2])^2) + exp([3]+[4]*x)",
        0,
        1
    );
}



void FitController::Fit(TH1* h, double xmin, double xmax)
{
   
    if(!h)
        return;


    if(xmin >= xmax)
	 {
		
        xmin = h->GetXaxis()->GetXmin();
        xmax = h->GetXaxis()->GetXmax();
	 }
   


    PeakDetector detector;

    auto peaks = detector.FindPeaks(h, 0.2);


    double a  = h->GetMaximum();
    double x0 = h->GetMean();


    if(!peaks.empty())
        x0 = peaks[0];


    f->SetParameters(
        a,
        x0,
        3,
        1,
        -0.01
    );


    h->Fit(
        f,
        "RQ0",
        "",
        xmin,
        xmax
    );
	f->SetRange(xmin, xmax);

}



TF1* FitController::Function()
{
   
    return f;
}

