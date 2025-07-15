#include "rtsp_connection.h"
#include <sys2/util.h>
#include <sys2/string_util.h>
#include <sys2/socket.h>

rtsp_connection::rtsp_connection(tcp_socket_ptr socket)
	:recv_ring_buffer_(102400)
{
	socket_ = socket;
	updated_time_ = time(nullptr);
	id_ = sys::util::uuid();
	sending_queue_ = std::make_shared<asio::sending_queue>();
}

rtsp_connection::~rtsp_connection()
{
	if (log_)
	{
		std::string url = url_.to_string();
		log_->debug("RTSP connection free, url={}", url)->flush();
	}
}

const std::string& rtsp_connection::id()const
{
	return id_;
}

bool rtsp_connection::is_timeout()const
{
	time_t now = time(nullptr);
	time_t sec = now - updated_time_;
	return sec >= 180;
}

bool rtsp_connection::start()
{
	std::lock_guard<std::mutex> lk(mutex_);
	if (active_)
	{
		return true;
	}
	active_ = true;

	updated_time_ = time(nullptr);
	sending_queue_->clear();
	auto self = shared_from_this();
	socket_->async_read_some(asio::buffer(recv_buffer_), std::bind(&rtsp_connection::handle_read, this, self, std::placeholders::_1, std::placeholders::_2));
	return true;
}

void rtsp_connection::close()
{
	active_ = false;
	std::error_code ec;
	socket_->cancel(ec);

	if (log_) {
		std::string url = url_.to_string();
		log_->debug("Rtsp session closed.id={}, url={}", id_,url)->flush();
	}

	if (on_destroy)
	{
		auto self = shared_from_this();
		on_destroy(id());
	}
}

bool rtsp_connection::send_response(rtsp_response& response)
{
	std::lock_guard<std::mutex> lk(mutex_);
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

	return sending_queue_->send(socket_, data);
}



hypertext::uri rtsp_connection::url()
{
	std::lock_guard<std::mutex> lk(mutex_);
	return url_;
}
void rtsp_connection::set_url(const std::string& url)
{
	std::lock_guard<std::mutex> lk(mutex_);
	url_.parse(url);
}

void rtsp_connection::handle_read(rtsp_connection_ptr con, const std::error_code& ec, std::size_t bytes)
{
	if (ec)
	{
		close();
		return;
	}
	if (!recv_ring_buffer_.write(recv_buffer_.data(), bytes))
	{
		close();
		return;
	}
	size_t rdsize =recv_ring_buffer_.available_to_read();
	if (rdsize == 0)
	{
		close();
		return;
	}
	std::unique_ptr<char[], std::function<void(char*)>> buf(new char[rdsize], [](char* ptr) {
		delete[] ptr;
		});

	if (!recv_ring_buffer_.peek(buf.get(), rdsize))
		return;

	char packet[2048] = { 0 };

	int64_t pos = 0;
	while (rdsize > 0)
	{
		char magic = buf[pos];
		if (magic == '$')
		{
			if (rdsize <= 4)
				break;

			char channel = buf[pos + 1];

			uint16_t len = 0;
			memcpy(&len, buf.get() + pos + 2, 2);
			len = sys::socket::ntoh16(len);
			if (len > rdsize-pos-4)
				break;
			if (len >= 2048)
				return;

			updated_time_ = time(nullptr);
			memcpy(packet,buf.get()+pos+4,len);

			int64_t r = pos + 4 + len;
			recv_ring_buffer_.remove(r);
			pos += r;
			rdsize -= (size_t)r;
		}
		else
		{
			rtsp_request request;
			int64_t r = request.parse(buf.get() + pos, rdsize);
			if (r < 0)
				return;
			if (r == 0)
				break;

			on_message(request);

			recv_ring_buffer_.remove(r);
			pos += r;
			rdsize -= (size_t)r;

		}
	}


	if (active_)
	{
		socket_->async_read_some(asio::buffer(recv_buffer_), std::bind(&rtsp_connection::handle_read, this, con, std::placeholders::_1, std::placeholders::_2));
	}
}



void rtsp_connection::on_message(rtsp_request& request)
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
		on_message_teardown(request);
		close();
	}
	else
	{
		rtsp_response response;
		response.set_cseq(request.cseq());
		response.set_status("405");
		response.set_msg("Method Not Allowed");
		send_response(response);
		print_message(request, response);
	}
}

void rtsp_connection::on_message_options(const rtsp_request& request)
{
	set_url(request.url());
}

void rtsp_connection::on_message_describe(const rtsp_request& request)
{

}

void rtsp_connection::on_message_setup(const rtsp_request& request)
{

}

void rtsp_connection::on_message_announce(const rtsp_request& request)
{

}

void rtsp_connection::on_message_play(const rtsp_request& request)
{

}

void rtsp_connection::on_message_pause(const rtsp_request& request)
{

}

void rtsp_connection::on_message_record(const rtsp_request& request)
{

}

void rtsp_connection::on_message_teardown(const rtsp_request& request)
{
}


void rtsp_connection::print_message(const rtsp_request& request, const rtsp_response& response)
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

