// ============================================================================
// Show_data_both.C
//
// Linux-only version.
//
// Usage:
//   root -l
//   .x Show_data_both.C("file1.root","file2.root")
//
// Controls:
//   ENTER       next Y-bin
//   p ENTER     previous Y-bin
//   q ENTER     quit
//
// Every time ENTER or p ENTER is used, the new channel starts unzoomed.
//
// Controls work both:
//   - in the ROOT terminal
//   - when the canvas window has focus
// ============================================================================

#include <TFile.h>
#include <TH2D.h>
#include <TH1D.h>
#include <TCanvas.h>
#include <TLegend.h>
#include <TSystem.h>
#include <TString.h>

#include <iostream>
#include <string>

#include <sys/select.h>
#include <unistd.h>


// ============================================================================
// Check whether a complete line is waiting on stdin
// ============================================================================

bool InputAvailable()
{
    fd_set set;

    FD_ZERO(&set);
    FD_SET(STDIN_FILENO, &set);

    struct timeval timeout;

    timeout.tv_sec  = 0;
    timeout.tv_usec = 0;

    return select(
        STDIN_FILENO + 1,
        &set,
        nullptr,
        nullptr,
        &timeout
    ) > 0;
}


// ============================================================================
// Wait for keyboard input while keeping ROOT responsive
//
// Input can come from:
//
//   1. ROOT terminal
//   2. Canvas window
//
// Canvas keys:
//   ENTER -> next
//   p     -> previous
//   q     -> quit
// ============================================================================

bool WaitForCommand(
    std::string &command,
    TCanvas *canvas)
{
    command.clear();

    while (true) {

        // ------------------------------------------------------------
        // Keep ROOT GUI alive
        // ------------------------------------------------------------

        gSystem->ProcessEvents();


        // ------------------------------------------------------------
        // Check for keyboard input in the CANVAS
        // ------------------------------------------------------------

        if (canvas) {

            if (canvas->GetEvent() == kKeyPress) {

                const int key = canvas->GetEventX();


                // ----------------------------------------------------
                // ENTER
                // ----------------------------------------------------

                if (key == '\r' ||
                    key == '\n') {

                    command.clear();

                    canvas->HandleInput(
                        kMouseMotion,
                        canvas->GetWw() / 2,
                        canvas->GetWh() / 2
                    );

                    return true;
                }


                // ----------------------------------------------------
                // p = previous
                // ----------------------------------------------------

                if (key == 'p' ||
                    key == 'P') {

                    command = "p";

                    canvas->HandleInput(
                        kMouseMotion,
                        canvas->GetWw() / 2,
                        canvas->GetWh() / 2
                    );

                    return true;
                }


                // ----------------------------------------------------
                // q = quit
                // ----------------------------------------------------

                if (key == 'q' ||
                    key == 'Q') {

                    command = "q";

                    canvas->HandleInput(
                        kMouseMotion,
                        canvas->GetWw() / 2,
                        canvas->GetWh() / 2
                    );

                    return true;
                }


                // ----------------------------------------------------
                // Other canvas key:
                // consume it and continue waiting.
                // ----------------------------------------------------

                canvas->HandleInput(
                    kMouseMotion,
                    canvas->GetWw() / 2,
                    canvas->GetWh() / 2
                );
            }
        }


        // ------------------------------------------------------------
        // Check terminal
        // ------------------------------------------------------------

        if (InputAvailable()) {

            if (!std::getline(std::cin, command)) {

                // EOF / terminal closed
                return false;
            }

            return true;
        }


        // ------------------------------------------------------------
        // Small sleep so we don't burn CPU
        // ------------------------------------------------------------

        gSystem->Sleep(10);
    }
}


// ============================================================================
// Main
// ============================================================================

