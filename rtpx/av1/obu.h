#pragma once

#include <stdint.h>

class obu
{
public:
	typedef struct _aggr_header_t
	{
		uint8_t z : 1;
		uint8_t y : 1;
		uint8_t w : 2;
		uint8_t n : 1;
	}aggr_header_t;

	static void decode_aggr_header(uint8_t b, aggr_header_t* hdr);
	static uint8_t encode_aggr_header(const aggr_header_t* hdr);

	static int encode(uint8_t* buffer,int offset, int size);
	static int decode(const uint8_t* buffer,int offset, int size);
};

