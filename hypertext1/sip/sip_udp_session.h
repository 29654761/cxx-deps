#pragma once
#include "sip_session.h"

class sip_udp_session:public sip_session
{
public:
	sip_udp_session(sip_server* server, spdlogger_ptr log, sys::async_socket_ptr socket,const sockaddr* addr,socklen_t addr_size);
	~sip_udp_session();
	virtual std::string id()const;
	virtual bool is_tcp()const { return false; }
	virtual bool send_message(const sip_message& message);
	void on_data_received(const char* buffer, uint32_t size, const sockaddr* addr, socklen_t addr_size);

};

