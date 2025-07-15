#pragma once

#include "../base/base_request.h"
#include "rtsp_transport.h"

#include <sstream>

class rtsp_request:public base_request
{
public:
	rtsp_request();
	~rtsp_request();

	uint32_t cseq()const;
	void set_cseq(uint32_t cseq);

	std::string session()const;
	void set_session(const std::string& session);

	std::string user_agent()const;
	void set_user_agent(const std::string& user_agent);

	bool transport(rtsp_transport& transport)const;
	void set_transport(const rtsp_transport& transport);

	virtual void reset();
};