#pragma once

#include <TCanvas.h>

class InteractiveCanvas;

class PeakCanvas : public TCanvas
{
public:
    PeakCanvas(const char* name,
               const char* title,
               Int_t w,
               Int_t h,
               InteractiveCanvas* owner);

protected:
    void HandleInput(EEventType event,
                     Int_t px,
                     Int_t py) override;

private:
    InteractiveCanvas* owner;
};
