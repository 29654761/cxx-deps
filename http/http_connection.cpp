#include "http_connection.h"
#include <sys2/string_util.h>

http_connection::http_connection(asio::io_context& ioc,socket_ptr socket)
	:recv_ring_buffer_(102400)
{
	socket_ =socket;
	recv_buffer_.fill(0);
	sending_queue_ = std::make_shared<asio::sending_queue>();
}

http_connection::http_connection(asio::io_context& ioc, ssl_socket_ptr socket)
	:recv_ring_buffer_(102400)
{
	ssl_socket_ = socket;
	recv_buffer_.fill(0);
	sending_queue_ = std::make_shared<asio::sending_queue>();
}

http_connection::~http_connection()
{
}

bool http_connection::is_timeout()const
{
	time_t now = time(nullptr);
	time_t sec = now - updated_time_;
	return sec >= 60;
}

bool http_connection::start()
{
	updated_time_ = time(nullptr);

	auto self = shared_from_this();
	if (socket_)
	{
		socket_->async_read_some(asio::buffer(recv_buffer_), std::bind(&http_connection::handle_read, this, self, std::placeholders::_1, std::placeholders::_2));
	}
	else if (ssl_socket_)
	{
		ssl_socket_->async_handshake(asio::ssl::stream_base::server, std::bind(&http_connection::handle_handshake, this, self, std::placeholders::_1));
	}
	else
	{
		stop();
		return false;
	}

	return true;
}

void http_connection::stop()
{
	asio::error_code ec;
	if (socket_) {
		socket_->cancel(ec);
	}
	if (ssl_socket_) {
		ssl_socket_->shutdown(ec);
		ssl_socket_->lowest_layer().cancel(ec);
	}
	if (log_) {
		log_->debug("HTTP session closed.")->flush();
	}
	if (close_handler_)
	{
		auto self = shared_from_this();
		close_handler_(self);
	}
}

bool http_connection::async_read()
{
	if (socket_)
	{
		socket_->async_read_some(asio::buffer(recv_buffer_), std::bind(&http_connection::handle_read, this, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
		return true;
	}
	else if (ssl_socket_)
	{
		ssl_socket_->async_read_some(asio::buffer(recv_buffer_), std::bind(&http_connection::handle_read, this, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
		return true;
	}
	else
	{
		return false;
	}
}


bool http_connection::send_response(http_response& response)
{
	std::string data = response.to_string();

	if (socket_)
	{
		return sending_queue_->send(socket_,data);
	}
	else if (ssl_socket_)
	{
		return sending_queue_->send(ssl_socket_, data);
	}
	else
	{
		return false;
	}

}

bool http_connection::send_response(const std::string& status, const std::string& message, const std::string& content, const std::string& content_type)
{
	http_response rsp;
	rsp.set_status(status);
	rsp.set_msg(message);
	rsp.set_content_type(content_type);
	rsp.set_body(content);
	return send_response(rsp);
}

bool http_connection::send_response(const http_request& request, const std::string& status, const std::string& message, const std::string& content, const std::string& content_type)
{
	http_response response;
	response.set_status(status);
	response.set_msg(message);
	response.set_content_type(content_type);
	response.set_body(content);
	bool ret = send_response(response);
	print_message(request, response);
	return ret;
}

std::string http_connection::get_client_address(const http_request& request)
{
	std::string address;
	std::string forwards;
	if (request.headers.get("X-Forwarded-For", forwards))
	{
		std::vector<std::string> vec=sys::string_util::split(forwards, ",");
		address = vec[0];
	}
	else
	{
		if (socket_)
		{
			asio::error_code ec;
			address = socket_->remote_endpoint().address().to_string(ec);
		}
		else if (ssl_socket_)
		{
			asio::error_code ec;
			address = ssl_socket_->lowest_layer().remote_endpoint().address().to_string(ec);
		}
		
	}
	return address;
}

void http_connection::handle_handshake(http_connection_ptr con, const asio::error_code& ec)
{
	auto self = shared_from_this();
	if (ec || !ssl_socket_)
	{
		stop();
		return;
	}
	ssl_socket_->async_read_some(asio::buffer(recv_buffer_), std::bind(&http_connection::handle_read, this, self, std::placeholders::_1, std::placeholders::_2));
}

void http_connection::handle_read(http_connection_ptr con, const asio::error_code& ec, std::size_t bytes)
{
	if (ec)
	{
		stop();
		return;
	}
	updated_time_ = time(nullptr);

	if (!recv_ring_buffer_.write(recv_buffer_.data(), bytes))
	{
		stop();
		return;
	}
	size_t rdsize=recv_ring_buffer_.available_to_read();
	std::unique_ptr<char[], std::function<void(char*)>> buf(new char[rdsize], [](char* ptr) {
		delete[] ptr;
		});

	if (!recv_ring_buffer_.peek(buf.get(), rdsize))
		return;

	int64_t pos = 0;
	while (rdsize > 0)
	{
		http_request request;
		int64_t r= request.parse(buf.get()+pos, rdsize);
		if (r < 0)
			return;
		if (r == 0)
			break;

		if (message_handler_)
		{
			message_handler_(con, request);
		}

		recv_ring_buffer_.remove((size_t)r);
		pos += r;
		rdsize -= (size_t)r;
	}
	
	async_read();
}



void http_connection::print_message(const http_request& request, const http_response& response)
{
	if (!log_)
	{
		return;
	}
	std::stringstream log;
	if (log_->level() <= log_level_t::trace)
	{
		log << "HTTP Request:" << std::endl;
		log << request.to_string();
		log << std::endl;

		log << "HTTP Response:" << std::endl;
		log << response.to_string();
		log << std::endl;

		log_->trace(log.str())->flush();
	}
}

