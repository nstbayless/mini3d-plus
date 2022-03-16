#ifndef scanline_h
#define scanline_h

#include "mini3d.h"

#if ENABLE_POLYGON_SCANLINING
typedef struct
{
    enum
    {
        kScanlineOdd = 0,
        kScanlineEven = 1,
        kScanlineAll = 2,
    } select;
    
    // fill pattern. Typical values are:
    // 0x00000000 (black), 0xFFFFFFFF (white), and 0xAAAAAAAA (dithered grey)
    uint32_t fill;
} ScanlineFill;
#endif

#endif