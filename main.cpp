#include <iostream>
#include "param.h"

Config config={
    // SSDParam
    {
        // GcParam
        {
            GC_MODE::GC_MODE_COST_BENEFIT, // mode
            0.8,                           // gc_threshold_high
            0.2                            // gc_threshold_low
        },
        // SlcCacheParam
        {
            SLC_CACHE_MODE::SLC_CACHE_MODE_DYNAMIC, // mode
            128,                                    // slc_static_size in blocks
            0.1                                     // slc_dynamic_ratio in percentage
        },
        MAPPING_MODE::MAPPING_MODE_PAGE_LEVEL, // mapping_mode
        4,                               // ChannelNum
        2                                // ChipPerChannel
    },
    // NandParam
    {
        1,      // DiePerChip
        1,      // PlanePerDie
        64,     // BlockPerPlane
        8,      // PagePerBlock
        16384,  // PageSize
        2208    // SpareSize
    }
};

int main()
{
    return 0;
}