
#include "obu.h"
#include "../util/leb128.h"
#include "../util/bit.h"

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

int obu::decode_header(const uint8_t* buffer, int offset, int size, header_t* hdr)
{
    int pos = offset;
    CHECK_SIZE(size, pos + 2);
    uint32_t v = (buffer[0] << 24) | (buffer[1]<<16);
    hdr->forbidden_bit = bit::read_bits_32(v, 0, 1);
    hdr->type= bit::read_bits_32(v, 1, 4);
    hdr->extension_flag= bit::read_bits_32(v, 5, 1);
    hdr->has_size_field= bit::read_bits_32(v, 6, 1);
    hdr->reserved_1bit= bit::read_bits_32(v, 7, 1);
    pos++;
    if (hdr->extension_flag)
    {
        hdr->temporal_id= bit::read_bits_32(v, 8, 3);
        hdr->spatial_id= bit::read_bits_32(v, 11, 2);
        hdr->extension_header_reserved_3bits = bit::read_bits_32(v, 13, 3);
        pos++;
    }

    if (hdr->has_size_field) 
    {
        size_t r = leb128::decode(buffer + pos, size - pos, &hdr->obu_size);
        pos += r;
    }
    else
    {
        hdr->obu_size = size - pos;
    }

    return pos;
}

int obu::encode_header(const header_t* hdr, uint8_t* buffer, int offset, int size)
{
    return 0;
}

void obu::find_elements(const uint8_t* buffer, size_t size, uint8_t w, std::vector<element_t>& elements)
{
    elements.clear();
    if (w > 0)
    {
        int pos = 0;
        for (uint8_t i = 0; i < w; i++)
        {
            if (i != w - 1)
            {
                uint32_t len = 0;
                int r = leb128::decode(buffer + pos, size, &len);
                if (r == 0)
                    break;
                pos += r;
                if (pos + len > size)
                    break;

                element_t elem = {};
                elem.data = (uint8_t*)buffer + pos;
                elem.size = len;
                elements.push_back(elem);
                pos += len;
            }
            else
            {
                element_t elem = {};
                elem.data = (uint8_t*)buffer + pos;
                elem.size = size-pos;
                elements.push_back(elem);

                pos += elem.size;
            }
        }
    }
    else
    {
        int pos = 0;
        for (;;)
        {
            uint32_t len = 0;
            int r = leb128::decode(buffer + pos, size, &len);
            if (r == 0)
                break;

            pos += r;

            if (pos + len > size)
                break;


            element_t elem = {};
            elem.data = (uint8_t*)buffer + pos;
            elem.size = len;
            elements.push_back(elem);
            pos += len;
        }
    }

}


