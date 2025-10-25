#pragma once

#include <stdint.h>
#include <stddef.h>

class leb128
{
public:
	static size_t encode(uint32_t value, uint8_t* out, size_t len);
	static size_t decode(const uint8_t* data, size_t len, uint32_t* value);
};

