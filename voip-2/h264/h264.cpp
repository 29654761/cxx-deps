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



bool h264::find_next_nal(const uint8_t* buffer, int size, int& offset, int& nal_start, int& nal_size, bool& islast)
{
    int zeroes = 0;
    // Parse NALs from H264 access unit, encoded as an Annex B bitstream.
    // NALs are delimited by 0x000001 or 0x00000001.
    int cur_pos = offset;
    nal_start = 0;
    nal_size = 0;
    islast = false;
    for (int i = offset; i < size; i++)
    {
        if (buffer[i] == 0x00)
        {
            zeroes++;
        }
        else if (buffer[i] == 0x01 && zeroes >= 2)
        {
            // This is a NAL start sequence.
            int nal_start2 = i + 1;
            if (nal_start > 0)
            {
                int end_pos = nal_start2 - (zeroes == 2 ? 3 : 4);
                nal_size = end_pos - cur_pos;
                islast = (cur_pos + nal_size) == size;
                offset = end_pos;

                if (nal_size <= 0)
                {
                    return false;
                }
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
        nal_size = size - nal_start;
        if (nal_size <= 0)
        {
            return false;
        }
        offset = size;
        islast = true;
        return true;
    }

    return false;
}


int h264::find_nal_start(const uint8_t* buffer, int size)
{
    int zeroes = 0;
    for (int i = 0; i < size; i++)
    {
        if (buffer[i] == 0x00)
        {
            zeroes++;
        }
        else if (buffer[i] == 0x01 && zeroes >= 2)
        {
            // This is a NAL start sequence.
            return i + 1;

            zeroes = 0;
        }
        else
        {
            zeroes = 0;
        }
    }

    return -1;
}

uint8_t h264::get_nal_type(uint8_t b)
{
    return (uint8_t)(b & 0x1F);
}

bool h264::is_key_frame(uint8_t t)
{
    t = (uint8_t)(t & 0x1F);
    return t == 5 || t == 7 || t == 8;
}
