#include "bfcp_attribute.h"
#include <sys2/socket.h>

bfcp_attribute::bfcp_attribute()
{
	type = bfcp_attribute_enum_t::unknown;
	m = 0;
}

bfcp_attribute::~bfcp_attribute()
{
}

int bfcp_attribute::deserialize(const uint8_t* data, int len)
{
	if (len <= 2)
		return 0;

	int pos = 0;
	type = (bfcp_attribute_enum_t)((data[pos] & 0xEF) >> 1);
	m = data[pos] & 0x01;
	pos++;

	uint8_t clen = data[pos];
	pos++;

	if (clen > len)
		return 0;
	else if (clen < 2)
		return -1;

	contents.clear();
	contents.append((const char*)data + pos, clen - 2);
	pos += (clen - 2);

	return pos;
}

std::string bfcp_attribute::serialize()const
{
	std::string buf;

	uint8_t b = 0;
	b |= (((uint8_t)type) << 1);
	b |= m;
	buf.append(1,b);

	uint8_t len = (uint8_t)contents.size() + 2;
	buf.append(1,(char)len);
	buf.append(contents);
	return buf;
}


void bfcp_attribute::set_ushort(uint16_t v)
{
	v = sys::socket::hton16(v);
	contents.append((const char*)&v, 2);
}

uint16_t bfcp_attribute::get_ushort()const
{
	uint16_t v = 0;
	contents.copy((char*)&v, 2);
	return sys::socket::ntoh16(v);
}

void bfcp_attribute::set_string(const std::string& v)
{
	contents = v;
}

std::string bfcp_attribute::get_string()const
{
	return contents;
}

void bfcp_attribute::set_attribute(const bfcp_attribute& attr)
{
	contents = attr.serialize();
}

bool bfcp_attribute::get_attribute(bfcp_attribute& attr)const
{
	int r=attr.deserialize((const uint8_t*)contents.data(), (int)contents.size());
	return r > 0;
}

