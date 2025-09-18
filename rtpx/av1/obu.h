#pragma once

#include <vector>


#define OBU_SEQUENCE_HEADER 1			//SPS
#define OBU_TEMPORAL_DELIMITER 2
#define OBU_FRAME_HEADER 3				//PPS
#define OBU_TILE_GROUP 4
#define OBU_METADATA 5
#define OBU_FRAME 6
#define OBU_REDUNDANT_FRAME_HEADER 7
#define OBU_TILE_LIST 8
#define OBU_PADDING  15


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

	typedef struct _header_t
	{
		uint8_t forbidden_bit : 1;
		uint8_t type : 4;
		uint8_t extension_flag : 1;
		uint8_t has_size_field : 1;
		uint8_t reserved_1bit : 1;

		uint8_t temporal_id : 3;
		uint8_t spatial_id : 2;
		uint8_t extension_header_reserved_3bits : 3;

		uint32_t obu_size;
	}header_t;

	typedef struct _element_t
	{
		uint8_t* data;
		size_t size;
	}element_t;

	static void decode_aggr_header(uint8_t b, aggr_header_t* hdr);
	static uint8_t encode_aggr_header(const aggr_header_t* hdr);

	static int decode_header(const uint8_t* buffer,int offset,int size, header_t* hdr);
	static int encode_header(const header_t* hdr, uint8_t* buffer, int offset, int size);

	static void find_elements(const uint8_t* buffer, size_t size,uint8_t w, std::vector<_element_t>& elements);
};

