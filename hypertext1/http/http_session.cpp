#include "http_session.h"
#include "http_server.h"
#include <sys2/string_util.h>

http_session::http_session(http_server* server,spdlogger_ptr log, sys::socket_ptr socket)
{
	server_ = server;
	log_ = log;
	socket_ = socket;

	id_ = socket->handle();
}

http_session::http_session(http_server* server, spdlogger_ptr log, sys::ssl_socket_ptr socket)
{
	server_ = server;
	log_ = log;
	ssl_socket_ = socket;

	id_ = socket->socket()->handle();
}

http_session::~http_session()
{
	close();
}

SOCKET http_session::id()const
{
	return id_;
}

bool http_session::is_timeout()const
{
	time_t now = time(nullptr);
	time_t sec = now - updated_time_;
	return sec >= 180;
}

bool http_session::open()
{
	updated_time_ = time(nullptr);
	worker_ = new std::thread(&http_session::run, this);
	return true;
}

void http_session::close()
{
	active_ = false;
	if (socket_) {
		socket_->close();
	}
	if (worker_)
	{
		worker_->join();
		delete worker_;
		worker_ = nullptr;
	}

	socket_.reset();
	ssl_socket_.reset();
	if (log_) {
		log_->debug("HTTP session closed.")->flush();
	}
}

bool http_session::send_response(http_response& response)
{
	std::string data = response.to_string();

	if (socket_)
	{
		int size = socket_->send(data.data(), (int)data.size());
		if (size <= 0)
		{
			return false;
		}
		return true;
	}
	else if (ssl_socket_)
	{
		int size = ssl_socket_->ssl_write(data.data(), (int)data.size());
		if (size <= 0)
		{
			return false;
		}
		return true;
	}
	
	return false;
}

bool http_session::send_response(const std::string& status, const std::string& message, const std::string& content, const std::string& content_type)
{
	http_response rsp;
	rsp.set_status(status);
	rsp.set_message(message);
	rsp.set_content_type(content_type);
	rsp.set_body(content);
	return send_response(rsp);
}

bool http_session::send_response(const http_request& request, const std::string& status, const std::string& message, const std::string& content, const std::string& content_type)
{
	http_response response;
	response.set_status(status);
	response.set_message(message);
	response.set_content_type(content_type);
	response.set_body(content);
	bool ret = send_response(response);
	print_message(request, response);
	return ret;
}

std::string http_session::get_client_address(const http_request& request)
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
		sys::socket_ptr skt = socket_ ? socket_ : ssl_socket_->socket();
		int port = 0;
		skt->remote_addr(address,port);
	}
	return address;
}


void http_session::run()
{
	while (active_)
	{
		std::string line;
		if (!read_line(line))
		{
			goto CLEAR;
		}

		http_request request;
		request.set_first_line(line);


		for (;;)
		{
			line.clear();
			if (!read_line(line))
			{
				goto CLEAR;
			}
			request.set_header_line(line);
			if (line.size() == 0)
			{
				break;
			}
		}

		int64_t content_len = request.content_length();
		if (content_len > 0)
		{
			line.clear();
			if (!read_size(line, (int)content_len))
			{
				goto CLEAR;
			}

			request.set_body(line);
		}
		on_message(request);
	}

CLEAR:
	server_->remove_queue_.push(id());
}


void http_session::on_message(const http_request& request)
{
	updated_time_ = time(nullptr);
	auto sft = shared_from_this();
	server_->session_message_event.invoke(sft, request);
}


bool http_session::read_line(std::string& s)
{
	for (;;)
	{
		char c = 0;
		int r = read_buffer(&c, 1);
		if (r <= 0)
		{
			return false;
		}

		s.append(1, c);

		if (c == '\n')
		{
			break;
		}

	}

	sys::string_util::trim(s);
	return true;
}

bool http_session::read_size(std::string& buffer, int size)
{
	while (size > 0)
	{
		char* tmp = new char[size]();
		int rd= read_buffer(tmp, size);
		if (rd <= 0)
		{
			delete[] tmp;
			return false;
		}

		buffer.append(tmp, rd);
		delete[] tmp;
		size -= rd;
	}

	return true;
}

bool http_session::read_size(char* buffer, int size)
{
	while (size > 0)
	{
		char* tmp = new char[size]();
		int rd = read_buffer(tmp, size);
		if (rd <= 0)
		{
			delete[] tmp;
			return false;
		}

		memcpy(buffer, tmp, rd);
		delete[] tmp;
		size -= rd;
		buffer += rd;
	}

	return true;
}

int http_session::read_buffer(char* buffer, int size)
{
	if (socket_)
	{
		return socket_->recv(buffer, size);
	}
	else if (ssl_socket_)
	{
		return ssl_socket_->ssl_read(buffer, size);
	}
	else
	{
		return -1;
	}
}

void http_session::print_message(const http_request& request, const http_response& response)
{
	if (!log_)
	{
		return;
	}
	std::stringstream log;
	if (log_->level() <= spdlog::level::level_enum::trace)
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

