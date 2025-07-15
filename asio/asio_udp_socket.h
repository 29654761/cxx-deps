#pragma once
#include <asio.hpp>
#include <mutex>
#include <shared_mutex>
#include <memory>
#include <queue>
#include <vector>
#include <functional>

namespace asio
{
	class udp_socket;
	typedef std::shared_ptr<udp_socket> udp_socket_ptr;

	class udp_socket :public std::enable_shared_from_this<udp_socket>
	{
		struct item_t
		{
			std::vector<uint8_t> msg;
			asio::ip::udp::endpoint ep;
		};
	public:
		typedef std::function<void(udp_socket_ptr, const uint8_t*, size_t, const asio::ip::udp::endpoint&)> data_received_handler;
		udp_socket(asio::ip::udp::socket&& socket, size_t max_sending_queue_size = 5000);
		~udp_socket();

		void bind_data_received_handler(data_received_handler handler) { 
			std::unique_lock<std::shared_mutex> lk(handler_mutex_);
			data_received_handler_ = handler;
		}
		void close();
		bool send_to(const uint8_t* message, size_t size, const asio::ip::udp::endpoint& endpoint);
		void post_read();
	private:
		// You can't clear the queue when sending is pending.
		void clear();
		void handle_write(const asio::error_code& ec, std::size_t bytes);
		void handle_read(std::shared_ptr<asio::ip::udp::endpoint> ep, const asio::error_code& ec, std::size_t bytes);
	private:
		std::mutex mutex_;
		bool active_=true;
		asio::ip::udp::socket socket_;
		std::queue<item_t> queue_;
		size_t max_sending_queue_size_ = 5000;
		std::array<uint8_t, 2048> buffer_;

		std::shared_mutex handler_mutex_;
		data_received_handler data_received_handler_;
	};

}

