#pragma once
#include <memory>
#include <queue>
#include <vector>
#include <functional>
#include <asio.hpp>
#include <shared_mutex>

namespace asio
{
	class tcp_socket;
	typedef std::shared_ptr<tcp_socket> tcp_socket_ptr;

	class tcp_socket:public std::enable_shared_from_this<tcp_socket>
	{
		struct item_t
		{
			std::vector<uint8_t> msg;
		};
	public:
		typedef std::function<void(tcp_socket_ptr, const uint8_t*, size_t)> data_received_handler;
		tcp_socket(asio::ip::tcp::socket&& socket, size_t max_sending_queue_size = 5000);
		~tcp_socket();

		void bind_data_received_handler(data_received_handler handler) { 
			std::unique_lock<std::shared_mutex> lk(handler_mutex_);
			data_received_handler_ = handler; 
		}

		void close();
		bool send(const uint8_t* message, size_t size);
		void post_read();

	private:
		// You can't clear the queue when sending is pending.
		void clear();
		void handle_write(const asio::error_code& ec, std::size_t bytes);
		void handle_read(const asio::error_code& ec, std::size_t bytes);

	private:
		asio::ip::tcp::socket socket_;
		std::mutex mutex_;
		bool active_ = true;
		std::queue<item_t> queue_;
		size_t max_sending_queue_size_ = 5000;
		size_t offset_ = 0;
		std::array<uint8_t, 10240> buffer_;

		std::shared_mutex handler_mutex_;
		data_received_handler data_received_handler_;
	};

}

