#pragma once
#include <memory>
#include <string>

class TFile;

class AnalysisContext
{
public:
    AnalysisContext(const std::string& file);
    ~AnalysisContext();

    TFile* File();

private:
    class Impl;
    std::unique_ptr<Impl> p;
};
