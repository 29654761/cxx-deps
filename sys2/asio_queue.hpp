#pragma once
#include <asio.hpp>
#include <string>
#include <mutex>
#include <memory>
#include <queue>
#include <functional>
#include <spdlog/spdlogger.hpp>

namespace asio
{
	class post_queue;
	typedef std::shared_ptr<post_queue> post_queue_ptr;

	class post_queue :public std::enable_shared_from_this<post_queue>
	{
	public:
		typedef std::function<void()> handler_t;
		post_queue(asio::io_context& ioc,size_t max_size = 1000)
			:ioc_(ioc)
		{
			max_size_ = max_size; 
		}
		~post_queue(){}

		bool post(handler_t handler)
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
				ioc_.post(std::bind(&post_queue::handle_post,this, self));
			}
			return true;
		}

		void clear() {
			std::lock_guard<std::mutex> lk(mutex_);
			while (!queue_.empty())
			{
				queue_.pop();
			}
		}

	private:
		void handle_post(post_queue_ptr self)
		{
			std::lock_guard<std::mutex> lk(mutex_);
			if (queue_.empty())
				return;

			auto& front = queue_.front();
			front();
			queue_.pop();

			if (queue_.empty())
				return;

			ioc_.post(std::bind(&post_queue::handle_post, this,self));
		}
	private:
		asio::io_context& ioc_;
		size_t max_size_ = 1000;
		std::mutex mutex_;
		std::queue<handler_t> queue_;
	};

	class sending_queue;
	typedef std::shared_ptr<sending_queue> sending_queue_ptr;

	class sending_queue:public std::enable_shared_from_this<sending_queue>
	{
		struct item_t
		{
			std::string msg;
		};
	public:
		sending_queue(size_t max_size=500) {
			max_size_ = max_size;
		}
		~sending_queue() {
			
		}

		void set_logger(spdlogger_ptr log) { log_ = log; }

		template<typename TSocketPtr>
		bool send(TSocketPtr socket,const std::string& message)
		{
			std::lock_guard<std::mutex> lk(mutex_);
			size_t qsize = queue_.size();
			if (qsize >= max_size_)
			{
				if (log_)
				{
					log_->error("Sending queue size overflow. max={}", max_size_)->flush();
				}
				return false;
			}

			item_t item;
			item.msg = message;

			queue_.push(std::move(item));
			
			if (log_)
			{
				log_->trace("Sending queue pushed. size={}", message.size())->flush();
			}

			if (qsize == 0)
			{
				const item_t& front= queue_.front();
				offset_ = 0;
				auto self = shared_from_this();
				socket->async_write_some(asio::buffer(front.msg), std::bind(&sending_queue::handle_write<TSocketPtr>, this, self,socket, std::placeholders::_1, std::placeholders::_2));

				if (log_)
				{
					log_->debug("Sending queue posted. size={}", front.msg.size())->flush();
				}
			}
			return true;
		}

	private:
		// You can't clear the queue when sending is pending.
		void clear() {
			std::lock_guard<std::mutex> lk(mutex_);
			while (!queue_.empty())
			{
				queue_.pop();
			}
		}
	private:

		template<typename TSocketPtr>
		void handle_write(sending_queue_ptr self,TSocketPtr socket,const asio::error_code& ec, std::size_t bytes)
		{
			if (ec)
			{
				if (log_)
				{
					log_->error("Sending queue error. err={}", ec.message())->flush();
				}
				clear();
				return;
			}
			std::lock_guard<std::mutex> lk(mutex_);
			if (queue_.empty())
			{
				if (log_)
				{
					log_->trace("Sending queue is empty. size={}", bytes)->flush();
				}
				return;
			}
			auto& front = queue_.front();
			offset_ += bytes;
			if (offset_ >= front.msg.size())
			{
				if (log_)
				{
					log_->trace("Sending queue is finished. size={}", offset_)->flush();
				}
				offset_ = 0;
				queue_.pop();
				if (!queue_.empty())
				{
					auto& front2 = queue_.front();
					socket->async_write_some(asio::buffer(front2.msg), std::bind(&sending_queue::handle_write<TSocketPtr>, this, self, socket, std::placeholders::_1, std::placeholders::_2));
					if (log_)
					{
						log_->trace("Sending queue posted again. size={}", front2.msg.size())->flush();
					}
				}
				else
				{
					if (log_)
					{
						log_->trace("Sending queue is empty.")->flush();
					}
				}
			}
			else
			{
				socket->async_write_some(asio::buffer(front.msg.data()+ offset_,front.msg.size()- offset_), std::bind(&sending_queue::handle_write<TSocketPtr>, this, self,socket, std::placeholders::_1, std::placeholders::_2));
				if (log_)
				{
					log_->trace("Sending queue is unfinished, posted again. offset={} size={}", offset_, front.msg.size() - offset_)->flush();
				}
			}
		}

	private:
		std::mutex mutex_;
		std::queue<item_t> queue_;
		size_t max_size_ = 500;
		size_t offset_ = 0;
		spdlogger_ptr log_;
	};


	


}