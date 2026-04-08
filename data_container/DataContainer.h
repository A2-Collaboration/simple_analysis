#pragma once
#include <vector>
#include <algorithm>
#include <cstdio>
#include <stdexcept>

// ======================================================
// Array2D: Channel-major storage (hits contiguous per channel)
// ======================================================
class Array2D {
    std::vector<unsigned int> data;
    size_t hits{}, channels{};
    bool valid{false};

    // Channel-major indexing: hits are contiguous for each channel
    size_t index(size_t hidx, size_t ch) const { 
        return ch * hits + hidx;  
    }

public:
    Array2D() = default;
    Array2D(size_t h, size_t c) { init(h, c); }

    unsigned int get(size_t ch, size_t hidx) const {
        if (!valid || ch >= channels || hidx >= hits) {
            fprintf(stderr, "Warning: Accessing invalid element ch=%zu h=%zu\n", ch, hidx);
            return 0;
        }
        return data[index(hidx, ch)];
    }

    void set(size_t ch, size_t hidx, unsigned int val) {
        if (!valid || ch >= channels || hidx >= hits) {
            fprintf(stderr, "Warning: Setting invalid element ch=%zu h=%zu\n", ch, hidx);
            return;
        }
        data[index(hidx, ch)] = val;
    }

    unsigned int& operator()(size_t ch, size_t hidx) {
        if (!valid || ch >= channels || hidx >= hits)
            throw std::out_of_range("Array2D access");
        return data[index(hidx, ch)];
    }

    const unsigned int& operator()(size_t ch, size_t hidx) const {
        if (!valid || ch >= channels || hidx >= hits)
            throw std::out_of_range("Array2D access");
        return data[index(hidx, ch)];
    }

    size_t get_hits() const { return valid ? hits : 0; }
    size_t get_channels() const { return valid ? channels : 0; }

    void clear() {
        std::fill(data.begin(), data.end(), 0u);  // zero all elements
    }

    void init(size_t c, size_t h) {
        hits = h;
        channels = c;
        data.assign(h * c, 0u);  // allocate and zero
        valid = true;
    }

    bool is_valid() const { return valid; }
};


// ======================================================
// Data: Stores all arrays
// ======================================================
class Data {
public:
    enum ArrayName {
        TAGGER_TDC, TAGGER_SCALER, MWPC_W_TDC, MWPC_S_ADC,
        PID_ADC, PID_TDC, CB_ADC, CB_TDC,
        VETO_ADC, VETO_TDC, BAF2_S_ADC, BAF2_S_TDC, BAF2_L_ADC, BAF2_L_TDC,
        PBWO4_ADC, PBWO4_TDC, PBWO4_S_ADC, PBWO4_S_TDC,
        ARRAY_COUNT
    };

private:
    Array2D arrays[ARRAY_COUNT]{};

public:
    // ---------------- Constructor ----------------
    Data() {
        // Initialize all arrays (replace sizes with actual experiment values)
        arrays[TAGGER_TDC].init(64, 16);
        arrays[TAGGER_SCALER].init(64, 1);
        arrays[MWPC_W_TDC].init(128, 8);
        arrays[MWPC_S_ADC].init(128, 8);
        arrays[PID_ADC].init(32, 16);
        arrays[PID_TDC].init(32, 16);
        arrays[CB_ADC].init(700, 8);
        arrays[CB_TDC].init(720, 2);
        arrays[VETO_ADC].init(64, 8);
        arrays[VETO_TDC].init(64, 8);
        arrays[BAF2_S_ADC].init(32, 16);
        arrays[BAF2_S_TDC].init(32, 16);
        arrays[BAF2_L_ADC].init(32, 16);
        arrays[BAF2_L_TDC].init(32, 16);
        arrays[PBWO4_ADC].init(32, 16);
        arrays[PBWO4_TDC].init(32, 16);
        arrays[PBWO4_S_ADC].init(32, 16);
        arrays[PBWO4_S_TDC].init(32, 16);
    }

