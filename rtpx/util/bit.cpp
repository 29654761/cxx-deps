
#include "bit.h"




uint32_t bit::make_mask_32(uint8_t off, uint8_t bits)
{
    uint32_t v = 1, msk = 0;
    if (off + bits > 32)
        return 0;
    for (uint8_t i = 0; i < bits; i++)
    {
        msk |= (v << (31 - off++));
    }
    return msk;
}

uint32_t bit::read_bits_32(uint32_t v, uint8_t off, uint8_t bits)
{
    uint32_t msk = make_mask_32(off, bits);
    if (msk == 0)
        return 0;
    return (v & msk) >> (32 - (off + bits));
}

