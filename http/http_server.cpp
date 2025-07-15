#include "http_server.h"
#include <sys2/util.h>

http_server::http_server(asio::io_context& ioc)
	:io_context_(ioc)
	, timer_(ioc)
{
}

http_server::~http_server()
{
	stop();
}

bool http_server::listen(int port)
{
	acceptor_ = std::make_shared<asio::ip::tcp::acceptor>(io_context_);
	asio::error_code ec;

	acceptor_->open(asio::ip::tcp::v4(), ec);
	if (ec)
	{
		stop();
		return false;
	}
	acceptor_->set_option(asio::socket_base::reuse_address(true), ec);
	acceptor_->bind(asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port),ec);
	if (ec)
	{
		stop();
		return false;
	}
	acceptor_->listen(asio::socket_base::max_listen_connections, ec);
	if (ec)
	{
		stop();
		return false;
	}
	auto self = shared_from_this();
	acceptor_->async_accept(std::bind(&http_server::handle_accepted, this, self, std::placeholders::_1, std::placeholders::_2));

	return true;
}

bool http_server::ssl_listen(int port, std::shared_ptr<asio::ssl::context> ssl)
{
	ssl_context_ = ssl;
	ssl_acceptor_ = std::make_shared<asio::ip::tcp::acceptor>(io_context_);
	asio::error_code ec;
	ssl_acceptor_->open(asio::ip::tcp::v4(), ec);
	if (ec)
	{
		stop();
		return false;
	}

	ssl_acceptor_->bind(asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port), ec);
	if (ec)
	{
		stop();
		return false;
	}
	ssl_acceptor_->listen(asio::socket_base::max_listen_connections, ec);
	if (ec)
	{
		stop();
		return false;
	}

	auto self = shared_from_this();
	ssl_acceptor_->async_accept(std::bind(&http_server::handle_ssl_accepted, this, self, std::placeholders::_1, std::placeholders::_2));
	return true;
}

bool http_server::start()
{
	auto self = shared_from_this();
	timer_.expires_after(std::chrono::seconds(20));
	timer_.async_wait(std::bind(&http_server::handle_timer, this, self, std::placeholders::_1));
	return true;
}

void http_server::stop()
{
	std::error_code ec;
	timer_.cancel(ec);
	if (acceptor_)
	{
		acceptor_->cancel(ec);
	}
	if (ssl_acceptor_)
	{
		ssl_acceptor_->cancel(ec);
	}
	std::vector<http_connection_ptr> cons;
	all_connections(cons);
	for (auto itr = cons.begin(); itr != cons.end(); itr++)
	{
		(*itr)->stop();
	}
	clear_connections();
}


void http_server::handle_accepted(http_server_ptr svr, const asio::error_code& ec, asio::ip::tcp::socket socket)
{
	auto self = this->shared_from_this();
	if (!ec)
	{
		auto sktptr = std::make_shared<asio::ip::tcp::socket>(std::move(socket));
		auto con = std::make_shared<http_connection>(io_context_, sktptr);
		con->set_logger(log_);
		con->set_message_handler(std::bind(&http_server::handle_message, this, self, std::placeholders::_1, std::placeholders::_2));
		con->set_close_handler(std::bind(&http_server::handle_close, this, self, std::placeholders::_1));
		if (con->start())
		{
			add_connection(con);
		}
	}
	else if (ec == asio::error::operation_aborted || ec == asio::error::bad_descriptor)
	{
		return;
	}
	else
	{
		if (log_)
		{
			std::string err=ec.message();
			log_->error("Accept failed:{}", err)->flush();
		}
	}

	acceptor_->async_accept(std::bind(&http_server::handle_accepted, this, self, std::placeholders::_1, std::placeholders::_2));
}

void http_server::handle_ssl_accepted(http_server_ptr svr, const asio::error_code& ec, asio::ip::tcp::socket socket)
{

	auto self = this->shared_from_this();
	if (!ec)
	{
		auto sktptr = std::make_shared<asio::ssl::stream<asio::ip::tcp::socket>>(std::move(socket), *ssl_context_);
		auto con = std::make_shared<http_connection>(io_context_, sktptr);
		con->set_logger(log_);
		con->set_message_handler(std::bind(&http_server::handle_message, this, self, std::placeholders::_1, std::placeholders::_2));
		con->set_close_handler(std::bind(&http_server::handle_close, this, self, std::placeholders::_1));
		if (con->start())
		{
			add_connection(con);
		}
	}
	else if (ec == asio::error::operation_aborted||ec==asio::error::bad_descriptor)
	{
		return;
	}
	else
	{
		if (log_)
		{
			std::string err = ec.message();
			log_->error("SSL Accept failed:{}", err)->flush();
		}
	}

	ssl_acceptor_->async_accept(std::bind(&http_server::handle_ssl_accepted, this, self, std::placeholders::_1, std::placeholders::_2));
}

void http_server::handle_message(http_server_ptr svr,http_connection_ptr con, const http_request& request)
{
	if (message_handler_)
	{
		message_handler_(con, request);
	}
}

void http_server::handle_close(http_server_ptr svr, http_connection_ptr con)
{
	remove_connection(con);
}


void http_server::handle_timer(http_server_ptr svr, const std::error_code& ec)
{
	if (ec)
		return;

	std::vector<http_connection_ptr> cons;
	all_connections(cons);
	for (auto itr = cons.begin(); itr != cons.end(); itr++)
	{
		if ((*itr)->is_timeout())
		{
			(*itr)->stop();
		}
	}

	timer_.expires_after(std::chrono::seconds(20));
	timer_.async_wait(std::bind(&http_server::handle_timer, this, svr, std::placeholders::_1));
}

void http_server::add_connection(http_connection_ptr con)
{
	std::lock_guard<std::mutex> lk(connections_mutex_);
	connections_.push_back(con);
}

void http_server::remove_connection(http_connection_ptr con)
{
	std::lock_guard<std::mutex> lk(connections_mutex_);
	for (auto itr = connections_.begin(); itr != connections_.end();)
	{
		if (*itr == con)
		{
			itr = connections_.erase(itr);
			break;
		}
		else
		{
			itr++;
		}
	}
}

void http_server::all_connections(std::vector<http_connection_ptr>& cons)
{
	std::lock_guard<std::mutex> lk(connections_mutex_);
	cons.reserve(connections_.size());
	for (auto itr = connections_.begin(); itr != connections_.end(); itr++)
		cons.push_back(*itr);
}

void http_server::clear_connections()
{
	std::lock_guard<std::mutex> lk(connections_mutex_);
	connections_.clear();
}


