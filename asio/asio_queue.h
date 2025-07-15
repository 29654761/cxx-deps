#pragma once
#include <asio.hpp>

#include <mutex>
#include <memory>
#include <queue>
#include <functional>
#include <spdlog/spdlogger.hpp>

namespace asio
{
	class asio_queue;
	typedef std::shared_ptr<asio_queue> asio_queue_ptr;

	class asio_queue :public std::enable_shared_from_this<asio_queue>
	{
	public:
		typedef std::function<void()> handler_t;
		asio_queue(asio::io_context& ioc, size_t max_size = 1000);
		~asio_queue();

		bool post(handler_t handler);

		void clear();

	private:
		void handle_post(asio_queue_ptr self);

	private:
		asio::io_context& ioc_;
		size_t max_size_ = 1000;
		std::mutex mutex_;
		std::queue<handler_t> queue_;
	};



}