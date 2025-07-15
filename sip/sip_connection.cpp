#include "sip_connection.h"
#include <sys2/string_util.h>

sip_connection::sip_connection(asio::io_context& ioc)
	:ioc_(ioc)
{

}


sip_connection::~sip_connection()
{
	if (log_) {
		log_->info("SIP connection free. id={}",id())->flush();
	}
}


bool sip_connection::is_timeout()const
{
	time_t now = time(nullptr);
	time_t sec = now - updated_time_;
	return sec >= 180;
}

bool sip_connection::start()
{
	updated_time_ = time(nullptr);
	return true;
}

void sip_connection::stop()
{
	if (remove_handler_)
	{
		auto self = shared_from_this();
		remove_handler_(self);
	}
}

void sip_connection::invoke_close()
{
	if (close_handler_)
	{
		auto self = shared_from_this();
		close_handler_(self);
	}
}

void sip_connection::on_message(const sip_message& message)
{
	if (log_) {
		std::string msg = message.to_string();
		log_->trace("Received sip message:\n{}", msg)->flush();
	}
	updated_time_ = time(nullptr);
	
	if (message_handler_)
	{
		auto self = shared_from_this();
		message_handler_(self, message);
	}
}








