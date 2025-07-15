/**
 * @file sctp_chunk.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2025 Shijie Zhou
 */

#include "sctp_chunk.h"

namespace litertp
{

	sctp_chunk_tlv::sctp_chunk_tlv()
	{

	}

	sctp_chunk_tlv::~sctp_chunk_tlv()
	{

	}

	size_t sctp_chunk_tlv::deserialize(const uint8_t* data, size_t size)
	{
		if (size<4 || size>SCTP_MAX_CHUNK_SIZE)
			return 0;

		type = static_cast<uint16_t>((data[0] << 8) | data[1]);
		this->len = (data[2] << 8) | data[3];
		uint16_t remainder = len % 4;
		size_t padding = remainder != 0 ? (4 - remainder) : 0;
		size_t padding_len = len + padding;
		if (size < len)
			return 0;


		value.insert(value.end(), data + 4, data + len);

		return std::min<size_t>(size,padding_len);
	}

	std::vector<uint8_t> sctp_chunk_tlv::serialize() const
	{
		this->len = static_cast <uint16_t>(4 + value.size());
		uint16_t remainder = len % 4;
		uint16_t padding = remainder != 0 ? 4 - remainder : 0;
		uint16_t padding_len = remainder != 0 ? len + padding : len;
		std::vector<uint8_t> buf;
		buf.reserve(padding_len);

		buf.push_back(static_cast<uint8_t>((uint16_t)type >> 8));
		buf.push_back(static_cast<uint8_t>((uint16_t)type & 0xFF));

		buf.push_back(static_cast<uint8_t>(len >> 8));
		buf.push_back(static_cast<uint8_t>(len & 0xFF));

		buf.insert(buf.end(), value.begin(), value.end());
		if (padding > 0)
		{
			buf.insert(buf.end(), padding, 0);
		}
		return buf;
	}


	//===================================================


	sctp_chunk::sctp_chunk()
	{
		
	}

	sctp_chunk::sctp_chunk(sctp_chunk_type type, sctp_chunk_flags flags, const std::vector<uint8_t>& payload)
	{
		this->type = type;
		this->flags = flags;
		this->payload = payload;
	}

	sctp_chunk::~sctp_chunk()
	{
	}

	size_t sctp_chunk::deserialize(const uint8_t* data, size_t size)
	{
		if (size < 4 || size>SCTP_MAX_CHUNK_SIZE)
			return 0;

		type = static_cast<sctp_chunk_type>(data[0]);
		flags = static_cast<sctp_chunk_flags>(data[1]);

		len = (data[2] << 8) | data[3];
		size_t remainder = len % 4;
		size_t padding = remainder != 0 ? (4 - remainder) : 0;
		size_t padding_len = len + padding;

		if (padding_len < 4 || size < padding_len)
			return 0;

		payload.insert(payload.end(), data + 4, data + len);
		return padding_len;
	}

	std::vector<uint8_t> sctp_chunk::serialize() const
	{
		std::vector<uint8_t> buf;
		len = static_cast<uint16_t>(4 + payload.size());
		buf.reserve(len);
		buf.push_back(static_cast<uint8_t>(type));
		buf.push_back(static_cast<uint8_t>(flags));

		buf.push_back(static_cast<uint8_t>(len >> 8));
		buf.push_back(static_cast<uint8_t>(len & 0xFF));

		uint16_t remainder = len % 4;
		uint16_t padding = remainder != 0 ? (4 - remainder) : 0;

		buf.insert(buf.end(), payload.begin(), payload.end());
		if (padding > 0)
		{
			buf.insert(buf.end(), padding, 0);
		}
		return buf;
	}


	//====================================

	sctp_data_chunk::sctp_data_chunk()
	{

	}

	sctp_data_chunk::~sctp_data_chunk()
	{

	}

