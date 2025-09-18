#pragma once

#include <stdint.h>

class bit
{
public:
	static uint32_t make_mask_32(uint8_t off, uint8_t bits);
	static uint32_t read_bits_32(uint32_t v, uint8_t off, uint8_t bits);
};

