#include "sip_server.h"
#include "sip_tcp_connection.h"
#include "sip_udp_connection.h"
#include <hypertext/uri.h>
#include <sys2/util.h>

sip_server::sip_server(asio::io_context& ioc)
	:ioc_(ioc)
	, timer_(ioc_)
{
	udp_recv_buffer_.fill(0);
}

sip_server::~sip_server()
{
	stop();
}




bool sip_server::start(const std::string& addr, int port)
{
	tcp_socket_ = std::make_shared<asio::ip::tcp::acceptor>(ioc_);
	std::error_code ec;

	auto ip=asio::ip::address::from_string(addr, ec);
	if (ec)
	{
		stop();
		return false;
	}
	asio::ip::tcp::endpoint ep_tcp(ip, port);

	tcp_socket_->open(ep_tcp.protocol(), ec);
	if (ec)
	{
		stop();
		return false;
	}
	tcp_socket_->set_option(asio::socket_base::reuse_address(true),ec);
	tcp_socket_->bind(ep_tcp, ec);
	if (ec)
	{
		stop();
		return false;
	}
	tcp_socket_->listen(asio::socket_base::max_listen_connections, ec);
	if (ec)
	{
		stop();
		return false;
	}

	asio::ip::udp::endpoint ep_udp(ip, port);
	udp_socket_ = std::make_shared<asio::ip::udp::socket>(ioc_);
	udp_socket_->open(ep_udp.protocol(), ec);
	if (ec)
	{
		stop();
		return false;
	}
	udp_socket_->set_option(asio::socket_base::reuse_address(true), ec);
	udp_socket_->bind(ep_udp, ec);
	if (ec)
	{
		stop();
		return false;
	}

	auto self = shared_from_this();
	tcp_socket_->async_accept(std::bind(&sip_server::handle_accept,this, self,std::placeholders::_1,std::placeholders::_2));
	udp_socket_->async_receive_from(asio::buffer(udp_recv_buffer_, udp_recv_buffer_.size()), udp_endpoint_,
		std::bind(&sip_server::handle_read,this, self, udp_socket_, std::placeholders::_1, std::placeholders::_2));
	
	timer_.expires_after(std::chrono::milliseconds(20000));
	timer_.async_wait(std::bind(&sip_server::handle_timer, this, self, std::placeholders::_1));

	return true;
}

void sip_server::stop()
{
	std::error_code ec;
	timer_.cancel(ec);
	if (tcp_socket_)
	{
		tcp_socket_->cancel(ec);
	}
	if (udp_socket_)
	{
		udp_socket_->cancel(ec);
	}


	std::vector<sip_connection_ptr> cons;
	all_connections(cons);
	for (auto itr = cons.begin(); itr != cons.end(); itr++)
	{
		(*itr)->stop();
	}
	clear_connections();
}

void sip_server::handle_message(sip_server_ptr svr, sip_connection_ptr con, const sip_message& msg)
{
	if (message_handler_)
	{
		message_handler_(svr, con, msg);
	}
}

void sip_server::handle_remove(sip_server_ptr svr, sip_connection_ptr con)
{
	remove_connection(con->id());
}

void sip_server::handle_accept(sip_server_ptr svr, const std::error_code& ec, asio::ip::tcp::socket newsocket)
{
	if (!ec)
	{
		auto sktptr = std::make_shared<asio::ip::tcp::socket>(std::move(newsocket));
		auto con = std::make_shared<sip_tcp_connection>(ioc_, sktptr);
		add_connection(con);
		con->start();
	}
	else if(ec==asio::error::operation_aborted||ec==asio::error::bad_descriptor)
	{
		return;
	}

	auto self = shared_from_this();
	tcp_socket_->async_accept(std::bind(&sip_server::handle_accept,this,self, std::placeholders::_1, std::placeholders::_2));

}

