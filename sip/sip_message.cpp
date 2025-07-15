#include "sip_message.h"
#include <sys2/string_util.h>

sip_message::sip_message()
{
	reset();
}


sip_message::~sip_message()
{

}

std::string sip_message::call_id()const
{
	std::string value;
	headers.get("Call-ID", value);
	return value.c_str();
}

void sip_message::set_call_id(const std::string& call_id)
{
	headers.set("Call-ID", call_id);
}



bool sip_message::get_cseq(uint32_t& cseq, std::string& method)const
{
	std::string value;
	if (!headers.get("CSeq", value))
	{
		return false;
	}

	auto ss=sys::string_util::split(value, " ", 2);
	if (ss.size() >= 1)
	{
		char* endptr = nullptr;
		cseq=(uint32_t)strtoul(ss[0].c_str(), &endptr, 10);
	}
	if (ss.size() >= 2)
	{
		method = ss[1];
	}
	return true;
}

uint32_t sip_message::get_cseq()const
{
	std::string value;
	if (!headers.get("CSeq", value))
	{
		return 0;
	}

	char* endptr = nullptr;
	return (uint32_t)strtoul(value.c_str(), &endptr, 10);
}

void sip_message::set_cseq(uint32_t cseq, const std::string& method)
{
	std::string s = std::to_string(cseq);
	if (method.size() > 0) {
		s += " " + method;
	}
	headers.set("CSeq", s);
}


std::vector<sip_via> sip_message::via()const
{
	std::vector<sip_via> vias;
	for (auto itr = headers.items.begin(); itr != headers.items.end(); itr++)
	{
		if (itr->name == "Via") {
			sip_via via;
			if (via.parse(itr->value)) {
				vias.push_back(via);
			}
		}
	}
	return vias;
}

std::vector<std::string> sip_message::svia()const
{
	std::vector<std::string> vias;
	for (auto itr = headers.items.begin(); itr != headers.items.end(); itr++)
	{
		if (itr->name == "Via") {
			vias.push_back(itr->value);
		}
	}
	return vias;
}

void sip_message::add_via(const std::string& via)
{
	headers.add("Via", via);
}

void sip_message::add_via(const sip_via& via)
{
	std::string v = via.to_string();
	headers.add("Via", v);
}

sip_address sip_message::to()const
{
	sip_address addr;
	to(addr);
	return addr;
}

bool sip_message::to(sip_address& addr)const
{
	std::string value;
	if (!headers.get("To", value))
	{
		return false;
	}
	addr.parse(value);
	return true;
}

void sip_message::set_to(const sip_address& to)
{
	std::string v = to.to_string();
	headers.set("To", v);
}

sip_address sip_message::from()const
{
	sip_address addr;
	from(addr);
	return addr;
}
bool sip_message::from(sip_address& addr)const
{
	std::string value;
	if (!headers.get("From", value))
	{
		return false;
	}
	addr.parse(value);
	return true;
}

void sip_message::set_from(const sip_address& to)
{
	std::string v = to.to_string();
	headers.set("From", v);
}

sip_address sip_message::contact()const
{
	sip_address addr;
	contact(addr);
	return addr;
}
bool sip_message::contact(sip_address& addr)const
{
	std::string value;
	if (!headers.get("Contact", value))
	{
		return false;
	}
	addr.parse(value);
	return true;
}

void sip_message::set_contact(const sip_address& contact)
{
	std::string v = contact.to_string();
	headers.set("Contact", v);
}

std::string sip_message::user_agent()const
{
	std::string value;
	headers.get("User-Agent", value);
	return value;
}

void sip_message::set_user_agent(const std::string& user_agent)
{
	headers.set("User-Agent", user_agent);
}

int sip_message::expires()const
{
	std::string value;
	if (!headers.get("Expires", value))
	{
		return 0;
	}
	char* endptr = nullptr;
	return (uint32_t)strtol(value.c_str(), &endptr, 10);
}
bool sip_message::expires(int& v)const
{
	std::string value;
	if (!headers.get("Expires", value))
	{
		return false;
	}
	char* endptr = nullptr;
	v = (uint32_t)strtol(value.c_str(), &endptr, 10);
	return true;
}

void sip_message::set_expires(int expires)
{
	std::string v = std::to_string(expires);
	headers.set("Expires", v);
}

std::string sip_message::allow()const
{
	std::string value;
	headers.get("Allow", value);
	return value;
}

void sip_message::set_allow(const std::string& allow)
{
	headers.set("Allow", allow);
}

int sip_message::max_forwards()const
{
	std::string value;
	if (!headers.get("Max-Forwards", value))
	{
		return 0;
	}
	char* endptr = nullptr;
	return (uint32_t)strtol(value.c_str(), &endptr, 10);
}
void sip_message::set_max_forwards(int max_forwards)
{
	std::string v = std::to_string(max_forwards);
	headers.set("Max-Forwards", v);
}

std::string sip_message::authenticate()const
{
	std::string value;
	headers.get("Authorization", value);
	return value;
}

void sip_message::set_authenticate(const std::string& v)
{
	headers.set("Authorization", v);
}

std::string sip_message::www_authenticate()const
{
	std::string value;
	headers.get("WWW-Authenticate", value);
	return value;
}

void sip_message::set_www_authenticate(const std::string& v)
{
	headers.set("WWW-Authenticate", v);
}

void sip_message::reset()
{
	hypertext::message::reset();
}

sip_message sip_message::create_request(const std::string& method, const voip_uri& url, uint64_t cseq, const std::vector<sip_via>& vias)
{
	sip_message request;
	request.set_request_version("SIP/2.0");
	request.set_method(method);
	request.set_url(url.to_string());
	for (auto itr = vias.begin(); itr != vias.end(); itr++)
		request.add_via(*itr);
	request.set_cseq((uint32_t)cseq, method);
	request.set_body("");
	return request;
}

sip_message sip_message::create_response(const sip_message& request)
{
	sip_message response;
	response.set_response_version("SIP/2.0");
	response.set_status("200");
	response.set_msg("OK");

	auto vias = request.svia();
	for (auto itr = vias.begin(); itr != vias.end(); itr++)
	{
		response.add_via(*itr);
	}

	uint32_t cseq;
	std::string cseq_method;
	request.get_cseq(cseq, cseq_method);
	response.set_cseq(cseq, cseq_method);

	response.set_call_id(request.call_id());
	response.set_from(request.from());
	response.set_to(request.to());
	response.set_body("");
	return response;
}
