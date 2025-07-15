#include "bfcp_packet.h"
#include <sys2/socket.h>

bfcp_packet::bfcp_packet()
{
	ver = 1;
	r = 0;
	f = 0;
	res = 0;
}

bfcp_packet::~bfcp_packet()
{
}

bool bfcp_packet::deserialize(const uint8_t* data, int size)
{
	attributes.clear();
	if (size < 12)
		return false;

	int pos = 0;
	ver = (data[pos] & 0xE0) >> 5;
	r = (data[pos] & 0x10) >> 4;
	f= (data[pos] & 0x8) >> 3;
	res = data[pos] & 0x7;
	pos++;

	primitive = (bfcp_packet_enum_t)data[pos];
	pos++;

	uint16_t payload_len = 0;
	memcpy(&payload_len, data + pos, 2);
	payload_len=sys::socket::ntoh16(payload_len);
	pos += 2;

	conference_id = 0;
	memcpy(&conference_id, data + pos, 4);
	conference_id = sys::socket::ntoh32(conference_id);
	pos += 4;

	transaction_id = 0;
	memcpy(&transaction_id, data + pos, 2);
	transaction_id = sys::socket::ntoh16(transaction_id);
	pos += 2;

	user_id = 0;
	memcpy(&user_id, data + pos, 2);
	user_id = sys::socket::ntoh16(user_id);
	pos += 2;

	if (f)
	{
		if (size < 16)
			return false;

		fragment_offset = 0;
		memcpy(&fragment_offset, data + pos, 2);
		fragment_offset = sys::socket::ntoh16(fragment_offset);
		pos += 2;

		fragment_length = 0;
		memcpy(&fragment_length, data + pos, 2);
		fragment_length = sys::socket::ntoh16(fragment_length);
		pos += 2;
	}

	if (pos + payload_len > size)
	{
		return false;
	}

	while (pos < size)
	{
		bfcp_attribute attr;
		int attrlen=attr.deserialize(data + pos, size - pos);
		if (attrlen <= 0)
			break;

		attributes.emplace_back(attr);
		pos += attrlen;
	}

	return true;
}

std::string bfcp_packet::serialize()const
{
	std::string payload;
	for (auto itr = attributes.begin(); itr != attributes.end(); itr++)
	{
		payload.append(itr->serialize());
	}


	std::string buf;
	uint8_t b = 0;
	b |= (ver << 5);
	b |= (r << 4);
	b |= (f << 3);
	b |= res;
	buf.append(1, (char)b);

	buf.append(1, (char)primitive);
	
	uint16_t payload_len = (uint16_t)payload.size();
	payload_len=sys::socket::hton16(payload_len);
	buf.append((const char*)&payload_len, 2);

	uint32_t conf_id= sys::socket::hton32(conference_id);
	buf.append((const char*)&conf_id,4);

	uint16_t ta_id = sys::socket::hton16(transaction_id);
	buf.append((const char*)&ta_id, 2);

	uint16_t uid = sys::socket::hton16(user_id);
	buf.append((const char*)&uid, 2);

	if (f)
	{
		uint16_t foff = sys::socket::hton16(fragment_offset);
		buf.append((const char*)&foff, 2);

		uint16_t flen = sys::socket::hton16(fragment_length);
		buf.append((const char*)&flen, 2);
	}

	buf.append(payload);

	return buf;
}

