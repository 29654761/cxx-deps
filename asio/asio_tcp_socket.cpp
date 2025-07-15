#include "asio_tcp_socket.h"

namespace asio
{
	tcp_socket::tcp_socket(asio::ip::tcp::socket&& socket, size_t max_sending_queue_size)
		:socket_(std::move(socket))
	{
		max_sending_queue_size_ = max_sending_queue_size;
	}

	tcp_socket::~tcp_socket()
	{
		close();
	}

	void tcp_socket::close()
	{
		std::lock_guard<std::mutex> lk(mutex_);
		active_ = false;
		std::error_code ec;
		socket_.cancel(ec);
		this->bind_data_received_handler(nullptr);
	}

	bool tcp_socket::send(const uint8_t* message, size_t size)
	{
		std::lock_guard<std::mutex> lk(mutex_);
		size_t qsize = queue_.size();
		if (qsize >= max_sending_queue_size_)
		{
			return false;
		}

		item_t item;
		item.msg.assign(message, message + size);

		queue_.push(std::move(item));

		if (qsize == 0)
		{
			const item_t& front = queue_.front();
			offset_ = 0;
			auto self = shared_from_this();
			socket_.async_write_some(asio::buffer(front.msg), std::bind(&tcp_socket::handle_write, self, std::placeholders::_1, std::placeholders::_2));
		}
		return true;
	}

	void tcp_socket::post_read()
	{
		auto self = shared_from_this();
		socket_.async_read_some(asio::buffer(buffer_),
			std::bind(&tcp_socket::handle_read, self, std::placeholders::_1, std::placeholders::_2));
	}


	void tcp_socket::clear()
	{
		std::lock_guard<std::mutex> lk(mutex_);
		while (!queue_.empty())
		{
			queue_.pop();
		}
	}

	void tcp_socket::handle_write(const asio::error_code& ec, std::size_t bytes)
	{
		if (!active_||ec)
		{
			clear();
			return;
		}
		std::lock_guard<std::mutex> lk(mutex_);
		if (queue_.empty())
		{
			return;
		}
		auto& front = queue_.front();
		offset_ += bytes;
		if (offset_ >= front.msg.size())
		{
			offset_ = 0;
			queue_.pop();
			if (!queue_.empty())
			{
				auto& front2 = queue_.front();
				auto self = shared_from_this();
				socket_.async_write_some(asio::buffer(front2.msg),
					std::bind(&tcp_socket::handle_write, self, std::placeholders::_1, std::placeholders::_2));
			}
		}
		else
		{
			auto self = shared_from_this();
			socket_.async_write_some(asio::buffer(front.msg.data() + offset_, front.msg.size() - offset_), std::bind(&tcp_socket::handle_write, self, std::placeholders::_1, std::placeholders::_2));

		}
	}

	void tcp_socket::handle_read(const asio::error_code& ec, std::size_t bytes)
	{
		if (!active_||ec)
			return;


		auto self = shared_from_this();

		data_received_handler handler;
		{
			std::shared_lock<std::shared_mutex> lk(handler_mutex_);
			if (data_received_handler_)
			{
				handler = data_received_handler_;
			}
		}
		if (handler)
		{
			handler(self, buffer_.data(), bytes);
		}

		socket_.async_read_some(asio::buffer(buffer_),
			std::bind(&tcp_socket::handle_read, self, std::placeholders::_1, std::placeholders::_2));
	}

}
