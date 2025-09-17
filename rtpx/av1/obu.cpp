
#include "obu.h"
#include "../util/leb128.h"

#define CHECK_SIZE(sz,need) if ((sz) < (need)) return -1

void obu::decode_aggr_header(uint8_t b, aggr_header_t* hdr)
{
    hdr->z = (b & 0x80) >> 7;
    hdr->y = (b & 0x40) >> 6;
    hdr->w = (b & 0x30) >> 4;
    hdr->n = (b & 0x8) >> 3;
}

uint8_t obu::encode_aggr_header(const aggr_header_t* hdr)
{
    return (hdr->z << 7) | (hdr->y << 6) | (hdr->w << 4) | (hdr->n << 3);
}

int obu::encode(uint8_t* buffer, int offset, int size)
{
    int pos = offset;
    uint32_t len = 0;
    int i=leb128::decode(buffer, size - offset, &len);
    if (i == 0)
        return -1;
    pos += i;

    pos += len;
    CHECK_SIZE(size, pos);

    return pos;
}

int obu::decode(const uint8_t* buffer, int offset, int size)
{
    return 0;
}