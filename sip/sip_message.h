#pragma once
#include "sip_address.h"
#include "sip_via.h"
#include <hypertext/message.h>
#include <sstream>
#include <set>

class sip_message:public hypertext::message
{
public:
	sip_message();
	~sip_message();

	std::string call_id()const;
	void set_call_id(const std::string& call_id);

	bool get_cseq(uint32_t& cseq, std::string& method)const;
	uint32_t get_cseq()const;
	void set_cseq(uint32_t cseq,const std::string& method="");


	std::vector<sip_via> via()const;
	std::vector<std::string> svia()const;
	void add_via(const std::string& via);
	void add_via(const sip_via& via);

	sip_address to()const;
	bool to(sip_address& addr)const;
	void set_to(const sip_address& to);

	sip_address from()const;
	bool from(sip_address& addr)const;
	void set_from(const sip_address& to);

	sip_address contact()const;
	bool contact(sip_address& addr)const;
	void set_contact(const sip_address& contact);

	std::string user_agent()const;
	void set_user_agent(const std::string& user_agent);

	int expires()const;
	bool expires(int& v)const;
	void set_expires(int expires);

	std::string allow()const;
	void set_allow(const std::string& allow);

	int max_forwards()const;
	void set_max_forwards(int max_forwards);

	std::string authenticate()const;
	void set_authenticate(const std::string& v);

	std::string www_authenticate()const;
	void set_www_authenticate(const std::string& v);

	virtual void reset();

	static sip_message create_request(const std::string& method,const voip_uri& url, uint64_t cseq,const std::vector<sip_via>& vias);
	static sip_message create_response(const sip_message& request);
};