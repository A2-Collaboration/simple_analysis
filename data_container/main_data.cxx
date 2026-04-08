#include "DataContainer.h"

int main() {
    Data data;

    for(int evt=0; evt<3; evt++) {
        printf("=== Event %d ===\n", evt);
        data.new_event();

        if(data.is_active(Data::CB_ADC)) {
            for(size_t ch=0; ch<data.cb_adc().get_channels(); ch++)
                for(size_t h=0; h<data.cb_adc().get_hits(); h++)
                    printf("ADC ch %zu hit %zu: %.2i\n", ch,h,data.cb_adc().get(ch,h));
        }

        if(data.is_active(Data::CB_TDC)) {
            for(size_t ch=0; ch<data.cb_tdc().get_channels(); ch++)
                for(size_t h=0; h<data.cb_tdc().get_hits(); h++)
                    printf("TDC ch %zu hit %zu: %.2i\n", ch,h,data.cb_tdc().get(ch,h));
        }


        // Repeat similarly for array4 … array14 if needed
    }
}
