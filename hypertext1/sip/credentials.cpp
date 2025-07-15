#include "credentials.h"
#include <sstream>
#include <algorithm>
#include <sys2/string_util.h>
#include <sys2/util.h>
#include <openssl/evp.h>
#include <iomanip>

static std::string hashf(const EVP_MD* md, const void* data,size_t size)
{
	std::string ret;
	unsigned char digest[256] = { 0 };
	unsigned int osize = 0;
	if (EVP_Digest(data, size, digest, &osize, md, nullptr))
	{
		ret.assign((const char*)digest, osize);

		std::stringstream ss;
		for (int i = 0; i < ret.size(); i++) {
			ss << std::hex << std::nouppercase << std::setfill('0') << std::setw(2) << (int)(unsigned char)ret[i];
		}
		ret = ss.str();
	}


	return ret;
}



credentials::credentials()
{
}

credentials::~credentials()
{
}

std::string credentials::to_string(const std::string& prefix)const
{
	std::stringstream ss;
	ss << prefix << " ";
	ss << "username=\"" << username << "\",";
	ss << "realm=\"" << realm << "\",";
	ss << "nonce=\"" << nonce << "\",";
	ss << "uri=\"" << uri << "\",";

	if (algorithm.size() > 0)
	{
		ss << "algorithm=\"" << algorithm << "\",";
	}

	if (cnonce.size() > 0)
	{
		ss << "cnonce=\"" << cnonce << "\",";
	}
	if (opaque.size() > 0)
	{
		ss << "opaque=\"" << opaque << "\",";
	}
	if (qop.size() > 0)
	{
		ss << "qop=\"" << qop << "\",";
	}
	if (userhash)
	{
		ss << "userhash=\"" << userhash << "\",";
	}

	ss << "response=\"" << response << "\"";
			

	return ss.str();
}

bool credentials::parse(const std::string& s, const std::string& prefix)
{
	size_t skip = prefix.size() + 1;
	if (s.size() < skip)
	{
		return false;
	}

	std::string s1 = s.substr(prefix.size() + 1);
	auto ss = sys::string_util::split(s1, ",");
	for (auto itr = ss.begin(); itr != ss.end(); itr++)
	{
		auto kv = sys::string_util::split(*itr, "=", 2);
		if (kv.size() < 2)
			continue;
		sys::string_util::trim(kv[0]);
		sys::string_util::trim(kv[1]);
		format_value(kv[1]);
		std::string& key = kv[0];
		std::string& val = kv[1];

		if (key == "username")
		{
			this->username = val;
		}
		else if (key == "realm")
		{
			this->realm = val;
		}
		else if (key == "nonce")
		{
			this->nonce = val;
		}
		else if (key == "uri")
		{
			this->uri = val;
		}
		else if (key == "response")
		{
			this->response = val;
		}
		else if (key == "algorithm")
		{
			this->algorithm = val;
		}
		else if (key == "cnonce")
		{
			this->cnonce = val;
		}
		else if (key == "opaque")
		{
			this->opaque = val;
		}
		else if (key == "qop")
		{
			this->qop = val;
		}
		else if (key == "nc")
		{
			char* endptr = nullptr;
			this->nc=(int)strtol(val.c_str(), &endptr, 16);
		}
		else if (key == "userhash")
		{
			std::transform(val.begin(), kv[0].end(), kv[0].begin(), ::tolower);
			this->userhash = val == "true";
		}

	}
	return true;
}

