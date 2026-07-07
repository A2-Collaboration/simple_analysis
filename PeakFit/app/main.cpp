#include "FitApplication.h"
#include <TApplication.h>
#include <iostream>
#include <string>

int main(int argc, char** argv)
{
    std::string filename;

    if (argc > 1)
        filename = argv[1];

    TApplication app("PeakFit", &argc, argv);

    FitApplication appFit(filename);

    appFit.Run();
    app.Run();

    return 0;
}
