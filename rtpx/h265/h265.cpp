/**
 * @file h265.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */


#include "h265.h"
#include <stdlib.h>
#include <string.h>

void h265::fu_header_set(fu_header_t* fu, uint8_t v)
{
    fu->s = (v & 0x80) >> 7;
    fu->e = (v & 0x40) >> 6;
    fu->r = (v & 0x20) >> 5;
    fu->t = v & 0x1f;
}

uint8_t h265::fu_header_get(const fu_header_t* fu)
{
    uint8_t v = 0;
    v |= fu->s << 7;
    v |= fu->e << 6;
    v |= fu->r << 5;
    v |= fu->t;
    return v;
}

size_t h265::nal_header_set(nal_header_t* nal, const uint8_t* v, size_t s)
{
    if (s < 2)
        return 0;
    nal->f= (v[0] & 0x80) >> 7;
    nal->type = (v[0] & 0x7e) >> 1;
    nal->layer = (v[0] & 0x01) | (v[1] & 0xf8);
    nal->tid = v[1] & 0x07;
    return 2;
}

size_t h265::nal_header_get(const nal_header_t* nal, uint8_t* v, size_t s)
{
    if (s < 2)
        return 0;
    
    uint8_t b0 = 0, b1 = 0;
    b0 |= nal->f << 7;
    b0 |= nal->tid << 1;
    b0 |= (nal->layer & 0x20) >> 5;
    b1 |= (nal->layer & 0x1f) << 3;
    b1 |= nal->tid;
    v[0] = b0;
    v[1] = b1;
    return 2;
}



bool h265::find_next_nal(const uint8_t* buffer, size_t size, size_t& offset, size_t& nal_start, size_t& nal_size, bool& islast)
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

                if (nal_size < 2)
                    return false;
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

        if (nal_size < 2)
            return false;
        return true;
    }

    return false;
}


int64_t h265::find_nal_start(const uint8_t* buffer, size_t size)
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

h265::nal_type_t h265::get_nal_type(const uint8_t* b, size_t s)
{
    nal_header_t hd = {};
    if (nal_header_set(&hd, b, s) < 2)
        return nal_type_t::reserved;

    return (h265::nal_type_t)hd.type;
}

bool h265::is_key_frame(const uint8_t* b, size_t s)
{
    nal_type_t t=get_nal_type(b, s);
    return t == nal_type_t::idr || t == nal_type_t::pps || t == nal_type_t::sps;
}

bool h265::is_key_frame(nal_type_t t)
{
    return t == nal_type_t::idr || t == nal_type_t::pps || t == nal_type_t::sps;
}

size_t h265::remove_emulation_prevention(uint8_t* nal, size_t size)
{
    uint8_t* nal_copy = (uint8_t*)malloc(size);
    if (!nal_copy)
        return size;

    memcpy(nal_copy, nal, size);
    size_t off = 0;
    for (size_t i = 0; i < size;)
    {
        if (i + 2 < size && nal_copy[i] == 0x00 && nal_copy[i + 1] == 0x00 && nal_copy[i + 2] == 0x03)
        {
            nal[off++] = 0x00;
            nal[off++] = 0x00;
            i += 3;
        }
        else
        {
            nal[off++] = nal_copy[i++];
        }
    }

    free(nal_copy);
    return off;
}