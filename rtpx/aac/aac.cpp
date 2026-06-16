/**
 * @file aac.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */

#include "aac.h"
#include <string.h>

static int get_aac_freq_index(int sampleRate)
{
    switch (sampleRate)
    {
    case 96000: return 0;
    case 88200: return 1;
    case 64000: return 2;
    case 48000: return 3;
    case 44100: return 4;
    case 32000: return 5;
    case 24000: return 6;
    case 22050: return 7;
    case 16000: return 8;
    case 12000: return 9;
    case 11025: return 10;
    case 8000:  return 11;
    case 7350:  return 12;
    default:
        return -1;
    }
}


aac_specific_config::aac_specific_config()
{

}

aac_specific_config::~aac_specific_config()
{

}

void aac_specific_config::parse(uint16_t v)
{
    complexity = (_aac_encode_complexity_t)(uint16_t)((v & 0xF800) >> 11);

    int sr = (uint16_t)((v & 0x0780) >> 7);  //4bits samplerate 0-96000, 1-88200, 2-64000, 3-48000,4-44100,5-32000,6-24000,7-22050,8-16000

    if (sr == 0)
        samplerate = 96000;
    else if (sr == 1)
        samplerate = 88200;
    else if (sr == 2)
        samplerate = 64000;
    else if (sr == 3)
        samplerate = 48000;
    else if (sr == 4)
        samplerate = 44100;
    else if (sr == 5)
        samplerate = 32000;
    else if (sr == 6)
        samplerate = 24000;
    else if (sr == 7)
        samplerate = 22050;
    else if (sr == 8)
        samplerate = 16000;
    else if (sr == 9)
        samplerate = 8000;

    channels = (uint16_t)((v & 0x078) >> 3);     //4bits channels

    //3bits fixed 0
}

void aac_specific_config::parse(uint8_t b0, uint8_t b1)
{
    uint16_t v = 0;
    v |= (uint16_t)(b0 << 8);
    v |= b1;
    parse(v);
}

uint16_t aac_specific_config::build()
{
    uint16_t v = 0;
    uint16_t comp = (uint16_t)this->complexity;
    v |= (uint16_t)(comp << 11);

    uint16_t sr = 0;
    if (samplerate == 96000)
        sr = 0;
    else if (samplerate == 88200)
        sr = 1;
    else if (samplerate == 64000)
        sr = 2;
    else if (samplerate == 48000)
        sr = 3;
    else if (samplerate == 44100)
        sr = 4;
    else if (samplerate == 32000)
        sr = 5;
    else if (samplerate == 24000)
        sr = 6;
    else if (samplerate == 22050)
        sr = 7;
    else if (samplerate == 16000)
        sr = 8;
    else if (samplerate == 8000)
        sr = 9;

    v |= (uint16_t)(sr << 7);
    v |= (uint16_t)(channels << 3);

    return v;
}


bool aac_helper::add_adts_header(uint8_t* frame, int frame_size, int sampleRate, int channels, std::vector<uint8_t>& adts_frame)
{
    // AAC LC
    int profile = 2;

    int freqIdx = get_aac_freq_index(sampleRate);

    if (freqIdx < 0)
    {
        return false;
    }

    // ADTS header size = 7
    int packetLen = frame_size + 7;

    adts_frame.resize(packetLen);

    uint8_t* packet = adts_frame.data();

    // ====================================
    // ADTS HEADER
    // ====================================

    // syncword 0xFFF
    packet[0] = 0xFF;

    // MPEG-4 + layer + protection_absent
    packet[1] = 0xF1;

    packet[2] =
        ((profile - 1) << 6) |
        (freqIdx << 2) |
        (channels >> 2);

    packet[3] =
        ((channels & 3) << 6) |
        (packetLen >> 11);

    packet[4] =
        (packetLen & 0x7FF) >> 3;

    packet[5] =
        ((packetLen & 7) << 5) | 0x1F;

    packet[6] = 0xFC;

    // ====================================
    // AAC RAW DATA
    // ====================================

    memcpy(packet + 7, frame, frame_size);
    return true;
}