	bool sctp_data_chunk::deserialize(const uint8_t* data, size_t size)
	{
		if (size<12 || size>SCTP_MAX_CHUNK_SIZE)
			return false;
		
		tsn= (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
		stream_id = (data[4] << 8) | data[5];
		stream_sequence= (data[6] << 8) | data[7];
		payload_protocol_id= static_cast<sctp_payload_protocol_id>((data[8] << 24) | (data[9] << 16) | (data[10] << 8) | data[11]);

		payload.insert(payload.end(), data + 12, data + size);

		return true;
	}

	std::vector<uint8_t> sctp_data_chunk::serialize() const
	{
		std::vector<uint8_t> buf;
		uint16_t len = static_cast<uint16_t>(12 + payload.size());
		buf.push_back(static_cast<uint8_t>(tsn >> 24));
		buf.push_back(static_cast<uint8_t>(tsn >> 16));
		buf.push_back(static_cast<uint8_t>(tsn >> 8));
		buf.push_back(static_cast<uint8_t>(tsn & 0xFF));

		buf.push_back(static_cast<uint8_t>(stream_id >> 8));
		buf.push_back(static_cast<uint8_t>(stream_id & 0xFF));

		buf.push_back(static_cast<uint8_t>(stream_sequence >> 8));
		buf.push_back(static_cast<uint8_t>(stream_sequence & 0xFF));

		buf.push_back(static_cast<uint8_t>((uint32_t)payload_protocol_id >> 24));
		buf.push_back(static_cast<uint8_t>((uint32_t)payload_protocol_id >> 16));
		buf.push_back(static_cast<uint8_t>((uint32_t)payload_protocol_id >> 8));
		buf.push_back(static_cast<uint8_t>((uint32_t)payload_protocol_id & 0xFF));

		buf.insert(buf.end(),payload.begin(),payload.end());
		return buf;
	}




	//==============================

	sctp_sack_chunk::sctp_sack_chunk()
	{

	}

	sctp_sack_chunk::~sctp_sack_chunk()
	{

	}

	bool sctp_sack_chunk::deserialize(const uint8_t* data, size_t size)
	{
		const int offset = 12;
		if (size < offset || size>SCTP_MAX_CHUNK_SIZE)
			return false;

		tsn = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
		arwnd= (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];

		uint16_t num_gap = (data[8] << 8) | (data[9]);
		uint16_t num_dup = (data[10] << 8) | (data[11]);

		if (size < (uint32_t)(num_gap * 8 + num_dup * 4 + offset))
			return false;

		int pos = offset;
		for (uint16_t i = 0; i < num_gap; i++)
		{
			gap_item_t gap = {};
			gap.begin = (data[pos] << 8) | data[pos + 1];
			gap.end= (data[pos + 2] << 8) | data[pos + 3];
			gaps.push_back(gap);
			pos += 4;
		}

		for (uint16_t i = 0; i < num_gap; i++)
		{
			uint32_t dup= (data[pos] << 24) | (data[pos + 1] << 16) | (data[pos + 2] << 8) | data[pos + 3];
			dups.push_back(dup);
			pos += 4;
		}

		return true;
	}

	std::vector<uint8_t> sctp_sack_chunk::serialize() const
	{
		const int offset = 12;
		std::vector<uint8_t> buf;
		uint16_t num_gap = static_cast<uint16_t>(gaps.size());
		uint16_t num_dup = static_cast<uint16_t>(dups.size());
		uint16_t len = static_cast<uint16_t>(offset + num_gap * 8 + num_dup * 4);
		buf.reserve(len);

		buf.push_back(static_cast<uint8_t>(tsn >> 24));
		buf.push_back(static_cast<uint8_t>(tsn >> 16));
		buf.push_back(static_cast<uint8_t>(tsn >> 8));
		buf.push_back(static_cast<uint8_t>(tsn & 0xFF));

		buf.push_back(static_cast<uint8_t>(arwnd >> 24));
		buf.push_back(static_cast<uint8_t>(arwnd >> 16));
		buf.push_back(static_cast<uint8_t>(arwnd >> 8));
		buf.push_back(static_cast<uint8_t>(arwnd & 0xFF));

		buf.push_back(static_cast<uint8_t>(num_gap >> 8));
		buf.push_back(static_cast<uint8_t>(num_gap & 0xFF));

		buf.push_back(static_cast<uint8_t>(num_dup >> 8));
		buf.push_back(static_cast<uint8_t>(num_dup & 0xFF));

		for (uint16_t i = 0; i < num_gap; i++)
		{
			gap_item_t v = gaps[i];
			buf.push_back(static_cast<uint8_t>((uint16_t)v.begin >> 8));
			buf.push_back(static_cast<uint8_t>((uint16_t)v.begin & 0xFF));

			buf.push_back(static_cast<uint8_t>((uint16_t)v.end >> 8));
			buf.push_back(static_cast<uint8_t>((uint16_t)v.end & 0xFF));
		}

		for (uint16_t i = 0; i < num_dup; i++)
		{
			uint32_t v = dups[i];
			buf.push_back(static_cast<uint8_t>(v >> 24));
			buf.push_back(static_cast<uint8_t>(v >> 16));
			buf.push_back(static_cast<uint8_t>(v >> 8));
			buf.push_back(static_cast<uint8_t>(v & 0xFF));
		}

		return buf;
	}


	//==========================================


	sctp_heartbeat_chunk::sctp_heartbeat_chunk()
	{
	}

	sctp_heartbeat_chunk::~sctp_heartbeat_chunk()
	{
	}

	bool sctp_heartbeat_chunk::deserialize(const uint8_t* data, size_t size)
	{
		if (size<=0 || size>SCTP_MAX_CHUNK_SIZE)
			return false;


		for (size_t pos = 0; pos < size;)
		{
			sctp_chunk_tlv tlv;
			size_t len = tlv.deserialize(data + pos, size - pos);
			if (len == 0)
				return false;
			parameters.push_back(tlv);
			pos += len;
		}
		return true;

		return true;
	}

	std::vector<uint8_t> sctp_heartbeat_chunk::serialize() const
	{
		std::vector<uint8_t> buf;
		buf.reserve(1200);
		for (auto itr = parameters.begin(); itr != parameters.end(); itr++)
		{
			auto tlv = itr->serialize();
			buf.insert(buf.end(), tlv.begin(), tlv.end());
		}
		return buf;
	}

	bool sctp_heartbeat_chunk::find_paramter(sctp_init_parameter_type type, std::vector<uint8_t>& value)
	{
		for (auto itr = parameters.begin(); itr != parameters.end(); itr++)
		{
			if (itr->type == (uint16_t)type)
			{
				value = itr->value;
				return true;
			}
		}
		return false;
	}


	//=========================================



	sctp_init_chunk::sctp_init_chunk()
	{
	}

	sctp_init_chunk::~sctp_init_chunk()
	{
	}

	bool sctp_init_chunk::deserialize(const uint8_t* data, size_t size)
	{
		if (size < 16 || size>SCTP_MAX_CHUNK_SIZE)
			return false;

		init_tag = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
		arwnd = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];
		num_out_stream = (data[8] << 8) | data[9];
		num_in_stream = (data[10] << 8) | data[11];
		init_tsn= (data[12] << 24) | (data[13] << 16) | (data[14] << 8) | data[15];

		for (size_t pos = 16; pos < size;)
		{
			sctp_chunk_tlv tlv;
			size_t len=tlv.deserialize(data + pos, size - pos);
			if (len == 0)
				return false;
			parameters.push_back(tlv);
			pos += len;
		}
		return true;
	}

