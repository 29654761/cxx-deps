/**
 * @file sctp.h
 * @brief
 * @author Shijie Zhou
 * @copyright 2025 Shijie Zhou
 */

#pragma once
#include "sctp_packet.h"
#include "sctp_stream.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <asio.hpp>

/*
Client                                     Server
  |-------- INIT ------------------------->|
  |<------- INIT ACK ----------------------|
  |-------- COOKIE ECHO ------------------>|
  |<------- COOKIE ACK --------------------|
  |                                        |
  |-------- DATA ------------------------->|
  |<------- SACK --------------------------|
  |-------- SHUTDOWN --------------------->|
  |<------- SHUTDOWN ACK ------------------|
  |-------- SHUTDOWN COMPLETE ------------>|
*/

namespace rtpx
{

	enum class sctp_state
	{
		none,
		connecting,
		connected,
		closed,
	};

	class sctp;
	typedef std::shared_ptr<sctp> sctp_ptr;

	class sctp:public std::enable_shared_from_this<sctp>
	{
	public:
		typedef std::function<void(const std::vector<uint8_t>& data,const asio::ip::udp::endpoint& endpoint)> send_data_handler;
		typedef std::function<void()> connected_handler;
		typedef std::function<void()> disconnected_handler;
		typedef std::function<void()> heartbeat_handler;
		typedef std::function<void(uint16_t stream_id)> stream_opened_handler;
		typedef std::function<void(uint16_t stream_id)> stream_closed_handler;
		typedef std::function<void(uint16_t stream_id, const std::vector<uint8_t>& message)> stream_message_handler;
		
		sctp(asio::io_context& ioc);
		~sctp();


		void bind_connected_handler(connected_handler handler);
		void bind_disconnected_handler(disconnected_handler handler);
		void bind_send_data_handler(send_data_handler handler);
		void bind_heartbeat_handler(heartbeat_handler handler);
		void bind_stream_opened_handler(stream_opened_handler handler);
		void bind_stream_closed_handler(stream_closed_handler handler);
		void bind_stream_message_handler(stream_message_handler handler);

		sctp_state state()const { return state_; }
		uint16_t local_port()const { return local_port_; }

		void set_remote_endpoint(const asio::ip::udp::endpoint& endpoint);
		void set_init_stream_id(uint16_t sid);

		bool open(uint16_t local_port);
		bool connect(uint16_t remote_port);
		void close();

		void conn_input(const uint8_t* data,size_t size);


		bool create_stream(const std::string& label,uint16_t& sid);
		void close_stream(uint16_t sid);
		bool send_user_message(uint16_t sid, const std::vector<uint8_t>& data);
	private:
		void on_chunk(const sctp_header& header, const sctp_chunk& chunk);
		void on_init(const sctp_header& header, const sctp_chunk& chunk);
		void on_init_ack(const sctp_header& header, const sctp_chunk& chunk);
		void on_cookie_echo(const sctp_header& header, const sctp_chunk& chunk);
		void on_cookie_ack(const sctp_header& header, const sctp_chunk& chunk);
		void on_data(const sctp_header& header, const sctp_chunk& chunk);
		void on_sack(const sctp_header& header, const sctp_chunk& chunk);
		void on_heartbeat(const sctp_header& header, const sctp_chunk& chunk);
		void on_heartbeat_ack(const sctp_header& header, const sctp_chunk& chunk);
		void on_abort(const sctp_header& header, const sctp_chunk& chunk);
		void on_shutdown(const sctp_header& header, const sctp_chunk& chunk);
		void on_shutdown_ack(const sctp_header& header, const sctp_chunk& chunk);
		void on_operational_error(const sctp_header& header, const sctp_chunk& chunk);

		void on_user_message(std::shared_ptr<sctp_stream> stream,const sctp_message& msg);
		void on_dcep_message(std::shared_ptr<sctp_stream> stream, const sctp_message& msg);

		bool send_data(const std::vector<sctp_chunk>& chunks);
		bool send_data(const sctp_chunk& chunk);
		bool send_data(sctp_chunk_type type, sctp_chunk_flags flags, const std::vector<uint8_t>& payload);
		void on_state_timer(const std::error_code& ec);
		void on_ack_timer(const std::error_code& ec);
		bool insert_data_chunk(sctp_chunk_flags flags, const sctp_data_chunk& chunk);
		void set_state(sctp_state state);
		void get_last_state(sctp_state& state, bool& timeout);
		void send_sack(sctp_message& msg);

		uint32_t make_next_stream_id();
		std::shared_ptr<sctp_stream> ensure_stream(uint16_t sid);
	private:
		connected_handler on_connected_;
		disconnected_handler on_disconnected_;
		send_data_handler on_send_data_;
		heartbeat_handler on_heartbeat_;
		stream_opened_handler on_stream_opened_;
		stream_closed_handler on_stream_closed_;
		stream_message_handler on_stream_message_;

		asio::io_context& ioc_;
		asio::steady_timer state_timer_;
		asio::steady_timer ack_timer_;
		mutable std::recursive_mutex mutex_;
		asio::ip::udp::endpoint remote_endpoint_ = {};
		bool active_ = false;
		

		sctp_state state_ = sctp_state::none;
		int64_t state_updated_at_ = 0;

		uint16_t local_port_ = 0;
		uint16_t remote_port_ = 0;

		uint32_t local_tag_ = 0;
		uint32_t remote_tag_ = 0;

		uint32_t local_arwnd_ = 0x00100000; //1MB
		uint32_t remote_arwnd = 0;

		uint32_t local_tsn_ = 0;
		uint32_t remote_init_tsn_ = 0;
		
		std::vector<uint8_t> local_cookie_;
		std::vector<uint8_t> remote_cookie_;

		uint16_t init_stream_id_ = 0;
		std::map<uint16_t, std::shared_ptr<sctp_stream>> streams_;
	};

	
}

