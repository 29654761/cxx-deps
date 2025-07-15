/**
 * @file rtpx_core.h
 * @brief
 * @author Shijie Zhou
 * @copyright 2025 Shijie Zhou
 */

#pragma once
#include <memory>
#include <mutex>
#include <vector>
#include <map>
#include <thread>
#include <asio.hpp>
#include <shared_mutex>
#include <spdlog/spdlogger.hpp>
#include "dtls/dtls.h"
#include "transports/transport.h"
#include "util/port_range.h"
#include "rtp_session.h"

namespace rtpx {
	
	class rtpx_core
	{
	private:
		rtpx_core();
		~rtpx_core();
		static rtpx_core* s_instance;

	public:
		static rtpx_core* instance();
		static void release();

		bool init(size_t work_nums);
		void cleanup();

		rtp_session_ptr create_rtp_session(bool stun=true, bool port_mux = true);


		asio::io_context& ioc_context() { return ioc_; }


		bool set_port_range(uint16_t min_port = 40000, uint16_t max_port = 49999);
		cert_ptr get_cert();
		dtls_ptr get_dtls();
		port_range& ports() { return ports_; }
		
		//asio::udp_socket_ptr create_socket(uint16_t port);
		bool destroy_socket(uint16_t port);
		//asio::udp_socket_ptr get_socket(uint16_t port);

		
		//bool register_media_stream(media_stream_ptr ms);
		//void unregister_media_stream(media_stream_ptr ms);

		//transport_ptr create_transport(uint16_t port, asio::udp_socket_ptr skt, const asio::ip::udp::endpoint& remote_endpoint);
		//transport_ptr create_transport(uint16_t port,transport::send_handler sender, const asio::ip::udp::endpoint& remote_endpoint);

		bool receive_message(const std::string& remote, const uint8_t* buffer, size_t size);
	private:
		void run();
		void handle_timer(const std::error_code& ec);
		//void handle_message(uint16_t local_port,asio::udp_socket_ptr skt, const uint8_t* buffer, size_t size,const asio::ip::udp::endpoint& endpoint);
		//media_stream_ptr find_media_stream(uint32_t ssrc, uint16_t port, const const asio::ip::udp::endpoint& remote,bool is_rtp);
		
		//asio::udp_socket_ptr _create_socket(uint16_t port);
		bool _destroy_socket(uint16_t port);
		void free_idle_sockets();
		void free_transports(uint16_t local_port);
		bool is_port_idle(uint16_t local_port);
		
	private:
		std::shared_mutex mutex_;
		bool active_ = false;
		spdlogger_ptr log_;
		port_range ports_;

		asio::io_context ioc_;
		asio::steady_timer timer_;

		std::vector<std::unique_ptr<std::thread>> workers_;
		/*
		std::map<uint16_t, asio::udp_socket_ptr> sockets_;
		std::map<std::string, transport_ptr> transports_;
		std::map<uint32_t,media_stream_ptr> media_streams_;
		*/
#ifdef RTPX_SSL
		cert_ptr cert_;
#endif
	};
	
}

