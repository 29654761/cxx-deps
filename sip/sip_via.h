#pragma once
#include "voip_uri.h"

class sip_via
{
public:
	sip_via();
	sip_via(bool tcp, const std::string& address, int port,const std::string& received, int rport, const std::string& branch);
	~sip_via();

	bool parse(const std::string& s);
	std::string to_string()const;

public:
	bool tcp = false;
	std::string address;
	int port = 0;
	int rport = -1;
	std::string received;
	std::string branch;
};