	std::vector<uint8_t> sctp_init_chunk::serialize() const
	{
		std::vector<uint8_t> buf;
		buf.reserve(1200);
		buf.push_back(static_cast<uint8_t>(init_tag >> 24));
		buf.push_back(static_cast<uint8_t>(init_tag >> 16));
		buf.push_back(static_cast<uint8_t>(init_tag >> 8));
		buf.push_back(static_cast<uint8_t>(init_tag));

		buf.push_back(static_cast<uint8_t>(arwnd >> 24));
		buf.push_back(static_cast<uint8_t>(arwnd >> 16));
		buf.push_back(static_cast<uint8_t>(arwnd >> 8));
		buf.push_back(static_cast<uint8_t>(arwnd));

		buf.push_back(static_cast<uint8_t>(num_out_stream >> 8));
		buf.push_back(static_cast<uint8_t>(num_out_stream));

		buf.push_back(static_cast<uint8_t>(num_in_stream >> 8));
		buf.push_back(static_cast<uint8_t>(num_in_stream));

		buf.push_back(static_cast<uint8_t>(init_tsn >> 24));
		buf.push_back(static_cast<uint8_t>(init_tsn >> 16));
		buf.push_back(static_cast<uint8_t>(init_tsn >> 8));
		buf.push_back(static_cast<uint8_t>(init_tsn));

		for (auto itr = parameters.begin(); itr != parameters.end(); itr++)
		{
			auto tlv=itr->serialize();
			buf.insert(buf.end(), tlv.begin(), tlv.end());
		}

		return buf;
	}

