#pragma once

#include <Rtypes.h>

class PeakCanvas;
class HistogramBrowser;
class FitController;

class TH1;
class TLine;
class TObject;

class InteractiveCanvas
{
public:
    InteractiveCanvas(HistogramBrowser* browser,
                      FitController* fit);

    void Start();

    void HandleEvent(Int_t event,
                     Int_t px,
                     Int_t py,
                     TObject* selected);

private:

    enum DragMode
    {
        DragNone,
        DragMin,
        DragMax
    };

    //--------------------------
    // drawing
    //--------------------------

    void Draw();
    void UpdateMarkers();
    void Refit();

    //--------------------------
    // histogram navigation
    //--------------------------

    void NextHistogram();
    void PreviousHistogram();

    //--------------------------
    // utilities
    //--------------------------

    double PixelToX(Int_t px) const;
    bool NearMarker(double x,
                    double marker) const;

    //--------------------------
    // application objects
    //--------------------------

    HistogramBrowser* browser;
    FitController* fit;

    PeakCanvas* canvas;
    TH1* hist;

    //--------------------------
    // marker graphics
    //--------------------------

    TLine* xminLine;
    TLine* xmaxLine;

    //--------------------------
    // fit limits
    //--------------------------

    double xmin;
    double xmax;

    //--------------------------
    // interaction state
    //--------------------------

    DragMode dragMode;
    bool mouseDown;
};
