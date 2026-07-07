#pragma once

class TH1;
class TF1;

class FitController
{
public:
    FitController();

    void Fit(TH1* h, double xmin, double xmax);
    TF1* Function();

    double FindSecondPeak(TH1* h);

private:
    TF1* f;
};
