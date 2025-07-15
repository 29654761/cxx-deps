#pragma once

#include "sip_message.h"
#include <spdlog/spdlogger.hpp>
#include <asio.hpp>
#include <memory>
#include <atomic>
#include <functional>

class sip_connection;
typedef std::shared_ptr<sip_connection> sip_connection_ptr;

typedef std::function<void(sip_connection_ptr, const sip_message&)> message_handler;
typedef std::function<void(sip_connection_ptr)> remove_handler;
typedef std::function<void(sip_connection_ptr)> close_handler;

class sip_connection :public std::enable_shared_from_this<sip_connection>
{
public:
	sip_connection(asio::io_context& ioc);
	virtual ~sip_connection();

	void set_logger(spdlogger_ptr log) { log_ = log; }

	const std::string& id()const { return id_; }

	virtual bool start();
	virtual void stop();
	virtual bool is_timeout()const;
	virtual bool is_tcp()const = 0;
	virtual bool send_message(const sip_message& message) = 0;
	virtual void on_message(const sip_message& message);

	virtual bool remote_address(std::string& ip, int& port)const = 0;
	virtual bool local_address(std::string& ip, int& port)const = 0;

	void set_message_handler(message_handler handler) { message_handler_ = handler; }
	void set_remove_handler(remove_handler handler) { remove_handler_ = handler; }
	void set_close_handler(close_handler handler) { close_handler_=handler; }

	static std::string make_id(const std::string& proto,const std::string& ip, int port)
	{
		return proto+"$" + ip + ":" + std::to_string(port);
	}

	void invoke_close();
protected:
	std::string id_;
	asio::io_context& ioc_;
	spdlogger_ptr log_;
	std::atomic<time_t> updated_time_;
	message_handler message_handler_;
	close_handler close_handler_;
	remove_handler remove_handler_;
};


