#include "challenge.h"
#include <sstream>
#include <algorithm>
#include <sys2/string_util.h>


challenge::challenge()
{
}

challenge::~challenge()
{
}

std::string challenge::to_string(const std::string& prefix)const
{
	std::stringstream ss;
	ss << prefix << " ";
	ss << "realm=\"" << realm << "\",";
	if (domains.size() > 0)
	{
		std::string domain=sys::string_util::join(" ", domains);
		ss << "domain=\"" << domain << "\",";
	}
	ss << "nonce=\"" << nonce << "\",";
	if (opaque.size() > 0)
	{
		ss << "opaque=\"" << opaque << "\",";
	}
	if (stale)
	{
		ss << "stale=\"" << stale << "\",";
	}
	if (algorithm.size() > 0)
	{
		ss << "algorithm=\"" << algorithm << "\",";
	}
	if (qops.size() > 0)
	{
		std::string qop = sys::string_util::join(",", qops);
		ss << "qop=\"" << qop << "\",";
	}
	if (charset.size() > 0)
	{
		ss << "charset=\"" << charset << "\",";
	}
	if (userhash)
	{
		ss << "userhash=\"" << userhash << "\",";
	}

	std::string s = ss.str();
	if (s.size() > 0)
	{
		size_t pos = s.find_last_of(',');
		if (pos != std::string::npos) {
			s.erase(pos);
		}
	}

	return s;
}

bool challenge::parse(const std::string& s, const std::string& prefix)
{
	size_t skip = prefix.size() + 1;
	if (s.size() < skip)
	{
		return false;
	}

	std::string s1 = s.substr(0, prefix.size() + 1);
	auto ss = sys::string_util::split(s1, ",");
	for (auto itr = ss.begin(); itr != ss.end(); itr++)
	{
		auto kv = sys::string_util::split(*itr, "=", 2);
		if (kv.size() < 2)
			continue;
		sys::string_util::trim(kv[0]);
		sys::string_util::trim(kv[1]);
		//std::transform(kv[0].begin(), kv[0].end(), kv[0].begin(), std::tolower);
		format_value(kv[1]);
		std::string& key = kv[0];
		std::string& val = kv[1];

		if (key == "realm")
		{
			this->realm = val;
		}
		else if (key == "domain")
		{
			this->domains = sys::string_util::split(val, " ");
		}
		else if (key == "nonce")
		{
			this->nonce = val;
		}
		else if (key == "algorithm")
		{
			this->algorithm = val;
		}
		else if (key == "stale")
		{
			std::transform(val.begin(), kv[0].end(), kv[0].begin(), ::tolower);
			this->stale = val == "true";
		}
		else if (key == "opaque")
		{
			this->opaque = val;
		}
		else if (key == "qop")
		{
			this->qops = sys::string_util::split(val, " ");
		}
		else if (key == "charset")
		{
			this->charset = val;
		}
		else if (key == "userhash")
		{
			std::transform(val.begin(), kv[0].end(), kv[0].begin(), ::tolower);
			this->userhash = val == "true";
		}
	}

	return true;
}

void challenge::format_value(std::string& val)
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

bool challenge::supports_qop(const std::string& qop)const
{
	for (auto itr = qops.begin(); itr != qops.end(); itr++)
	{
		if (*itr == qop)
		{
			return true;
		}
	}
	return false;
}


