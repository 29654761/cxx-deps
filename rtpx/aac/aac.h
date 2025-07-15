/**
 * @file aac.h
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */

#pragma once

#include <stdint.h>

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
	uint16_t build();
public:
	aac_encode_complexity_t complexity = aac_encode_complexity_low;
	int samplerate = 0;
	int channels = 0;
};