#pragma once
#include "sip_connection.h"

class sip_udp_connection:public sip_connection
{
public:
	typedef std::shared_ptr<asio::ip::udp::socket> socket_ptr;

	sip_udp_connection(asio::io_context& ioc, socket_ptr socket,const asio::ip::udp::endpoint& ep);
	~sip_udp_connection();
	virtual void stop();
	virtual bool is_tcp()const { return false; }
	virtual bool send_message(const sip_message& message);
	virtual bool remote_address(std::string& ip, int& port)const;
	virtual bool local_address(std::string& ip, int& port)const;

private:
	socket_ptr socket_;
	asio::ip::udp::endpoint ep_;
};

