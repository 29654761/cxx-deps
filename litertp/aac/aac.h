/**
 * @file aac.h
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */

#pragma once

#include <stdint.h>
#include <vector>

typedef enum _aac_encode_complexity_t
{
	aac_encode_complexity_main=1,
	aac_encode_complexity_low=2,
}aac_encode_complexity_t;

typedef struct _aac_specific_config_t
{
	aac_encode_complexity_t complexity;
	int samplerate;
	int channels;
}aac_specific_config_t;



class aac_specific_config
{
public:
	aac_specific_config();
	~aac_specific_config();

	void parse(uint16_t v);
	void parse(uint8_t b0,uint8_t b1);
	uint16_t build();
public:
	aac_encode_complexity_t complexity = aac_encode_complexity_low;
	int samplerate = 0;
	int channels = 0;
};

class aac_helper
{
public:
	static bool add_adts_header(uint8_t* frame, int frame_size, int sampleRate, int channels, std::vector<uint8_t>& adts_frame);
};