void sip_server::handle_read(sip_server_ptr svr, udp_socket_ptr skt,const std::error_code& ec, std::size_t bytes)
{
	if (ec)
		return;

	auto self = shared_from_this();
	sip_message msg;
	int64_t r = msg.parse(udp_recv_buffer_.data(), bytes);
	if (r > 0)
	{

		std::string ip = udp_endpoint_.address().to_string();
		int port = udp_endpoint_.port();
		std::string id = sip_connection::make_id("udp", ip, port);

		sip_connection_ptr con;
		{
			std::lock_guard<std::mutex> lk(connections_mutex_);
			auto itr = connections_.find(id);
			if (itr == connections_.end())
			{
				con = std::make_shared<sip_udp_connection>(ioc_, skt, udp_endpoint_);
				con->set_logger(log_);
				con->set_message_handler(std::bind(&sip_server::handle_message, this, self, std::placeholders::_1, std::placeholders::_2));
				con->set_remove_handler(std::bind(&sip_server::handle_remove, this, self, std::placeholders::_1));
				connections_.emplace(con->id(),con);
				if (!con->start())
				{
					connections_.erase(con->id());
				}
			}
			else
			{
				con = itr->second;
			}
		}

		if (con)
		{
			con->on_message(msg);
		}

	}
	udp_socket_->async_receive_from(asio::buffer(udp_recv_buffer_, udp_recv_buffer_.size()), udp_endpoint_,
		std::bind(&sip_server::handle_read, this, self, udp_socket_,std::placeholders::_1, std::placeholders::_2));


}

void sip_server::handle_timer(sip_server_ptr svr, const std::error_code& ec)
{
	if (ec)
		return;

	std::vector<sip_connection_ptr> cons;
	all_connections(cons);
	for (auto itr = cons.begin(); itr != cons.end(); itr++)
	{
		if ((*itr)->is_timeout())
		{
			(*itr)->invoke_close();
		}
	}

	timer_.expires_after(std::chrono::milliseconds(20000));
	timer_.async_wait(std::bind(&sip_server::handle_timer, this, svr, std::placeholders::_1));
}

sip_connection_ptr sip_server::create_connection(const voip_uri& url, bool tcp)
{
	if (url.host.empty())
		return nullptr;

	int port = url.port != 0 ? url.port : 5060;


	std::error_code ec;
	auto addr=asio::ip::address::from_string(url.host, ec);
	if (ec)
		return nullptr;

	if (tcp)
	{
		asio::ip::tcp::endpoint ep(addr,port);
		auto con = std::make_shared<sip_tcp_connection>(ioc_, ep);
		add_connection(con);
		return con;
	}
	else
	{
		asio::ip::udp::endpoint ep(addr, port);
		auto con = std::make_shared<sip_udp_connection>(ioc_, udp_socket_,ep);
		add_connection(con);
		return con;
	}
}

bool sip_server::add_connection(sip_connection_ptr con)
{
	auto self = shared_from_this();
	con->set_logger(log_);
	con->set_message_handler(std::bind(&sip_server::handle_message, this, self, std::placeholders::_1, std::placeholders::_2));
	con->set_remove_handler(std::bind(&sip_server::handle_remove, this, self, std::placeholders::_1));

	sip_connection_ptr old;
	{
		std::lock_guard<std::mutex> lk(connections_mutex_);
		auto itr = connections_.emplace(con->id(), con);
		if (!itr.second)
		{
			if (itr.first->second) {
				old=itr.first->second;
			}
			itr.first->second = con;
		}
	}
	if (old)
	{
		old->stop();
	}
	return true;
}

void sip_server::remove_connection(const std::string& id)
{
	sip_connection_ptr con;
	{
		std::lock_guard<std::mutex> lk(connections_mutex_);
		auto itr=connections_.find(id);
		if (itr != connections_.end())
		{
			con = itr->second;
			connections_.erase(itr);
		}
	}

	if (con)
	{
		if (close_con_handler_)
		{
			auto self = shared_from_this();
			close_con_handler_(self, con);
		}
	}
}

void sip_server::all_connections(std::vector<sip_connection_ptr>& cons)
{
	std::lock_guard<std::mutex> lk(connections_mutex_);
	cons.reserve(connections_.size());
	for (auto itr = connections_.begin(); itr != connections_.end(); itr++)
	{
		cons.push_back(itr->second);
	}
}

void sip_server::clear_connections()
{
	std::map<std::string, sip_connection_ptr> cons;
	{
		std::lock_guard<std::mutex> lk(connections_mutex_);
		cons = connections_;
		connections_.clear();
	}
}