std::string credentials::digest(const std::string& method, const std::string& password, const std::vector<std::string>* qops,const std::string& body)
{
	std::string rsp;
	const EVP_MD* md = nullptr;

	if (this->algorithm.size()==0||this->algorithm == "MD5")
	{
		md = EVP_md5();
	}
	else if (this->algorithm == "SHA-256")
	{
		md = EVP_sha256();
	}
	else if (this->algorithm == "SHA-512")
	{
		md = EVP_sha512();
	}
	else if (this->algorithm == "SHA-512-256")
	{
		md = EVP_sha512_256();
	}
	else
	{
		return rsp;
	}

			

	std::string susername;
	if (this->userhash)
	{
		unsigned char digest[100] = {};
		unsigned int size = 0;
		std::string s = this->username + ":" + this->realm;
		EVP_Digest(s.c_str(), s.size(), digest, &size, md, nullptr);
		susername.assign((const char*)digest, size);
	}
	else
	{
		susername = this->username;
	}


	if (!qops||qops->size() == 0)
	{
		unsigned char digest[100] = {};
		unsigned int size = 0;
		std::string s1 = susername + ":" + this->realm + ":" + password;
		s1=hashf(md, s1.c_str(), s1.size());
		std::string s2 = method + ":" + this->uri;
		s2 = hashf(md, s2.c_str(), s2.size());
		std::string s = s1 + ":"+ this->nonce+":" + s2;
		rsp = hashf(md, s.c_str(), s.size());

	}
	else if (qops && std::find(qops->begin(), qops->end(), "auth") != qops->end())
	{
		this->qop = "auth";
		if (this->cnonce.size() == 0)
		{
			std::stringstream ss;
			std::random_device rd;
			std::uniform_int_distribution<uint16_t> dist(0,0xFF);
			for (int i = 0; i < 8; i++)
			{
				uint16_t v = dist(rd);
				ss<< std::hex << std::nouppercase << std::setfill('0') << std::setw(2) << (uint8_t)v;
			}
			this->cnonce = ss.str();
		}
		if (this->nc == 0)
			this->nc = 1;

		std::string s1 = susername + ":" + this->realm + ":" + password;
		s1 = hashf(md, s1.c_str(), s1.size());
		std::string s2 = method + ":" + this->uri;
		s2 = hashf(md, s2.c_str(), s2.size());

		std::stringstream ss;
		ss << s1 << ":" << this->nonce << ":";
		ss << std::hex << std::nouppercase << std::setfill('0')<<std::setw(8) << this->nc << ":";
		ss << this->cnonce << ":";
		ss << this->qop << ":";
		ss << s2;
		std::string s = ss.str();
		rsp = hashf(md, s.c_str(), s.size());

				
	}
	else if (qops && std::find(qops->begin(), qops->end(), "auth-int") != qops->end())
	{
		this->qop = "auth-int";
		if (this->cnonce.size() == 0)
		{
			std::stringstream ss;
			std::random_device rd;
			std::uniform_int_distribution<uint16_t> dist(0, 0xFF);
			for (int i = 0; i < 8; i++)
			{
				uint16_t v = dist(rd);
				ss << std::hex << std::nouppercase << std::setfill('0') << std::setw(2) << (uint8_t)v;
			}
			this->cnonce = ss.str();
		}
		if (this->nc == 0)
			this->nc = 1;

		std::string sbody=hashf(md, body.c_str(), body.size());

		std::string s1 = susername + ":" + this->realm + ":" + password;
		s1 = hashf(md, s1.c_str(), s1.size());
		std::string s2 = method + ":" + this->uri + ":" + sbody;
		s2 = hashf(md, s2.c_str(), s2.size());

		std::stringstream ss;
		ss << s1 << ":" << this->nonce << ":";
		ss << std::hex << std::nouppercase << std::setfill('0') << std::setw(8) << this->nc << ":";
		ss << this->cnonce << ":";
		ss << this->qop << ":";
		ss << s2;
		std::string s = ss.str();
		rsp = hashf(md, s.c_str(), s.size());
	}
	else
	{
		return rsp;
	}

	return rsp;
}






void credentials::format_value(std::string& val)
{
	auto ritr = val.rbegin();
	if (ritr!=val.rend()&&*ritr == '\"')
	{
		val.erase((++ritr).base());
	}
	auto itr = val.begin();
	if (itr!=val.end()&&*itr == '\"')
	{
		val.erase(itr);
	}
}

