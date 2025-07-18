#pragma once
#include <stdint.h>

class bit_reader
{
public:
	bit_reader(const uint8_t* buffer,size_t size);
	~bit_reader();

	bool read_bits(size_t bit_count, uint32_t& val);
	bool peek_bits(size_t bit_count, uint32_t& val);
	bool read_ue(uint32_t& val);
	bool read_se(int32_t& val);

	bool skip_scaling_list(size_t bit_count);
private:
	const uint8_t* buffer_ = nullptr;
	size_t buffer_size_ = 0;
	size_t byte_pos_ = 0;
	size_t bit_pos_ = 0;
};