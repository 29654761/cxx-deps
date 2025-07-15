#include "asio_udp_socket.h"

namespace asio
{

	udp_socket::udp_socket(asio::ip::udp::socket&& socket, size_t max_sending_queue_size)
		:socket_(std::move(socket))
	{
	}

	udp_socket::~udp_socket()
	{
	}

	void udp_socket::close()
	{
		std::lock_guard<std::mutex> lk(mutex_);
		active_ = false;
		std::error_code ec;
		socket_.cancel(ec);
		this->bind_data_received_handler(nullptr);
	}

	bool udp_socket::send_to(const uint8_t* message, size_t size, const asio::ip::udp::endpoint& endpoint)
	{
		std::lock_guard<std::mutex> lk(mutex_);
		size_t qsize = queue_.size();
		if (qsize >= max_sending_queue_size_)
		{
			return false;
		}

		item_t item;
		item.ep = endpoint;
		item.msg.assign(message, message + size);

		queue_.push(std::move(item));


		if (qsize == 0)
		{
			const item_t& front = queue_.front();
			auto self = shared_from_this();
			socket_.async_send_to(asio::buffer(front.msg), front.ep,
				std::bind(&udp_socket::handle_write, self, std::placeholders::_1, std::placeholders::_2));
		}
		return true;
	}

	void udp_socket::post_read()
	{
		auto self = shared_from_this();
		auto ep = std::make_shared<asio::ip::udp::endpoint>();
		socket_.async_receive_from(asio::buffer(buffer_), *ep.get(),
			std::bind(&udp_socket::handle_read, self, ep, std::placeholders::_1, std::placeholders::_2));

	}

	void udp_socket::clear()
	{
		std::lock_guard<std::mutex> lk(mutex_);
		while (!queue_.empty())
		{
			queue_.pop();
		}
	}

	void udp_socket::handle_write(const asio::error_code& ec, std::size_t bytes)
	{
		if (!active_)
		{
			clear();
			return;
		}
		std::lock_guard<std::mutex> lk(mutex_);
		queue_.pop();
		if (!queue_.empty())
		{
			auto& front = queue_.front();
			auto self = shared_from_this();
			socket_.async_send_to(asio::buffer(front.msg), front.ep,
				std::bind(&udp_socket::handle_write, self, std::placeholders::_1, std::placeholders::_2));
		}

	}

	

	void udp_socket::handle_read(std::shared_ptr<asio::ip::udp::endpoint> ep, const asio::error_code& ec, std::size_t bytes)
	{
		if (!active_) {
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			return;
		}

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
			handler(self, buffer_.data(), bytes, *ep.get());
		}

		socket_.async_receive_from(asio::buffer(buffer_), *ep.get(),
			std::bind(&udp_socket::handle_read, self, ep, std::placeholders::_1, std::placeholders::_2));
	}

}
