#include "rtsp_session.h"
#include "rtsp_server.h"
#include <sys2/util.h>
#include <sys2/string_util.h>

rtsp_session::rtsp_session(rtsp_server* server,spdlogger_ptr log, std::shared_ptr<sys::socket> socket)
{
	server_ = server;
	log_ = log;
	socket_ = socket;
	updated_time_ = time(nullptr);
	id_ = sys::util::uuid();
}

rtsp_session::~rtsp_session()
{
}

const std::string& rtsp_session::id()const
{
	return id_;
}

bool rtsp_session::is_timeout()const
{
	time_t now = time(nullptr);
	time_t sec = now - updated_time_;
	return sec >= 180;
}

bool rtsp_session::start()
{
	std::lock_guard<std::recursive_mutex> lk(mutex_);
	if (active_)
	{
		return true;
	}
	active_ = true;

	updated_time_ = time(nullptr);

	send_buf_.clear();
	send_buf_.reset();
	worker_ = new std::thread(&rtsp_session::run, this);
	worker_sender_ = new std::thread(&rtsp_session::run_sender, this);
	return true;
}

void rtsp_session::close()
{
	std::thread* worker = nullptr;
	std::thread* worker_sender = nullptr;

	{
		std::lock_guard<std::recursive_mutex> lk(mutex_);
		active_ = false;
		if (socket_) {
			socket_->close();
		}
		worker = worker_;
		worker_ = nullptr;

		worker_sender = worker_sender_;
		worker_sender_ = nullptr;
		
	}
	if (worker)
	{
		worker->join();
		delete worker;
	}
	send_buf_.close();
	if (worker_sender)
	{
		worker_sender->join();
		delete worker_sender;
	}
	if (log_) {
		std::string url = url_.to_string();
		log_->debug("Rtsp session closed.id={}, url={}", id_,url)->flush();
	}
}

bool rtsp_session::send_response(rtsp_response& response)
{
	std::lock_guard<std::recursive_mutex> lk(mutex_);
	if (!socket_)
	{
		return false;
	}
	if (session_id_.size() > 0)
	{
		response.set_session(session_id_);
	}
	//response.headers.add("Server", "SRTC");
	std::string data = response.to_string();

	send_buf_.push(data);

	/*
	int size=socket_->send(data.data(), (int)data.size());
	if (size <= 0)
	{
		return false;
	}
	*/
	return true;
}



sys::uri rtsp_session::url()
{
	std::lock_guard<std::recursive_mutex> lk(mutex_);
	return url_;
}
void rtsp_session::set_url(const std::string& url)
{
	std::lock_guard<std::recursive_mutex> lk(mutex_);
	url_.parse(url);
}

void rtsp_session::run()
{
	while (active_)
	{
		char magic = 0;
		int rdsize = socket_->recv(&magic, 1);
		if (rdsize<=0)
		{
			goto CLEAR;
		}

		if (magic == '$')
		{
			if (!process_media())
			{
				goto CLEAR;
			}
		}
		else
		{
			std::string line;
			line.append(1, magic);
			if (!read_line(line))
			{
				goto CLEAR;
			}

			rtsp_request request;
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
	}

CLEAR:
	server_->remove_queue_.push(id());
}

void rtsp_session::run_sender()
{
	while (active_)
	{
		std::string buf;
		if (!send_buf_.pop(buf))
		{
			break;
		}

		if (socket_->send(buf.data(), (int)buf.size()) <= 0) {
			break;
		}
	}
}

void rtsp_session::on_message(rtsp_request& request)
{
	updated_time_ = time(nullptr);

	if (request.match_method("OPTIONS"))
	{
		on_message_options(request);
	}
	else if (request.match_method("DESCRIBE"))
	{
		on_message_describe(request);
	}
	else if (request.match_method("SETUP"))
	{
		on_message_setup(request);
	}
	else if (request.match_method("ANNOUNCE"))
	{
		on_message_announce(request);
	}
	else if (request.match_method("PLAY"))
	{
		on_message_play(request);
	}
	else if (request.match_method("PAUSE"))
	{
		on_message_pause(request);
	}
	else if (request.match_method("RECORD"))
	{
		on_message_record(request);
	}
	else if (request.match_method("TEARDOWN"))
	{
		active_ = false;
		on_message_teardown(request);
	}
	else
	{
		rtsp_response response;
		response.set_cseq(request.cseq());
		response.set_status("405");
		response.set_message("Method Not Allowed");
		send_response(response);
		print_message(request, response);
	}
}

void rtsp_session::on_message_options(const rtsp_request& request)
{
	set_url(request.url());
}

void rtsp_session::on_message_describe(const rtsp_request& request)
{

}

void rtsp_session::on_message_setup(const rtsp_request& request)
{

}

void rtsp_session::on_message_announce(const rtsp_request& request)
{

}

void rtsp_session::on_message_play(const rtsp_request& request)
{

}

void rtsp_session::on_message_pause(const rtsp_request& request)
{

}

void rtsp_session::on_message_record(const rtsp_request& request)
{

}

void rtsp_session::on_message_teardown(const rtsp_request& request)
{
}

bool rtsp_session::process_media()
{
	char channel = 0;
	if (!read_size(&channel, 1))
	{
		return false;
	}

	uint16_t len = 0;
	if (!read_size((char*)&len, 2))
	{
		return false;
	}
	len=sys::socket::ntoh16(len);
	if (len >= 2048)
	{
		return false;
	}

	char packet[2048] = { 0 };
	if (!read_size(packet, len))
	{
		return false;
	}


	return true;
}

bool rtsp_session::read_line(std::string& s)
{
	for (;;)
	{
		char c = 0;
		int r = socket_->recv(&c, 1);
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

bool rtsp_session::read_size(std::string& buffer, int size)
{
	while (size > 0)
	{
		char* tmp = new char[size]();
		int rd=socket_->recv(tmp, size);
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

bool rtsp_session::read_size(char* buffer, int size)
{
	while (size > 0)
	{
		char* tmp = new char[size]();
		int rd = socket_->recv(tmp, size);
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

void rtsp_session::print_message(const rtsp_request& request, const rtsp_response& response)
{
	if (!log_) {
		return;
	}
	std::stringstream log;
	if (log_->level() <= spdlog::level::level_enum::debug)
	{
		log << "RTSP Request:" << std::endl;
		log << request.to_string();
		log << std::endl;

		log << "RTSP Response:" << std::endl;
		log << response.to_string();
		log << std::endl;

		log_->debug(log.str())->flush();
	}
}

