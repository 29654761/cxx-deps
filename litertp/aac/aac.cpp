/**
 * @file aac.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */

#include "aac.h"


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