    // ---------------- Accessors ----------------
    Array2D& tagger_tdc()      { return arrays[TAGGER_TDC]; }
    Array2D& tagger_scaler()   { return arrays[TAGGER_SCALER]; }
    Array2D& mwpc_w_tdc()      { return arrays[MWPC_W_TDC]; }
    Array2D& mwpc_s_adc()      { return arrays[MWPC_S_ADC]; }
    Array2D& pid_adc()         { return arrays[PID_ADC]; }
    Array2D& pid_tdc()         { return arrays[PID_TDC]; }
    Array2D& cb_adc()          { return arrays[CB_ADC]; }
    Array2D& cb_tdc()          { return arrays[CB_TDC]; }
    Array2D& veto_adc()        { return arrays[VETO_ADC]; }
    Array2D& veto_tdc()        { return arrays[VETO_TDC]; }
    Array2D& baf2_s_adc()      { return arrays[BAF2_S_ADC]; }
    Array2D& baf2_s_tdc()      { return arrays[BAF2_S_TDC]; }
    Array2D& baf2_l_adc()      { return arrays[BAF2_L_ADC]; }
    Array2D& baf2_l_tdc()      { return arrays[BAF2_L_TDC]; }
    Array2D& pbwo4_adc()       { return arrays[PBWO4_ADC]; }
    Array2D& pbwo4_tdc()       { return arrays[PBWO4_TDC]; }
    Array2D& pbwo4_s_adc()     { return arrays[PBWO4_S_ADC]; }
    Array2D& pbwo4_s_tdc()     { return arrays[PBWO4_S_TDC]; }

    const Array2D& tagger_tdc() const      { return arrays[TAGGER_TDC]; }
    const Array2D& tagger_scaler() const   { return arrays[TAGGER_SCALER]; }
    const Array2D& mwpc_w_tdc() const      { return arrays[MWPC_W_TDC]; }
    const Array2D& mwpc_s_adc() const      { return arrays[MWPC_S_ADC]; }
    const Array2D& pid_adc() const         { return arrays[PID_ADC]; }
    const Array2D& pid_tdc() const         { return arrays[PID_TDC]; }
    const Array2D& cb_adc() const          { return arrays[CB_ADC]; }
    const Array2D& cb_tdc() const          { return arrays[CB_TDC]; }
    const Array2D& veto_adc() const        { return arrays[VETO_ADC]; }
    const Array2D& veto_tdc() const        { return arrays[VETO_TDC]; }
    const Array2D& baf2_s_adc() const      { return arrays[BAF2_S_ADC]; }
    const Array2D& baf2_s_tdc() const      { return arrays[BAF2_S_TDC]; }
    const Array2D& baf2_l_adc() const      { return arrays[BAF2_L_ADC]; }
    const Array2D& baf2_l_tdc() const      { return arrays[BAF2_L_TDC]; }
    const Array2D& pbwo4_adc() const       { return arrays[PBWO4_ADC]; }
    const Array2D& pbwo4_tdc() const       { return arrays[PBWO4_TDC]; }
    const Array2D& pbwo4_s_adc() const     { return arrays[PBWO4_S_ADC]; }
    const Array2D& pbwo4_s_tdc() const     { return arrays[PBWO4_S_TDC]; }

    bool is_active(ArrayName name) const {
        return arrays[name].is_valid();
    }

    // ---------------- New event ----------------
    void new_event() {
        // Zero all arrays
        for (size_t i = 0; i < ARRAY_COUNT; ++i) {
            arrays[i].clear();
        }

        // Fill CB_ADC (channel-major access is now cache-friendly)
        for (size_t ch = 0; ch < arrays[CB_ADC].get_channels(); ++ch)
            for (size_t h = 0; h < arrays[CB_ADC].get_hits(); ++h)
                arrays[CB_ADC].set(ch, h, static_cast<unsigned int>(ch  + h));

        // Fill CB_TDC
        for (size_t ch = 0; ch < arrays[CB_TDC].get_channels(); ++ch)
            for (size_t h = 0; h < arrays[CB_TDC].get_hits(); ++h)
                arrays[CB_TDC].set(ch, h, static_cast<unsigned int>(ch *2 + h));
    }
};
