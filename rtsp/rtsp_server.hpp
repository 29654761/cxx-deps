#pragma once

#include "rtsp_connection.h"

#include <sys2/socket.h>
#include <sys2/blocking_queue.hpp>
#include <sys2/signal.h>
#include <sys2/callback.hpp>
#include <spdlog/spdlogger.hpp>
#include <future>
#include <map>
#include <shared_mutex>



template<class TConnection>
class rtsp_server:public std::enable_shared_from_this<rtsp_server<TConnection>>
{
public:
	typedef std::shared_ptr<TConnection> TConnectionPtr;
	typedef std::function<void(std::shared_ptr<rtsp_server<TConnection>>,TConnectionPtr)> on_create_connsecion_t;
	typedef std::function<void(std::shared_ptr<rtsp_server<TConnection>>,TConnectionPtr)> on_destroy_connection_t;

	rtsp_server(asio::io_context& ioc)
		:acceptor_(ioc)
		, timer_(ioc)
	{

	}

	~rtsp_server()
	{
	}

	void set_logger(spdlogger_ptr log){ log_ = log; }
	void set_on_create_connsecion(on_create_connsecion_t handler) { on_create_connection = handler; }
	void set_on_destroy_connection(on_destroy_connection_t handler) { on_destroy_connection = handler; }

	bool start(int port)
	{
		if (active_)
		{
			return false;
		}
		active_ = true;
		std::error_code ec;
		asio::ip::tcp::endpoint ep(asio::ip::tcp::v4(), port);

		acceptor_.open(ep.protocol(),ec);
		if (ec)
		{
			stop();
			return false;
		}

		acceptor_.set_option(asio::socket_base::reuse_address(true), ec);
		acceptor_.bind(ep,ec);
		if (ec)
		{
			stop();
			return false;
		}
		acceptor_.listen(asio::socket_base::max_listen_connections, ec);
		if (ec)
		{
			stop();
			return false;
		}

		auto self = shared_from_this();
		acceptor_.async_accept(std::bind(&rtsp_server<TConnection>::handle_accept, this,self, std::placeholders::_1, std::placeholders::_2));
		timer_.expires_after(std::chrono::seconds(30));
		timer_.async_wait(std::bind(&rtsp_server<TConnection>::handle_timer, this, self, std::placeholders::_1));
		return true;

	}
	void stop()
	{
		active_ = false;
		std::error_code ec;
		acceptor_.cancel(ec);
		timer_.cancel(ec);
		
		std::vector<TConnectionPtr> cons;
		all_connections(cons);

		for (auto itr = cons.begin(); itr != cons.end(); itr++)
		{
			(*itr)->close();
		}
	}

private:

	void handle_accept(std::shared_ptr<rtsp_server<TConnection>> self,const std::error_code& ec,asio::ip::tcp::socket socket)
	{
		if (ec == asio::error::operation_aborted)
		{
			return;
		}

		auto sktptr = std::make_shared<asio::ip::tcp::socket>(std::move(socket));
		auto con = std::make_shared<TConnection>(sktptr);
		add_connection(con);

		if (active_)
		{
			acceptor_.async_accept(std::bind(&rtsp_server<TConnection>::handle_accept, this,self, std::placeholders::_1, std::placeholders::_2));
		}
	}

	void handle_destroy_connection(std::shared_ptr<rtsp_server<TConnection>> self, const std::string& id)
	{
		remove_connection(id);
	}

	void handle_timer(std::shared_ptr<rtsp_server<TConnection>> self, const std::error_code& ec)
	{
		if (ec)
			return;

		std::vector<TConnectionPtr> cons;
		all_connections(cons);

		for (auto itr = cons.begin(); itr != cons.end(); itr++)
		{
			if ((*itr)->is_timeout())
			{
				(*itr)->close();
			}
		}

		if (active_)
		{
			timer_.expires_after(std::chrono::seconds(30));
			timer_.async_wait(std::bind(&rtsp_server<TConnection>::handle_timer, this, self, std::placeholders::_1));
		}
	}
	

	bool add_connection(TConnectionPtr con)
	{
		auto self = shared_from_this();
		{
			std::unique_lock<std::shared_mutex> lk(connections_mutex_);
			for (auto itr = connections_.begin(); itr != connections_.end(); itr++)
			{
				if ((*itr) == con)
					return false;
			}
			con->set_logger(log_);
			con->set_on_destroy(std::bind(&rtsp_server<TConnection>::handle_destroy_connection, this, self, std::placeholders::_1));
			connections_.push_back(con);
		}
		if (on_create_connection)
		{
			on_create_connection(self,con);
		}
		con->start();
		return true;
	}
	bool remove_connection(const std::string& id)
	{
		TConnectionPtr con;
		{
			std::unique_lock<std::shared_mutex> lk(connections_mutex_);
			for (auto itr = connections_.begin(); itr != connections_.end(); )
			{
				if ((*itr)->id() == id)
				{
					con = *itr;
					itr = connections_.erase(itr);
					break;
				}
				else
				{
					itr++;
				}
			}
		}
		if (!con)
			return false;

		if (on_destroy_connection)
		{
			auto self = shared_from_this();
			on_destroy_connection(self,con);
		}
		return true;
	}

	void clear_connections()
	{
		std::unique_lock<std::shared_mutex> lk(connections_mutex_);
		connections_.clear();
	}
	void all_connections(std::vector<TConnectionPtr>& cons)
	{
		cons.clear();
		std::unique_lock<std::shared_mutex> lk(connections_mutex_);
		cons.reserve(connections_.size());
		for (auto& con : connections_)
		{
			cons.push_back(con);
		}
	}


private:
	spdlogger_ptr log_;
	bool active_ = false;
	asio::ip::tcp::acceptor acceptor_;
	asio::steady_timer timer_;


	std::shared_mutex connections_mutex_;
	std::vector<TConnectionPtr> connections_;


	on_create_connsecion_t on_create_connection;
	on_destroy_connection_t on_destroy_connection;
};

