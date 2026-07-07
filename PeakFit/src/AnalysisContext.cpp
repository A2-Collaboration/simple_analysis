#include "AnalysisContext.h"
#include <TFile.h>
#include <TSystem.h>

#include <iostream>

class AnalysisContext::Impl
{
public:
    std::unique_ptr<TFile> file;
};

AnalysisContext::AnalysisContext(const std::string& file)
: p(std::make_unique<Impl>())
{
    std::cout << "Opening file: '" << file << "'" << std::endl;

    p->file.reset(TFile::Open(file.c_str()));

    if (!p->file) {
        std::cout << "TFile::Open returned nullptr" << std::endl;
        return;
    }

    if (p->file->IsZombie()) {
        std::cout << "File is a zombie" << std::endl;
        return;
    }

    std::cout << "File opened successfully" << std::endl;

    gSystem->Exec("mkdir -p results");
}

AnalysisContext::~AnalysisContext() = default;

TFile* AnalysisContext::File()
{
    return p->file.get();
}