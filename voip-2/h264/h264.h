/**
 * @file h264.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */

#pragma once


#include <stdint.h>



typedef struct _fu_header_t
{
	uint8_t s : 1;
	uint8_t e : 1;
	uint8_t r : 1;
	uint8_t t : 5;
}fu_header_t;

typedef struct _nal_header_t
{
	uint8_t f : 1;
	uint8_t nri : 2;
	uint8_t t : 5;
}nal_header_t;


class h264
{
public:


	static void fu_header_set(fu_header_t* fu, uint8_t v);

	static uint8_t fu_header_get(const fu_header_t* fu);

	static void nal_header_set(nal_header_t* nal, uint8_t v);

	static uint8_t nal_header_get(const nal_header_t* nal);

	static bool find_next_nal(const uint8_t* buffer, int size, int& offset, int& nal_start, int& nal_size, bool& islast);
	static int find_nal_start(const uint8_t* buffer, int size);

	static uint8_t get_nal_type(uint8_t b);
	static bool is_key_frame(uint8_t t);
};
