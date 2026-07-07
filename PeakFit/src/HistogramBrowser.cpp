#include "HistogramBrowser.h"

#include <TFile.h>
#include <TDirectory.h>
#include <TH1.h>
#include <TKey.h>

#include <algorithm>


static const char* HISTOGRAM_DIRECTORY = "cb_single";
static const char* HISTOGRAM_PREFIX    = "CB_single_ADC_";


HistogramBrowser::HistogramBrowser(TFile* f)
: file(f)
{
   
    if(!file)
        return;

    auto dir = (TDirectory*)file->Get(HISTOGRAM_DIRECTORY);

    if(!dir)
        return;


    TIter it(dir->GetListOfKeys());
    TKey* k;

    while((k=(TKey*)it()))
	 {
		
		std::string name = k->GetName();

        if(name.find(HISTOGRAM_PREFIX) == 0)
            names.push_back(name);
	 }
   


   std::sort(names.begin(), names.end());
}



TH1* HistogramBrowser::Current()
{
   
    if(names.empty())
        return nullptr;


    auto dir = (TDirectory*)file->Get(HISTOGRAM_DIRECTORY);

    if(!dir)
        return nullptr;


    TH1* h = nullptr;

    dir->GetObject(names[idx].c_str(), h);

    if(h)
        h->SetDirectory(nullptr);


    return h;
}



bool HistogramBrowser::Next()
{
   
    if(idx >= (int)names.size()-1)
        return false;

    idx++;
    return true;
}



bool HistogramBrowser::Prev()
{
   
    if(idx <= 0)
        return false;

    idx--;
    return true;
}



std::string HistogramBrowser::Name()
{
   
   return names.empty() ? "" : names[idx];
}
