/**
 * @file vp9_header.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */

#include "vp9_header.h"
#include "../util/bit.h"
#include <algorithm>

#define CHECK_SIZE(sz,need) if ((sz) < (need)) return -1

vp9_scalability_pg::vp9_scalability_pg()
{
    tid = 0;
    u = 0;
    p_diff;
}

vp9_scalability_pg::~vp9_scalability_pg()
{
}

int vp9_scalability_pg::serialize(uint8_t* buffer, int offset, int size)const
{
    int pos = offset;
    uint8_t r = (uint8_t)p_diff.size();
    r = (r & 0x03);
    CHECK_SIZE(size,pos + r + 1);

    uint8_t v = ((tid&0x07) << 5) | ((u&0x01) << 4) | (r << 2);
    buffer[pos++] = v;
    for (uint8_t i = 0; i < r; i++)
    {
        buffer[pos++] = p_diff[i];
    }

    return pos;
}

int vp9_scalability_pg::deserialize(const uint8_t* buffer, int offset, int size)
{
    int pos = offset;
    CHECK_SIZE(size,pos + 1);

    uint8_t v = buffer[pos++];
    tid = (v & 0xE0) >> 5;
    u = (v & 0x10) >> 4;
    uint8_t r= (v & 0x0C) >> 2;
    CHECK_SIZE(size, pos+r);
    p_diff.clear();
    for (uint8_t i = 0; i < r; i++)
    {
        p_diff.push_back(buffer[pos++]);
    }
    return pos;
}


//=================================================================

vp9_scalability_header::vp9_scalability_header()
{
    ns = 0;
    y = 0;
    g = 0;
}

vp9_scalability_header::~vp9_scalability_header()
{
}

int vp9_scalability_header::serialize(uint8_t* buffer, int offset, int size)const
{
    int pos = offset;
    CHECK_SIZE(size, pos + 1);

    uint8_t v = (this->ns << 5) | (this->y << 4) | (this->g << 3);
    buffer[pos++] = v;
    
    for (int i = 0; i < this->ns + 1; i++)
    {
        if (this->y)
        {
            CHECK_SIZE(size, pos + 4);
            buffer[pos++] = (this->res[i].width & 0xFF00) >> 8;
            buffer[pos++] = (this->res[i].width & 0x00FF);
            buffer[pos++] = (this->res[i].height & 0xFF00) >> 8;
            buffer[pos++] = (this->res[i].height & 0x00FF);
        }
    }

    if (this->g)
    {
        uint8_t ng = (uint8_t)this->pg.size();
        for (uint8_t i = 0; i < ng; i++)
        {
            pos = this->pg[i].serialize(buffer, pos, size);
            if (pos < 0)
                return pos;
        }
    }
    return pos;
}

int vp9_scalability_header::deserialize(const uint8_t* buffer, int offset, int size)
{
    int pos = offset;
    CHECK_SIZE(size, pos + 1);
    uint8_t v = buffer[pos++];
    this->ns = (v & 0xE0) >> 5;
    this->y = (v & 0x10) >> 4;
    this->g = (v & 0x08) >> 3;

    this->res.clear();
    for (int i = 0; i < this->ns + 1; i++)
    {
        if (this->y)
        {
            CHECK_SIZE(size, pos + 4);
            vp9_resolution res = {};
            res.width = (buffer[pos] << 8 | (buffer[pos + 1]));
            res.height = (buffer[pos + 2] << 8 | (buffer[pos + 3]));
            this->res.push_back(res);
            pos += 4;
        }
    }
    if (this->g)
    {
        CHECK_SIZE(size, pos + 1);
        uint8_t ng = buffer[pos++];
        for (uint8_t i = 0; i < ng; i++)
        {
            vp9_scalability_pg pg;
            pos = pg.deserialize(buffer, pos, size);
            if (pos < 0)
                return pos;
        }
    }
    return pos;
}

//=================================================================

vp9_header::vp9_header()
{
    i = 0;
    p = 0;
    l = 0;
    f = 0;
    b = 0;
    e = 0;
    v = 0;
    z = 0;

    pid_m = 0;
    pid = 0;

    tid = 0;
    up_switch = 0;
    sid = 0;
    spatial_d = 0;

    tl0_pic_idx = 0;
}

vp9_header::~vp9_header()
{
}

