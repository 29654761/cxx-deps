/**
 * @file h264.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */


#include "h264.h"


void h264::fu_header_set(fu_header_t* fu, uint8_t v)
{
    fu->s = (v & 0x80) >> 7;
    fu->e = (v & 0x40) >> 6;
    fu->r = (v & 0x20) >> 5;
    fu->t = v & 0x1f;
}

uint8_t h264::fu_header_get(const fu_header_t* fu)
{
    uint8_t v = 0;
    v |= fu->s << 7;
    v |= fu->e << 6;
    v |= fu->r << 5;
    v |= fu->t;
    return v;
}

void h264::nal_header_set(nal_header_t* nal, uint8_t v)
{
    nal->f = (v & 0x80) >> 7;
    nal->nri = (v & 0x60) >> 5;
    nal->t = v & 0x1f;
}

uint8_t h264::nal_header_get(const nal_header_t* nal)
{
    uint8_t v = 0;
    v |= nal->f << 7;
    v |= nal->nri << 5;
    v |= nal->t;
    return v;
}



bool h264::find_next_nal(const uint8_t* buffer, size_t size, size_t& offset, size_t& nal_start, size_t& nal_size, bool& islast)
{
    int zeroes = 0;
    // Parse NALs from H264 access unit, encoded as an Annex B bitstream.
    // NALs are delimited by 0x000001 or 0x00000001.
    size_t cur_pos = offset;
    nal_start = 0;
    nal_size = 0;
    islast = false;
    for (size_t i = offset; i < size; i++)
    {
        if (buffer[i] == 0x00)
        {
            zeroes++;
        }
        else if (buffer[i] == 0x01 && zeroes >= 2)
        {
            // This is a NAL start sequence.
            size_t nal_start2 = i + 1;
            if (nal_start > 0)
            {
                size_t end_pos = nal_start2 - (zeroes == 2 ? 3 : 4);
                if (end_pos <= cur_pos)
                {
                    return false;
                }

                nal_size = end_pos - cur_pos;
                islast = (cur_pos + nal_size) == size;
                offset = end_pos;

                
                return true;
            }
            else
            {
                nal_start = nal_start2;
            }

            cur_pos = nal_start;

            zeroes = 0;
        }
        else
        {
            zeroes = 0;
        }
    }

    if (cur_pos < size)
    {
        nal_start = cur_pos;
        if (size <= nal_start)
        {
            return false;
        }
        nal_size = size - nal_start;
        
        offset = size;
        islast = true;
        return true;
    }

    return false;
}


int64_t h264::find_nal_start(const uint8_t* buffer, size_t size)
{
    int zeroes = 0;
    for (size_t i = 0; i < size; i++)
    {
        if (buffer[i] == 0x00)
        {
            zeroes++;
        }
        else if (buffer[i] == 0x01 && zeroes >= 2)
        {
            // This is a NAL start sequence.
            return i + 1;
        }
        else
        {
            zeroes = 0;
        }
    }

    return -1;
}

h264::nal_type_t h264::get_nal_type(uint8_t b)
{
    return (nal_type_t)(uint8_t)(b & 0x1F);
}

bool h264::is_key_frame(uint8_t t)
{
    t = (uint8_t)(t & 0x1F);
    return t == 5 || t == 7 || t == 8;
}

bool h264::is_key_frame(nal_type_t t)
{
    return t == nal_type_t::idr || t == nal_type_t::sps || t == nal_type_t::pps;
}
