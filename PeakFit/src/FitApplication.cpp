#include <iostream>
#include "FitApplication.h"

#include "AnalysisContext.h"
#include "HistogramBrowser.h"
#include "FitController.h"
#include "InteractiveCanvas.h"

#include <TFile.h>


FitApplication::FitApplication(const std::string& file)
    : fFile(file)
{
    std::cout << "CTOR: file='" << file
              << "' fFile='" << fFile
              << "' this=" << this << '\n';
}


FitApplication::~FitApplication() = default;


void FitApplication::Run()
{
   std::cout << "FitApplication::Run(): fFile = '" << fFile << "'\n";   
   std::cout << "RUN : this=" << this
          << " fFile='" << fFile << "'\n";
		  //
    // Open ROOT file
    //
   fCtx = std::make_unique<AnalysisContext>(fFile);

   if (!fCtx->File()) {
       std::cerr << "Failed to initialize AnalysisContext.\n";
       return;
   }

    //
    // Histogram browser
    //
   fBrowser = std::make_unique<HistogramBrowser>(
        fCtx->File()
    );


    //
    // Fit engine
    //
   fFit = std::make_unique<FitController>();


    //
    // Interactive ROOT canvas
    //
    InteractiveCanvas canvas(
        fBrowser.get(),
        fFit.get()
    );

    canvas.Start();
}
