#pragma once

#include <memory>
#include <string>

class AnalysisContext;
class HistogramBrowser;
class FitController;

class FitApplication
{
   
 public:
   FitApplication(const std::string& file);
    ~FitApplication();

    void Run();

 private:
   std::string fFile;

   std::unique_ptr<AnalysisContext> fCtx;
   std::unique_ptr<HistogramBrowser> fBrowser;
   std::unique_ptr<FitController> fFit;
};

