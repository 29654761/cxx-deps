/**
 * @file sctp.h
 * @brief
 * @author Shijie Zhou
 * @copyright 2025 Shijie Zhou
 */

#pragma once
#include "sctp_packet.h"
#include "sctp_stream.h"
#include <sys2/callback.hpp>
#include <sys2/socket.h>
#include <sys2/signal.h>
#include <atomic>
#include <memory>
#include <mutex>

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

namespace litertp
{

	enum class sctp_state
	{
		none,
		connecting,
		connected,
		closed,
	};

	class sctp
	{
	public:
		typedef void (*sctp_send_data_t)(void* ctx, const std::vector<uint8_t>& data,const sockaddr* addr,socklen_t addr_size);
		typedef void (*sctp_connected_t)(void* ctx);
		typedef void (*sctp_closed_t)(void* ctx);
		typedef void (*sctp_heartbeat_t)(void* ctx);

		typedef void (*sctp_stream_open_t)(void* ctx,uint16_t stream_id);
		typedef void (*sctp_stream_closed_t)(void* ctx, uint16_t stream_id);
		typedef void (*sctp_stream_message_t)(void* ctx, uint16_t stream_id, const std::vector<uint8_t>& message);
		
		sctp();
		~sctp();

		sctp_state state()const { return state_; }
		uint16_t local_port()const { return local_port_; }

		void set_remote_addr(const sockaddr* addr, socklen_t len);
		//void get_remote_addr(sockaddr_storage* addr) const;
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
		void run_timer();

		bool insert_data_chunk(sctp_chunk_flags flags, const sctp_data_chunk& chunk);
		void set_state(sctp_state state);
		void get_last_state(sctp_state& state, bool& timeout);
		void send_sack(sctp_message& msg);

		uint32_t make_next_stream_id();
		std::shared_ptr<sctp_stream> ensure_stream(uint16_t sid);
	public:
		sys::callback< sctp_connected_t> on_connected;
		sys::callback<sctp_closed_t> on_closed;
		sys::callback<sctp_send_data_t> on_sctp_send_data;
		sys::callback<sctp_heartbeat_t> on_sctp_heartbeat;

		sys::callback<sctp_stream_open_t> on_stream_opened;
		sys::callback<sctp_stream_closed_t> on_stream_closed;
		sys::callback<sctp_stream_message_t> on_stream_message;
	private:
		mutable std::mutex mutex_;
		sockaddr_storage remote_addr_ = {};
		bool active_ = false;
		sys::signal signal_;
		std::unique_ptr<std::thread> timer_;

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

	typedef std::shared_ptr<sctp> sctp_ptr;
}