	bool sctp_init_chunk::find_paramter(sctp_init_parameter_type type, std::vector<uint8_t>& value)
	{
		for (auto itr = parameters.begin(); itr != parameters.end(); itr++)
		{
			if (itr->type == (uint16_t)type)
			{
				value = itr->value;
				return true;
			}
		}
		return false;
	}





	//=================================================

	sctp_shutdown_chunk::sctp_shutdown_chunk()
	{

	}

	sctp_shutdown_chunk::~sctp_shutdown_chunk()
	{

	}

	bool sctp_shutdown_chunk::deserialize(const uint8_t* data, size_t size)
	{
		if (size < 4 || size>SCTP_MAX_CHUNK_SIZE)
			return false;

		tsn = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
		return true;
	}

	std::vector<uint8_t> sctp_shutdown_chunk::serialize() const
	{
		std::vector<uint8_t> buf(4);
		buf[0] = (static_cast<uint8_t>(tsn >> 24));
		buf[1] = (static_cast<uint8_t>(tsn >> 16));
		buf[2] = (static_cast<uint8_t>(tsn >> 8));
		buf[3] = (static_cast<uint8_t>(tsn & 0xFF));
		return buf;
	}






	//============================================

	sctp_abort_chunk::sctp_abort_chunk()
	{

	}

	sctp_abort_chunk::~sctp_abort_chunk()
	{

	}

	bool sctp_abort_chunk::deserialize(const uint8_t* data, size_t size)
	{
		if (size>SCTP_MAX_CHUNK_SIZE)
			return false;

		for (size_t pos = 0; pos < size;)
		{
			sctp_chunk_tlv tlv;
			size_t len = tlv.deserialize(data + pos, size - pos);
			if (len == 0)
				return false;
			parameters.push_back(tlv);
			pos += len;
		}
		return true;
	}

	std::vector<uint8_t> sctp_abort_chunk::serialize() const
	{
		std::vector<uint8_t> buf;
		buf.reserve(1200);

		for (auto itr = parameters.begin(); itr != parameters.end(); itr++)
		{
			auto tlv = itr->serialize();
			buf.insert(buf.end(), tlv.begin(), tlv.end());
		}

		return buf;
	}

	//=====================================================

	sctp_error_chunk::sctp_error_chunk()
	{

	}

	sctp_error_chunk::~sctp_error_chunk()
	{

	}

	bool sctp_error_chunk::deserialize(const uint8_t* data, size_t size)
	{
		if (size > SCTP_MAX_CHUNK_SIZE)
			return false;

		for (size_t pos = 0; pos < size;)
		{
			sctp_chunk_tlv tlv;
			size_t len = tlv.deserialize(data + pos, size - pos);
			if (len == 0)
				return false;
			parameters.push_back(tlv);
			pos += len;
		}
		return true;
	}

	std::vector<uint8_t> sctp_error_chunk::serialize() const
	{
		std::vector<uint8_t> buf;
		buf.reserve(1200);

		for (auto itr = parameters.begin(); itr != parameters.end(); itr++)
		{
			auto tlv = itr->serialize();
			buf.insert(buf.end(), tlv.begin(), tlv.end());
		}

		return buf;
	}


	//=====================================================

	sctp_cookie_echo_chunk::sctp_cookie_echo_chunk()
	{

	}

	sctp_cookie_echo_chunk::~sctp_cookie_echo_chunk()
	{

	}

	bool sctp_cookie_echo_chunk::deserialize(const uint8_t* data, size_t size, uint16_t len)
	{
		if (size<len || size>SCTP_MAX_CHUNK_SIZE)
			return false;


		cookie.insert(cookie.end(),data, data+len);

		return true;
	}

	std::vector<uint8_t> sctp_cookie_echo_chunk::serialize() const
	{
		size_t len = cookie.size();
		std::vector<uint8_t> buf;
		buf.insert(buf.end(), cookie.begin(), cookie.end());
		return buf;
	}

}

