#include "sip_udp_connection.h"

sip_udp_connection::sip_udp_connection(asio::io_context& ioc, socket_ptr socket, const asio::ip::udp::endpoint& ep)
	:sip_connection(ioc)
{
	socket_ = socket;
	ep_ = ep;

	std::string ip;
	int port = 0;
	this->remote_address(ip, port);
	id_ = make_id("udp", ip, port);
}

sip_udp_connection::~sip_udp_connection()
{
}

void sip_udp_connection::stop()
{
	sip_connection::stop();
}

bool sip_udp_connection::send_message(const sip_message& message)
{
	if (!socket_)
	{
		return false;
	}
	std::string s = message.to_string();
	if (log_) {
		log_->trace("Send sip message:\n{}", s)->flush();
	}
	asio::error_code ec;
	size_t r = socket_->send_to(asio::buffer(s),ep_,0,ec);

	return r > 0;
}

bool sip_udp_connection::remote_address(std::string& ip, int& port)const
{
	ip = ep_.address().to_string();
	port = ep_.port();
	return true;
}

bool sip_udp_connection::local_address(std::string& ip, int& port)const
{
	if (!socket_)
		return false;
	auto ep = socket_->local_endpoint();
	ip = ep.address().to_string();
	port = ep.port();
	return true;
}


