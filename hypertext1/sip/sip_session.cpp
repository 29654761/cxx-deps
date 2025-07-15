#include "sip_session.h"
#include "sip_server.h"
#include <sys2/string_util.h>

sip_session::sip_session(sip_server* server,spdlogger_ptr log, sys::async_socket_ptr socket, const sockaddr* addr, socklen_t addr_size)
{
	server_ = server;
	log_ = log;
	socket_ = socket;
	memcpy(&dst_addr_, addr, addr_size);
	socket_->closed_event.add(s_on_closed_event_t, this);
}


sip_session::~sip_session()
{
}


bool sip_session::is_timeout()const
{
	time_t now = time(nullptr);
	time_t sec = now - updated_time_;
	return sec >= 180;
}

bool sip_session::open()
{
	updated_time_ = time(nullptr);
	
	return true;
}

void sip_session::close()
{
	
	if (log_) {
		log_->debug("SIP session closed.")->flush();
	}
}

/*
bool sip_session::send_request(sip_message& request)
{
	request.filed3 = "SIP/2.0";
	std::string body = request.to_string();
	if(log_){
		log_->trace("Send sip message:\n{}", body)->flush();
	}
	return send_message(body);
}


bool sip_session::send_response(sip_message& response)
{
	response.filed1 = "SIP/2.0";
	std::string body = response.to_string();
	if(log_){
		log_->trace("Send sip message:\n{}", body)->flush();
	}
	return send_message(body);
}

bool sip_session::send_response(const std::string& status, const std::string& message, const std::string& content, const std::string& content_type)
{
	sip_message rsp;
	rsp.filed2 = status;
	rsp.filed3 = message;
	rsp.set_content_type(content_type);
	rsp.set_body(content);
	return send_response(rsp);
}
*/

std::string sip_session::get_client_address(const sip_message& request)
{
	std::string address;
	std::string forwards;
	if (request.headers.get("X-Forwarded-For", forwards))
	{
		std::vector<std::string> vec=sys::string_util::split(forwards, ",");
		address = vec[0];
	}
	return address;
}

void sip_session::get_remote_address(std::string& ip, int& port)
{
	sys::socket::addr2ep((const sockaddr*)&dst_addr_, &ip, &port);
}

bool sip_session::get_local_address(std::string& ip, int& port)
{
	if (!socket_)
		return false;
	sys::socket::local_addr(socket_->handle(), ip, port);
	return true;
}

void sip_session::on_message(const sip_message& message)
{
	if (log_) {
		std::string msg = message.to_string();
		log_->trace("Received sip message:\n{}", msg)->flush();
	}
	updated_time_ = time(nullptr);
	auto sft = shared_from_this();
	server_->session_message_event.invoke(sft, message);
}

void sip_session::s_on_read_event_t(void* ctx, sys::async_socket_ptr socket, const char* buffer, uint32_t size, const sockaddr* addr, socklen_t addr_size)
{
	sip_session* p = (sip_session*)ctx;
	p->on_data_received(buffer, size, addr, addr_size);
	socket->post_read();
}

void sip_session::s_on_closed_event_t(void* ctx, sys::async_socket_ptr socket)
{
	sip_session* p = (sip_session*)ctx;
	auto sft = p->shared_from_this();
	p->server_->on_close_session(sft);
}



