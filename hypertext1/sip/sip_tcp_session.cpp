#include "sip_tcp_session.h"
#include "sip_server.h"

sip_tcp_session::sip_tcp_session(sip_server* server, spdlogger_ptr log, sys::async_socket_ptr socket, const sockaddr* addr, socklen_t addr_size):
	sip_session(server,log,socket,addr,addr_size),
	ring_buffer_(1024*1024)
{
	socket->read_event.add(s_on_read_event_t, this);
}

sip_tcp_session::~sip_tcp_session()
{
	close();
}

std::string sip_tcp_session::id()const
{
	std::string addr;
	int port = 0;
	sys::socket::addr2ep((const sockaddr*)&dst_addr_, &addr, &port);
	return "tcp$" + addr + ":" + std::to_string(port);
}

bool sip_tcp_session::open()
{
	
	if (!sip_session::open())
		return false;

	socket_->post_read();
	return true;
}

void sip_tcp_session::close()
{
	sip_session::close();

	if (socket_)
	{
		socket_->close();
		socket_.reset();
	}
	ring_buffer_.clear();
}

bool sip_tcp_session::send_message(const sip_message& message)
{
	if (!socket_)
	{
		return false;
	}
	std::string s = message.to_string();
	if (log_) {
		log_->trace("Send sip message:\n{}", s)->flush();
	}
	return socket_->send(s.data(), (int)s.size());
}

void sip_tcp_session::on_data_received(const char* buffer, uint32_t size, const sockaddr* addr, socklen_t addr_size)
{
	if (!ring_buffer_.write(buffer, size))
	{
		if (log_) {
			log_->error("ring buffer is full,close socket")->flush();
		}
		auto sft = shared_from_this();
		server_->on_close_session(sft);
		return;
	}

	size_t len = ring_buffer_.available_to_read();
	if (len == 0) {
		return;
	}
	char* buf = new char[len]();
	if (!ring_buffer_.peek(buf, len))
	{
		delete[] buf;
		return;
	}

	int64_t pos = 0;
	while(len>0)
	{
		sip_message msg;
		int64_t r=msg.parse(buf+pos,len);
		if (r <= 0) {
			if (r < 0) {
				auto sft = shared_from_this();
				server_->on_close_session(sft);
			}
			break;
		}
		else
		{
			if (msg.headers.items.size() > 0) {
				this->on_message(msg);
			}
			ring_buffer_.remove(r);
			pos += r;
			len -= (size_t)r;
		}
	}
	delete[] buf;
}


