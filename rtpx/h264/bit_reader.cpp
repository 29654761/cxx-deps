#include "bit_reader.h"

bit_reader::bit_reader(const uint8_t* buffer, size_t size)
{
	buffer_ = buffer;
	buffer_size_ = size;
	byte_pos_ = 0;
	bit_pos_ = 0;
}

bit_reader::~bit_reader()
{

}

bool bit_reader::read_bits(size_t bit_count, uint32_t& val)
{
	size_t old_byte_pos_ = byte_pos_;
	size_t old_bit_pos_ = bit_pos_;

	val = 0;
	for (size_t i = 0; i < bit_count; i++)
	{
		val <<= 1;
		if (byte_pos_ >= buffer_size_) 
		{
			byte_pos_ = old_byte_pos_;
			bit_pos_ = old_bit_pos_;
			return false;
		}

		val|= (buffer_[byte_pos_] >> (7 - bit_pos_)) & 0x01;
		bit_pos_++;
		if (bit_pos_ == 8)
		{
			bit_pos_ = 0;
			byte_pos_++;
		}
	}

	return true;
}

bool bit_reader::peek_bits(size_t bit_count, uint32_t& val)
{
	size_t old_byte_pos_ = byte_pos_;
	size_t old_bit_pos_ = bit_pos_;

	if (!read_bits(bit_count, val))
		return false;

	byte_pos_ = old_byte_pos_;
	bit_pos_ = old_bit_pos_;
	return val;
}

bool bit_reader::read_ue(uint32_t& val)
{
	size_t old_byte_pos_ = byte_pos_;
	size_t old_bit_pos_ = bit_pos_;
	uint32_t zeros = 0;
	uint32_t v = 0;
	for(;;)
	{
		if (!read_bits(1, v))
		{
			byte_pos_ = old_byte_pos_;
			bit_pos_ = old_bit_pos_;
			return false;
		}
		if (v != 0)
			break;

		zeros++;
	}

	val = (1 << zeros) - 1;

	if (!read_bits(zeros, v))
	{
		byte_pos_ = old_byte_pos_;
		bit_pos_ = old_bit_pos_;
		return false;
	}
	val += v;
	return true;
}

bool bit_reader::read_se(int32_t& val)
{
	uint32_t ue_val = 0;
	val = 0;
	if (!read_ue(ue_val)) {
		return false;
	}

	int32_t sign = ((ue_val & 1) == 0) ? -1 : 1;
	val = ((ue_val + 1) >> 1) * sign;
	return true;
}

bool bit_reader::skip_scaling_list(size_t bit_count)
{
	int32_t last_scale = 8;
	int32_t next_scale = 8;
	size_t old_byte_pos_ = byte_pos_;
	size_t old_bit_pos_ = bit_pos_;

	for (size_t j = 0; j < bit_count; j++)
	{
		if (next_scale != 0)
		{
			int32_t delta_scale = 0;
			if (!read_se(delta_scale)) 
			{
				byte_pos_ = old_byte_pos_;
				bit_pos_ = old_bit_pos_;
				return false;
			}
			next_scale = (last_scale + delta_scale + 256) % 256;
		}
		last_scale = (next_scale == 0) ? last_scale : next_scale;
	}

	return true;
}



