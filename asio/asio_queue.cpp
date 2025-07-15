#include "asio_queue.h"

namespace asio
{
	asio_queue::asio_queue(asio::io_context& ioc, size_t max_size)
		:ioc_(ioc)
	{
		max_size_ = max_size;
	}
	asio_queue::~asio_queue()
	{
	}
	bool asio_queue::post(handler_t handler)
	{
		std::lock_guard<std::mutex> lk(mutex_);
		size_t qsize = queue_.size();
		if (qsize >= max_size_)
		{
			return false;
		}

		queue_.push(handler);

		if (qsize == 0)
		{
			auto self = shared_from_this();
			ioc_.post(std::bind(&asio_queue::handle_post, this, self));
		}
		return true;
	}
	void asio_queue::clear() {
		std::lock_guard<std::mutex> lk(mutex_);
		while (!queue_.empty())
		{
			queue_.pop();
		}
	}

	void asio_queue::handle_post(asio_queue_ptr self)
	{
		std::lock_guard<std::mutex> lk(mutex_);
		if (queue_.empty())
			return;

		auto& front = queue_.front();
		front();
		queue_.pop();

		if (queue_.empty())
			return;

		ioc_.post(std::bind(&asio_queue::handle_post, this, self));
	}

}