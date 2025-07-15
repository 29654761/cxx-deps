/**
 * @file h264.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */

#pragma once


#include <stdint.h>
#include <stddef.h>


class h264
{
public:
	struct fu_header_t
	{
		uint8_t s : 1;
		uint8_t e : 1;
		uint8_t r : 1;
		uint8_t t : 5;
	};

	struct nal_header_t
	{
		uint8_t f : 1;
		uint8_t nri : 2;
		uint8_t t : 5;
	};

	enum class nal_type_t
	{
		reserved=0,
		slice_non_idr=1,
		slice_a=2,
		slice_b=3,
		slice_c=4,
		idr=5,
		sei=6,
		sps=7,
		pps=8,
	};

	static void fu_header_set(fu_header_t* fu, uint8_t v);

	static uint8_t fu_header_get(const fu_header_t* fu);

	static void nal_header_set(nal_header_t* nal, uint8_t v);

	static uint8_t nal_header_get(const nal_header_t* nal);

	static bool find_next_nal(const uint8_t* buffer, size_t size, size_t& offset, size_t& nal_start, size_t& nal_size, bool& islast);
	static int64_t find_nal_start(const uint8_t* buffer, size_t size);

	static nal_type_t get_nal_type(uint8_t b);
	static bool is_key_frame(uint8_t t);
	static bool is_key_frame(nal_type_t t);
};
