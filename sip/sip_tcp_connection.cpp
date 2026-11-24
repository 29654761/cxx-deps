#include "sip_tcp_connection.h"
#include "sip_server.h"
#include <atomic>

sip_tcp_connection::sip_tcp_connection(asio::io_context& ioc, socket_ptr socket):
	sip_connection(ioc),
	recv_ring_buffer_(1024*1024)
{
	recv_buffer_.fill(0);
	socket_ = socket;

	std::string ip;
	int port = 0;
	this->remote_address(ip, port);
	id_ = make_id("tcp", ip, port);

	sending_queue_ = std::make_shared<asio::sending_queue>();
}

// 呼出连接 id 的唯一序号：id 只由远端地址构成时，同目标地址的并发呼出连接会在
// sip_server 的连接表里撞 key（add_connection 对冲突 key 会替换并 stop 旧连接，
// 误杀同目标地址的在途呼叫），呼出方向必须逐条区分
static std::atomic<uint64_t> s_outgoing_con_seq{ 0 };

sip_tcp_connection::sip_tcp_connection(asio::io_context& ioc, const asio::ip::tcp::endpoint& ep)
	:sip_connection(ioc),
	recv_ring_buffer_(1024 * 1024)
{
	recv_buffer_.fill(0);
	remote_ep_ = ep;

	asio::error_code ec;
	std::string ip = ep.address().to_string(ec);
	int port = ep.port();
	id_ = make_id("tcp", ip, port) + "$" + std::to_string(++s_outgoing_con_seq);

	sending_queue_ = std::make_shared<asio::sending_queue>();
}


sip_tcp_connection::~sip_tcp_connection()
{
}


bool sip_tcp_connection::start()
{
	if (active_)
		return true;
	active_ = true;
	if (!sip_connection::start())
	{
		//invoke_close();
		stop();
		return false;
	}
	recv_ring_buffer_.clear();

	auto self = shared_from_this();
	if (!socket_)
	{
		std::error_code ec;
		socket_ = std::make_shared<asio::ip::tcp::socket>(ioc_);
		socket_->open(remote_ep_.protocol(), ec);
		if (ec)
		{
			if (log_)
			{
				log_->error("SIP connection open socket failed: {}:{}: {}",
					remote_ep_.address().to_string(), remote_ep_.port(), ec.message())->flush();
			}
			stop();
			return false;
		}
		socket_->connect(remote_ep_,ec);
		if (ec)
		{
			if (log_)
			{
				log_->error("SIP connection connect failed: {}:{}: {}",
					remote_ep_.address().to_string(), remote_ep_.port(), ec.message())->flush();
			}
			stop();
			return false;
		}
	}

	socket_->async_read_some(asio::buffer(recv_buffer_.data(), recv_buffer_.size()), std::bind(&sip_tcp_connection::handle_read, this, self, std::placeholders::_1, std::placeholders::_2));
	return true;
}

void sip_tcp_connection::stop()
{
	active_ = false;
	if (socket_)
	{
		std::error_code ec;
		socket_->close(ec);
		//std::string err = ec.message();
	}
	//recv_ring_buffer_.clear();
	//sending_queue_->clear();
	sip_connection::stop();
}

bool sip_tcp_connection::send_message(const sip_message& message)
{
	// 已停/未启动的连接不再排队发送：sending_queue 的异步写在已关闭的 socket 上必然失败，
	// 排进去只会让 "Send sip message" 日志看起来发出去了（实际被静默丢弃）
	if (!socket_ || !active_)
	{
		return false;
	}
	std::string s = message.to_string();
	if (log_) {
		log_->trace("Send sip message:\n{}", s)->flush();
	}

	auto self = shared_from_this();
	return sending_queue_->send(socket_, s);
}

bool sip_tcp_connection::remote_address(std::string& ip, int& port)const
{
	if (!socket_)
		return false;

	std::error_code ec;
	auto ep = socket_->remote_endpoint(ec);
	if (ec)
		return false;
	ip = ep.address().to_string();
	port = ep.port();
	return true;
}

bool sip_tcp_connection::local_address(std::string& ip, int& port)const
{
	if (!socket_)
		return false;
	std::error_code ec;
	auto ep = socket_->local_endpoint(ec);
	if (ec)
		return false;
	ip = ep.address().to_string();
	port = ep.port();
	return true;
}

void sip_tcp_connection::handle_read(sip_connection_ptr con, const std::error_code& ec, std::size_t bytes)
{
	if (ec) {
		invoke_close();
		return;
	}
	if (!recv_ring_buffer_.write(recv_buffer_.data(), bytes))
	{
		invoke_close();
		return;
	}

	size_t rdsize = recv_ring_buffer_.available_to_read();
	if (rdsize == 0) {
		invoke_close();
		return;
	}
	std::unique_ptr<char[], std::function<void(char*)>> buf(new char[rdsize], [](char* ptr) {
		delete[] ptr;
		});
	if (!recv_ring_buffer_.peek(buf.get(), rdsize))
	{
		invoke_close();
		return;
	}

	int64_t pos = 0;
	while (rdsize > 0)
	{
		sip_message msg;
		int64_t r = msg.parse(buf.get() + pos, rdsize);
		if (r < 0)
		{
			invoke_close();
			return;
		}
		if (r == 0)
		{
			break;
		}
		if (msg.headers.items.size() > 0) {
			this->on_message(msg);
		}
		recv_ring_buffer_.remove(r);
		pos += r;
		rdsize -= (size_t)r;
	}

	if (active_) {
		socket_->async_read_some(asio::buffer(recv_buffer_.data(), recv_buffer_.size()), std::bind(&sip_tcp_connection::handle_read, this, con, std::placeholders::_1, std::placeholders::_2));
	}
}




