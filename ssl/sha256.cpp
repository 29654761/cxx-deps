#include "sha256.h"
#include <memory>

namespace ssl
{

	sha256::sha256()
	{
		SHA256_Init(&ctx_);
	}

	sha256::~sha256()
	{
	}

	bool sha256::update(const void* data, size_t len)
	{
		return SHA256_Update(&ctx_, data, len);
	}

	std::string sha256::final()
	{
		uint8_t hash[SHA256_DIGEST_LENGTH] = {};
		SHA256_Final(hash, &ctx_);
		std::string r;
		r.assign((const char*)hash, SHA256_DIGEST_LENGTH);
		return r;
	}


}