int vp9_header::serialize(uint8_t* buffer, int offset, int size)const
{
    int pos = offset;
    CHECK_SIZE(size, 1);
    uint8_t v = 0;
    v |= (this->i & 0x01) << 7;
    v |= (this->p & 0x01) << 6;
    v |= (this->l & 0x01) << 5;
    v |= (this->f & 0x01) << 4;
    v |= (this->b & 0x01) << 3;
    v |= (this->e & 0x01) << 2;
    v |= (this->v & 0x01) << 1;
    v |= (this->z & 0x01);
    buffer[pos++] = v;

    // Picture ID
    if (this->i)
    {
        if (this->pid_m == 0)
        {
            CHECK_SIZE(size, pos + 1);
            buffer[pos++] = (uint8_t)(this->pid & 0x7F);
        }
        else
        {
            CHECK_SIZE(size, pos + 2);
            uint8_t p0 = (uint8_t)((this->pid >> 8) & 0x7F);
            p0 |= 0x80; // M bit
            uint8_t p1 = (uint8_t)(this->pid & 0xFF);
            buffer[pos++] = p0;
            buffer[pos++] = p1;
        }
    }

    // Layer indices
    if (this->l)
    {
        CHECK_SIZE(size, pos + 1);
        uint8_t li = 0;
        li |= (this->tid & 0x07) << 5;
        li |= (this->up_switch & 0x01) << 4;
        li |= (this->sid & 0x07) << 1;
        li |= (this->spatial_d & 0x01);
        buffer[pos++] = li;

        if (!this->f) // non-flexible: two octets (li + tl0picidx)
        {
            CHECK_SIZE(size, pos + 1);
            buffer[pos++] = this->tl0_pic_idx;
        }
    }

    if (this->p && this->f)
    {
        uint8_t c=std::min<uint8_t>(3,(uint8_t)p_diff.size());
        CHECK_SIZE(size, pos + c);
        for (uint8_t i = 0; i < c; i++)
        {
            uint8_t v = (this->p_diff[i] << 1);
            if (i < c - 1)
                v |= 1;
            buffer[pos++] = v;
        }
    }

    if (this->v) {
        return ss.serialize(buffer, pos, size);
    }
    else {
        return pos;
    }
}

int vp9_header::deserialize(const uint8_t* buffer, int offset, int size)
{
    int pos = offset;
    CHECK_SIZE(size, 1);

    uint8_t v = buffer[pos++];
    this->i = (v & 0x80) >> 7;
    this->p = (v & 0x40) >> 6;
    this->l = (v & 0x20) >> 5;
    this->f = (v & 0x10) >> 4;
    this->b = (v & 0x08) >> 3;
    this->e = (v & 0x04) >> 2;
    this->v = (v & 0x02) >> 1;
    this->z = (v & 0x01);

    // Picture ID (if present)
    if (this->i)
    {
        CHECK_SIZE(size, pos + 1);
        uint8_t p0 = buffer[pos++];
        this->pid_m = (p0 & 0x80) >> 7;
        if (this->pid_m == 0)
        {
            this->pid = (p0 & 0x7F);
        }
        else
        {
            // 15-bit PID in network order (two bytes)
            CHECK_SIZE(size, pos + 1);
            uint8_t p1 = buffer[pos++];
            this->pid = (((uint16_t)(p0 & 0x7F)) << 8) | p1;
        }
    }


    // Layer indices
    if (this->l)
    {
        CHECK_SIZE(size, pos + 1);
        uint8_t li = buffer[pos++];
        this->tid = (li & 0xE0) >> 5; // top 3 bits
        this->up_switch = (li & 0x10) >> 4;
        this->sid = (li & 0x0E) >> 1; // bits 3..1
        this->spatial_d = (li & 0x01);

        if (!this->f) // non-flexible mode: two octets (layer indices + TL0PICIDX)
        {
            CHECK_SIZE(size, pos + 1);
            this->tl0_pic_idx = buffer[pos++];
            // In flexible mode, TL0PICIDX is not present here; reference indices follow if P=1
        }
    }

    this->p_diff.clear();
    if (this->p && this->f)
    {
        for (int i = 0; i < 3; i++)
        {
            uint8_t b = buffer[pos++];
            uint8_t pdiff = b >> 1;
            uint8_t n = b & 0x01;
            this->p_diff.push_back(pdiff);
            if (n == 0)
                break;
        }
    }

    if (this->v) {
        return this->ss.deserialize(buffer, pos, size);
    }
    else {
        return pos;
    }
}




bool vp9_header::is_keyframe(const uint8_t* buffer,size_t size)
{
    if (size < 4)
        return false;

    uint32_t v = (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | (buffer[3]);
    int pos = 0;
    uint8_t marker = (uint8_t)bit::read_bits_32(v, pos, 2);
    pos += 2;
    if (marker != 2)
        return false;
    

    uint8_t version= (uint8_t)bit::read_bits_32(v, pos, 1);
    pos++;

    uint8_t high= (uint8_t)bit::read_bits_32(v, pos, 1);
    pos++;
    
    uint8_t profile = (high << 1) + version;
    if (profile == 3)
    {
        pos++;  //RESERVED_ZERO
    }

    uint8_t show_existing_frame = (uint8_t)bit::read_bits_32(v, pos, 1);
    pos++;

    if (show_existing_frame) {
        uint8_t index_of_frame_to_show = (uint8_t)bit::read_bits_32(v, pos, 3);
        pos += 3;
        return false;
    }

    uint8_t frame_type = (uint8_t)bit::read_bits_32(v, pos, 1);
    pos++;
    return (frame_type == 0);
}