void Show_data_both(
    const char *filename1,
    const char *filename2)
{
    // ========================================================================
    // Open first ROOT file
    // ========================================================================

    TFile *file1 = TFile::Open(
        filename1,
        "READ"
    );

    if (!file1 || file1->IsZombie()) {

        std::cerr
            << "ERROR: Cannot open first file: "
            << filename1
            << std::endl;

        if (file1)
            delete file1;

        return;
    }


    // ========================================================================
    // Get 2D histogram
    // ========================================================================

    TH2D *h2 = nullptr;

    file1->GetObject(
        "CB_SourceCalib/HitsADC_Cluster",
        h2
    );

    if (!h2) {

        std::cerr
            << "ERROR: Cannot find "
            << "CB_SourceCalib/HitsADC_Cluster"
            << " in first file."
            << std::endl;

        file1->Close();
        delete file1;

        return;
    }


    const int nY = h2->GetNbinsY();


    // ========================================================================
    // Open second ROOT file
    // ========================================================================

    TFile *file2 = TFile::Open(
        filename2,
        "READ"
    );

    if (!file2 || file2->IsZombie()) {

        std::cerr
            << "ERROR: Cannot open second file: "
            << filename2
            << std::endl;

        if (file2)
            delete file2;

        file1->Close();
        delete file1;

        return;
    }


    // ========================================================================
    // Canvas
    // ========================================================================

    TCanvas *canvas = new TCanvas(
        "Show_data_both_canvas",
        "Show_data_both",
        900,
        700
    );


    // ========================================================================
    // Information
    // ========================================================================

    std::cout << std::endl;

    std::cout
        << "========================================"
        << std::endl;

    std::cout
        << " Show_data_both"
        << std::endl;

    std::cout
        << "========================================"
        << std::endl;

    std::cout
        << "File 1: "
        << filename1
        << std::endl;

    std::cout
        << "File 2: "
        << filename2
        << std::endl;

    std::cout << std::endl;

    std::cout
        << "Controls:"
        << std::endl;

    std::cout
        << "  ENTER       next Y-bin"
        << std::endl;

    std::cout
        << "  p ENTER     previous Y-bin"
        << std::endl;

    std::cout
        << "  q ENTER     quit"
        << std::endl;

    std::cout
        << "========================================"
        << std::endl;


    // ========================================================================
    // Start at first Y-bin
    // ========================================================================

    int ybin = 1;


    // ========================================================================
    // Main loop
    // ========================================================================

    while (true) {

        // ====================================================================
        // Clear canvas
        // ====================================================================

        canvas->cd();
        canvas->Clear();


        // ====================================================================
        // Histogram 1
        //
        // Projection of current Y-bin onto X.
        //
        // A completely new histogram is created for every Y-bin.
        // ====================================================================

        TH1D *h1 = h2->ProjectionX(
            Form(
                "Show_data_both_projection_%d",
                ybin
            ),
            ybin,
            ybin
        );

        if (!h1) {

            std::cerr
                << "ERROR: Projection failed."
                << std::endl;

            break;
        }

        // Keep projection independent of ROOT directories.
        h1->SetDirectory(nullptr);


        // ====================================================================
        // Explicitly reset axis ranges
        //
        // This guarantees that a zoom from the previous channel cannot
        // influence this channel.
        // ====================================================================

        h1->GetXaxis()->SetRange(0, 0);
        h1->GetYaxis()->SetRange(0, 0);


        // ====================================================================
        // Histogram 1 appearance
        // ====================================================================

        h1->SetLineColor(kBlue);
        h1->SetLineWidth(2);

        h1->SetTitle(
            Form(
                "Y-bin %d / %d",
                ybin,
                nY
            )
        );

        h1->GetXaxis()->SetTitle("ADC");
        h1->GetYaxis()->SetTitle("Counts");


        // ====================================================================
        // Histogram 2
        //
        // Y-bin 1 -> CB_single_ADC_000
        // Y-bin 2 -> CB_single_ADC_001
        // etc.
        // ====================================================================

        const int adcNumber = ybin - 1;

        TString histName = Form(
            "cb_single/CB_single_ADC_%03d",
            adcNumber
        );

        TH1D *h2_single = nullptr;

        file2->GetObject(
            histName.Data(),
            h2_single
        );


        // ====================================================================
        // Histogram 2 appearance
        // ====================================================================

        if (h2_single) {

            // ------------------------------------------------------------
            // Reset any range left on the histogram object.
            //
            // This is especially important because the histogram is
            // obtained directly from file2.
            // ------------------------------------------------------------

            h2_single->GetXaxis()->SetRange(0, 0);
            h2_single->GetYaxis()->SetRange(0, 0);

            h2_single->SetLineColor(kRed);
            h2_single->SetLineWidth(2);

        }
        else {

            std::cerr
                << std::endl
                << "WARNING: Cannot find "
                << histName
                << " in second file."
                << std::endl;
        }


        // ====================================================================
        // Y-axis range
        //
        // Use the larger maximum of the two histograms.
        // ====================================================================

        double max1 = h1->GetMaximum();

        double max2 = 0.0;

        if (h2_single)
            max2 = h2_single->GetMaximum();

        double maxY = (max1 > max2)
                    ? max1
                    : max2;

        if (maxY > 0.0)
            maxY *= 1.10;

        h1->SetMinimum(0.0);
        h1->SetMaximum(maxY);


        // ====================================================================
        // Draw
        // ====================================================================

        h1->Draw("HIST");

        if (h2_single)
            h2_single->Draw("HIST SAME");


        // ====================================================================
        // Legend
        // ====================================================================

        TLegend *legend = new TLegend(
            0.35,
            0.78,
            0.75,
            0.87
        );

        legend->SetBorderSize(0);
        legend->SetFillStyle(0);
        legend->SetTextSize(0.030);

        legend->AddEntry(
            h1,
            "HitsADC_Cluster projection",
            "l"
        );

        if (h2_single) {

            legend->AddEntry(
                h2_single,
                histName.Data(),
                "l"
            );
        }

        legend->Draw();


        // ====================================================================
        // Update canvas
        // ====================================================================

        canvas->Modified();
        canvas->Update();


        // ====================================================================
        // Make canvas ready for keyboard input
        // ====================================================================

        canvas->HandleInput(
            kMouseMotion,
            canvas->GetWw() / 2,
            canvas->GetWh() / 2
        );


        // ====================================================================
        // Print information
        // ====================================================================

        std::cout << std::endl;

        std::cout
            << "----------------------------------------"
            << std::endl;

        std::cout
            << "Y-bin "
            << ybin
            << " / "
            << nY
            << std::endl;

        std::cout
            << "Y range: "
            << h2->GetYaxis()->GetBinLowEdge(ybin)
            << " - "
            << h2->GetYaxis()->GetBinUpEdge(ybin)
            << std::endl;

        std::cout
            << "Second histogram: "
            << histName
            << std::endl;

        std::cout << std::endl;

        std::cout
            << "ENTER = next   "
            << "p + ENTER = previous   "
            << "q + ENTER = quit"
            << std::endl;


        // ====================================================================
        // Wait for command
        // ====================================================================

        std::string command;

        bool gotInput = WaitForCommand(
            command,
            canvas
        );


        // ====================================================================
        // Delete temporary objects BEFORE changing bin
        // ====================================================================

        delete legend;
        delete h1;


        // ====================================================================
        // EOF / terminal problem
        // ====================================================================

        if (!gotInput) {

            std::cout
                << std::endl
                << "Input closed. Stopping."
                << std::endl;

            break;
        }


        // ====================================================================
        // Quit
        // ====================================================================

        if (command == "q" ||
            command == "Q") {

            std::cout
                << std::endl
                << "Stopped by user."
                << std::endl;

            break;
        }


        // ====================================================================
        // Previous
        // ====================================================================

        if (command == "p" ||
            command == "P") {

            if (ybin > 1) {

                --ybin;

            }
            else {

                std::cout
                    << "Already at first Y-bin."
                    << std::endl;
            }

            continue;
        }


        // ====================================================================
        // Anything else, including an empty line = next
        // ====================================================================

        if (ybin < nY) {

            ++ybin;

        }
        else {

            std::cout
                << "Already at last Y-bin."
                << std::endl;
        }
    }


    // ========================================================================
    // Cleanup
    // ========================================================================

    file2->Close();
    delete file2;

    file1->Close();
    delete file1;

    delete canvas;


    std::cout
        << std::endl
        << "Finished."
        << std::endl;
}