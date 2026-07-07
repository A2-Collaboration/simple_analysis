#include "PeakCanvas.h"
#include "InteractiveCanvas.h"

PeakCanvas::PeakCanvas(const char* name,
                       const char* title,
                       Int_t w,
                       Int_t h,
                       InteractiveCanvas* o)
    : TCanvas(name, title, w, h),
      owner(o)
{
}

void PeakCanvas::HandleInput(EEventType event,
                             Int_t px,
                             Int_t py)
{
    TCanvas::HandleInput(event, px, py);

    if (owner)
        owner->HandleEvent(event, px, py, nullptr);
}
