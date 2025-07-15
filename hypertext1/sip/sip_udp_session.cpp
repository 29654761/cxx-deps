#include "sip_udp_session.h"

sip_udp_session::sip_udp_session(sip_server* server, spdlogger_ptr log, sys::async_socket_ptr socket,const sockaddr* addr, socklen_t addr_size)
	:sip_session(server,log,socket,addr,addr_size)
{
}

sip_udp_session::~sip_udp_session()
{
	close();
}


std::string sip_udp_session::id()const
{
	std::string addr;
	int port = 0;
	sys::socket::addr2ep((const sockaddr*)&dst_addr_,&addr, &port);
	return "udp$" + addr + ":" + std::to_string(port);
}


bool sip_udp_session::send_message(const sip_message& message)
{
	if (!socket_)
	{
		return false;
	}
	std::string s = message.to_string();
	if (log_) {
		log_->trace("Send sip message:\n{}", s)->flush();
	}
	return socket_->send_to(s.data(), s.size(), (const sockaddr*)&dst_addr_, sizeof(dst_addr_));
}

void sip_udp_session::on_data_received(const char* buffer, uint32_t size, const sockaddr* addr, socklen_t addr_size)
{
	sip_message msg;
	std::string sip(buffer, size);
	if (!msg.parse(sip))
	{
		if (log_) {
			log_->error("parse sip protocol error: {}", sip.c_str())->flush();
		}
		return;
	}

	this->on_message(msg);
}
