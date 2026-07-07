#include "InteractiveCanvas.h"
#include "HistogramBrowser.h"
#include "FitController.h"

#include <TCanvas.h>
#include <TH1.h>
#include <TF1.h>
#include <TLine.h>
#include <TPad.h>
#include <TROOT.h>
#include "PeakCanvas.h"

InteractiveCanvas::InteractiveCanvas(HistogramBrowser* b, FitController* f)
: browser(b), fit(f), canvas(nullptr), hist(nullptr), xminLine(nullptr), xmaxLine(nullptr)
{}

void InteractiveCanvas::Start()
{
    canvas = new PeakCanvas("c","PeakFit V1",900,600, this);

    Draw();
}

void InteractiveCanvas::Draw()
{

    hist = browser->Current();


    if(!hist) {
        std::cout << "No histogram\n";
        return;
    }

    std::cout << hist->GetName() << std::endl;

    canvas->cd();

    hist->GetXaxis()->SetRangeUser(0, 300);
    hist->Draw();

    fit->Fit(hist, 0, 300);

    if (auto f = fit->Function())
    f->Draw("same");

    canvas->Update();
    canvas->Update();
}

