#include "sip_via.h"
#include <sys2/string_util.h>
#include <sstream>

sip_via::sip_via()
{
}

sip_via::sip_via(bool tcp, const std::string& address, int port, const std::string& received, int rport, const std::string& branch)
{
	this->tcp = tcp;
	this->address = address;
	this->port = port;
	this->received = received;
	this->rport = rport;
	this->branch = branch;
}

sip_via::~sip_via()
{
}

bool sip_via::parse(const std::string& s)
{
	auto ss=sys::string_util::split(s, " ", 2);
	if (ss.size() < 2) {
		return false;
	}


	auto header = sys::string_util::split(ss[0], "/");
	tcp = false;
	if (header.size() >= 3) {
		if (sys::string_util::icasecompare(header[2], "TCP"))
		{
			tcp = true;
		}
	}


	auto url= sys::string_util::split(ss[1], ";");
	if (url.size() == 0) {
		return false;
	}
	for (auto itr = url.begin(); itr != url.end(); itr++)
	{
		if (itr == url.begin()) {
			auto host_port=sys::string_util::split(*itr, ":", 2);
			if (host_port.size() >= 1) {
				address = host_port[0];
			}
			if (host_port.size() >= 2) {
				char* endptr = nullptr;
				port= strtol(host_port[1].c_str(),&endptr,10);
			}
			else {
				port = 0;
			}
		}
		else {
			auto kv = sys::string_util::split(*itr, "=", 2);
			rport = -1;
			if (kv.size() >= 1) {
				if (sys::string_util::icasecompare(kv[0], "rport")) {
					if (kv.size() >= 2)
					{
						char* endptr = nullptr;
						rport = strtol(kv[1].c_str(), &endptr, 10);
					}
					else
					{
						rport = 0;
					}
				}
				if (sys::string_util::icasecompare(kv[0], "received")) {
					if (kv.size() >= 2) {
						received = kv[1];
					}
				}
				else if (sys::string_util::icasecompare(kv[0], "branch")) {
					if (kv.size() >= 2) {
						branch = kv[1];
					}
				}
			}
		}
	}

	return true;
}

std::string sip_via::to_string()const
{
	std::stringstream ss;
	ss << "SIP/2.0";
	if (tcp) {
		ss << "/TCP";
	}
	else {
		ss << "/UDP";
	}
	ss << " ";

	ss << address;
	if (port > 0) {
		ss << ":" << port;
	}

	if (!branch.empty()) {
		ss << ";branch=" << branch;
	}
	if (!received.empty())
	{
		ss << ";received=" << received;
	}
	if (rport > 0)
	{
		ss << ";rport=" << rport;
	}
	else if (rport == 0)
	{
		ss << ";rport";
	}

	return ss.str();
}

