#pragma once
#include "sip_session.h"
#include <sys2/ringbuffer.hpp>

class sip_tcp_session:public sip_session
{
public:
	sip_tcp_session(sip_server* server, spdlogger_ptr log, sys::async_socket_ptr socket, const sockaddr* addr, socklen_t addr_size);
	~sip_tcp_session();

	virtual std::string id()const;
	virtual bool open();
	virtual void close();

	virtual bool is_tcp()const { return true; }

	virtual bool send_message(const sip_message& message);

protected:
	void on_data_received(const char* buffer, uint32_t size, const sockaddr* addr, socklen_t addr_size);

private:
	sys::ring_buffer<char> ring_buffer_;
};

