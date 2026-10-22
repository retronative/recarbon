// ReCarbon runtime (based on SpeedBreaker). GPL-3.0-or-later (see COPYING).
//
// What an EDRAM colour render target's format means for its storage, as
// Xenia defines it (src/xenia/gpu/xenos.h at 95a5c3ee, BSD-3-Clause):
//   - GetStorageColorFormat: k_2_10_10_10_AS_10_10_10_10 (10) and
//     k_2_10_10_10_FLOAT_AS_16_16_16_16 (12) have "the same packing and
//     meaning of values" as k_2_10_10_10 (2) and k_2_10_10_10_FLOAT (3); they
//     differ only in blending precision. Xenia's render target cache keys
//     its host render targets by that storage format, so a title that draws
//     into EDRAM as one and then as the other works on the same pixels.
//   - IsColorRenderTargetFormat64bpp: only 16_16_16_16, 16_16_16_16_FLOAT
//     and 32_32_FLOAT take 64 bits a sample; the _AS_16_16_16_16 one is 32.
//
// SpeedBreaker keyed host render targets by the raw format and counted 12 as
// 64 bpp. NFS: Carbon's movie player draws each frame into EDRAM base 0 as
// format 3, then blends its overlay on it as format 12 and resolves that to
// the front buffer: format 12 was a separate (never written) host image, so
// every movie frame came out black (ROADMAP M4).
#pragma once
#include <cstdint>

namespace gpu
{
    // Xenos ColorRenderTargetFormat values.
    inline constexpr uint32_t kColor_2_10_10_10 = 2;
    inline constexpr uint32_t kColor_2_10_10_10_Float = 3;
    inline constexpr uint32_t kColor_16_16_16_16 = 5;
    inline constexpr uint32_t kColor_16_16_16_16_Float = 7;
    inline constexpr uint32_t kColor_2_10_10_10_As_10_10_10_10 = 10;
    inline constexpr uint32_t kColor_2_10_10_10_Float_As_16_16_16_16 = 12;
    inline constexpr uint32_t kColor_32_32_Float = 15;

    // The format whose host render target holds this one's pixels.
    constexpr uint32_t StorageColorFormat(uint32_t format)
    {
        switch (format)
        {
        case kColor_2_10_10_10_As_10_10_10_10: return kColor_2_10_10_10;
        case kColor_2_10_10_10_Float_As_16_16_16_16: return kColor_2_10_10_10_Float;
        default: return format;
        }
    }

    constexpr bool IsColorFormat64bpp(uint32_t format)
    {
        return format == kColor_16_16_16_16 || format == kColor_16_16_16_16_Float || format == kColor_32_32_Float;
    }
}
