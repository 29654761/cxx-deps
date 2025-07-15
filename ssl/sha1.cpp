#include "sha1.h"
#include <memory>

namespace ssl
{

	sha1::sha1()
	{
		SHA1_Init(&ctx_);
	}

	sha1::~sha1()
	{
	}

	bool sha1::update(const void* data, size_t len)
	{
		return SHA1_Update(&ctx_, data, len);
	}

	std::string sha1::final()
	{
		uint8_t hash[20] = {};
		SHA1_Final(hash, &ctx_);
		std::string r;
		r.assign((const char*)hash, 20);
		return r;
	}


}
