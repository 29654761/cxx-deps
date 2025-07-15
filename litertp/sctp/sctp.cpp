/**
 * @file sctp.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2025 Shijie Zhou
 */

#include "sctp.h"
#include <sys2/util.h>

namespace litertp
{

	sctp::sctp()
	{
	}

	sctp::~sctp()
	{
		close();
	}

	void sctp::set_remote_addr(const sockaddr* addr, socklen_t len)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		memcpy(&remote_addr_, addr, len);
	}
	//void sctp::get_remote_addr(sockaddr_storage* addr) const
	//{
	//	std::lock_guard<std::mutex> lock(mutex_);
	//	memcpy(addr,&remote_addr_,sizeof(remote_addr_));
	//}
	void sctp::set_init_stream_id(uint16_t sid)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		init_stream_id_ = sid;
	}



	bool sctp::open(uint16_t local_port)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (active_)
			return true;
		active_ = true;
		state_ = sctp_state::none;
		state_updated_at_ = 0;
		this->local_port_ = local_port;

		std::string uuid = sys::util::uuid();
		local_cookie_.assign(uuid.begin(), uuid.end());
		local_tag_ = sys::util::random_number<uint32_t>(0, 0xFFFFFFFF);
		local_tsn_ = sys::util::random_number<uint32_t>(0, 0xFF000000);

		timer_=std::make_unique<std::thread>(&sctp::run_timer, this);
		sys::util::set_thread_name("sctp_timer", timer_.get());
		return true;
	}
	bool sctp::connect(uint16_t remote_port)
	{
		std::lock_guard<std::mutex> lock(mutex_);

		if (local_port_ == 0)
			return false;

		if (state_ != sctp_state::none)
			return true;

		this->remote_port_ = remote_port;
		this->local_port_ = local_port_;

		streams_.clear();

		sctp_init_chunk init_chunk;
		init_chunk.arwnd = local_arwnd_;
		init_chunk.init_tag = local_tag_;
		init_chunk.init_tsn = local_tsn_;
		init_chunk.num_in_stream = 16;
		init_chunk.num_out_stream = 16;

		auto buf = init_chunk.serialize();
		bool ret = send_data(sctp_chunk_type::init, sctp_chunk_flags::none, buf);
		set_state(sctp_state::connecting);
		return ret;
	}

	void sctp::close()
	{
		std::unique_ptr<std::thread> timer;
		{
			std::lock_guard<std::mutex> lock(mutex_);
			state_ = sctp_state::none;
			state_updated_at_ = 0;
			active_ = false;
			signal_.notify();
			sctp_shutdown_chunk sd_chunk;
			sd_chunk.tsn = local_tsn_;
			auto buf = sd_chunk.serialize();
			send_data(sctp_chunk_type::init, sctp_chunk_flags::none, buf);

			timer = std::move(timer_);

			for (auto itr = streams_.begin(); itr != streams_.end(); itr++)
			{
				itr->second->close(local_tsn_);
			}
			streams_.clear();
		}
		if (timer)
		{
			timer->join();
			timer.reset();
		}
	}

	bool sctp::create_stream(const std::string& label, uint16_t& sid)
	{
		std::lock_guard<std::mutex> lock(mutex_);

		sid = make_next_stream_id();
		auto s = ensure_stream(sid);

		sctp_dcep_open open;
		open.message_type = sctp_dcep_message_type::data_channel_open;
		open.channel_type = sctp_dcep_channel_type::reliable_unordered;
		open.reliability_parameter = 0;
		open.label.assign(label.begin(), label.end());
		return s->send(local_tsn_, open.serialize(), sctp_payload_protocol_id::dcep);
	}

	void sctp::close_stream(uint16_t sid)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		auto itr=streams_.find(sid);
		if (itr != streams_.end())
		{
			itr->second->close(local_tsn_);
			on_stream_closed.invoke(sid);
		}
		streams_.erase(itr);
	}

	bool sctp::send_user_message(uint16_t sid, const std::vector<uint8_t>& data)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		auto itr = streams_.find(sid);
		if (itr == streams_.end())
		{
			return false;
		}

		return itr->second->send(local_tsn_,data);
	}

	void sctp::conn_input(const uint8_t* data, size_t size)
	{
		sctp_packet pkt;
		if (!pkt.deserialize(data, size))
		{
			return;
		}

		if (pkt.header.checksum > 0)
		{
			uint32_t crc32 = sctp_packet::check_crc32(data, size);
			if (pkt.header.checksum != crc32)
			{
				return;
			}
		}

		for (auto itr = pkt.chunks.begin(); itr != pkt.chunks.end(); itr++)
		{
			on_chunk(pkt.header, *itr);
		}
	}



	void sctp::on_chunk(const sctp_header& header, const sctp_chunk& chunk)
	{
		if (chunk.type == sctp_chunk_type::init)
		{
			on_init(header, chunk);
		}
		else if (chunk.type == sctp_chunk_type::init_ack)
		{
			on_init_ack(header, chunk);
		}
		else if (chunk.type == sctp_chunk_type::cookie_echo)
		{
			on_cookie_echo(header, chunk);
		}
		else if (chunk.type == sctp_chunk_type::cookie_ack)
		{
			on_cookie_ack(header, chunk);
		}
		else if (chunk.type == sctp_chunk_type::data)
		{
			on_data(header, chunk);
		}
		else if (chunk.type == sctp_chunk_type::sack)
		{
			on_sack(header, chunk);
		}
		else if (chunk.type == sctp_chunk_type::heartbeat)
		{
			on_heartbeat(header, chunk);
		}
		else if (chunk.type == sctp_chunk_type::heartbeat_ack)
		{
			on_heartbeat_ack(header, chunk);
		}
		else if (chunk.type == sctp_chunk_type::shutdown)
		{
			on_shutdown(header, chunk);
		}
		else if (chunk.type == sctp_chunk_type::shutdown_ack)
		{
			on_shutdown_ack(header, chunk);
		}
		else if (chunk.type == sctp_chunk_type::abort)
		{
			on_abort(header, chunk);
		}
		else if (chunk.type == sctp_chunk_type::operational_error)
		{
			on_operational_error(header, chunk);
		}
	}

	void sctp::on_init(const sctp_header& header, const sctp_chunk& chunk)
	{
		sctp_init_chunk init;
		if (!init.deserialize(chunk.payload.data(), chunk.payload.size()))
		{
			sctp_error_chunk err_chunk;
			sctp_chunk_tlv tlv;
			tlv.type = (uint16_t)sctp_error_cause::protocol_violation;
			err_chunk.parameters.push_back(tlv);
			auto buf = err_chunk.serialize();
			send_data(sctp_chunk_type::operational_error, sctp_chunk_flags::none, buf);
			return;
		}
		std::lock_guard<std::mutex> lock(mutex_);
		streams_.clear();
		remote_tag_ = init.init_tag;
		remote_init_tsn_ = init.init_tsn;
		
		remote_port_ = header.src_port;

		sctp_init_ack_chunk ack_chunk;
		ack_chunk.arwnd = local_arwnd_;
		ack_chunk.init_tag = local_tag_;
		ack_chunk.init_tsn = local_tsn_;
		ack_chunk.num_in_stream = 16;
		ack_chunk.num_out_stream = 16;
		sctp_chunk_tlv cookie_tlv;
		cookie_tlv.type = (uint16_t)sctp_init_parameter_type::state_cookie;
		cookie_tlv.value = local_cookie_;
		ack_chunk.parameters.push_back(cookie_tlv);

		auto buf = ack_chunk.serialize();
		send_data(sctp_chunk_type::init_ack, sctp_chunk_flags::none, buf);
	}

	void sctp::on_init_ack(const sctp_header& header, const sctp_chunk& chunk)
	{
		sctp_init_ack_chunk init_ack;
		if (!init_ack.deserialize(chunk.payload.data(), chunk.payload.size()))
		{
			sctp_error_chunk err_chunk;
			sctp_chunk_tlv tlv;
			tlv.type = (uint16_t)sctp_error_cause::protocol_violation;
			err_chunk.parameters.push_back(tlv);
			auto buf = err_chunk.serialize();
			send_data(sctp_chunk_type::operational_error, sctp_chunk_flags::none, buf);
			return;
		}

		if (!init_ack.find_paramter(sctp_init_parameter_type::state_cookie, remote_cookie_))
		{
			sctp_error_chunk err_chunk;
			sctp_chunk_tlv tlv;
			tlv.type = (uint16_t)sctp_error_cause::missing_mandatory_parameter;
			err_chunk.parameters.push_back(tlv);
			auto buf = err_chunk.serialize();
			send_data(sctp_chunk_type::operational_error, sctp_chunk_flags::none, buf);
			return;
		}

		sctp_cookie_echo_chunk cookie_chunk;
		cookie_chunk.cookie = remote_cookie_;
		auto buf = cookie_chunk.serialize();
		send_data(sctp_chunk_type::cookie_echo, sctp_chunk_flags::none, buf);
	}

	void sctp::on_cookie_echo(const sctp_header& header, const sctp_chunk& chunk)
	{
		sctp_cookie_echo_chunk cookie_echo_chunk;
		if (!cookie_echo_chunk.deserialize(chunk.payload.data(), chunk.payload.size(),chunk.payload_length()))
		{
			sctp_error_chunk err_chunk;
			
			sctp_chunk_tlv tlv;
			tlv.type = (uint16_t)sctp_error_cause::protocol_violation;
			err_chunk.parameters.push_back(tlv);
			auto buf = err_chunk.serialize();
			send_data(sctp_chunk_type::operational_error, sctp_chunk_flags::none, buf);
			return;
		}

		if (!std::equal(cookie_echo_chunk.cookie.begin(), cookie_echo_chunk.cookie.end(), local_cookie_.begin(), local_cookie_.end()))
		{
			sctp_error_chunk err_chunk;
			sctp_chunk_tlv tlv;
			tlv.type = (uint16_t)sctp_error_cause::stale_cookie_error;
			err_chunk.parameters.push_back(tlv);
			auto buf = err_chunk.serialize();
			send_data(sctp_chunk_type::operational_error, sctp_chunk_flags::none, buf);
			return;
		}


		std::vector<uint8_t> buf;
		send_data(sctp_chunk_type::cookie_ack, sctp_chunk_flags::none, buf);
		set_state(sctp_state::connected);
		on_connected.invoke();
	}
	void sctp::on_cookie_ack(const sctp_header& header, const sctp_chunk& chunk)
	{
		set_state(sctp_state::connected);
		on_connected.invoke();
	}

	void sctp::on_data(const sctp_header& header, const sctp_chunk& chunk)
	{
		sctp_data_chunk data_chunk;
		if (!data_chunk.deserialize(chunk.payload.data(), chunk.payload.size()))
		{
			sctp_error_chunk err_chunk;

			sctp_chunk_tlv tlv;
			tlv.type = (uint16_t)sctp_error_cause::protocol_violation;
			err_chunk.parameters.push_back(tlv);
			auto buf = err_chunk.serialize();
			send_data(sctp_chunk_type::operational_error, sctp_chunk_flags::none, buf);
			return;
		}



		insert_data_chunk(chunk.flags, data_chunk);

	}

	void sctp::on_sack(const sctp_header& header, const sctp_chunk& chunk)
	{
		sctp_sack_chunk sack_chunk;
		if (!sack_chunk.deserialize(chunk.payload.data(), chunk.payload.size()))
		{
			sctp_error_chunk err_chunk;
			sctp_chunk_tlv tlv;
			tlv.type = (uint16_t)sctp_error_cause::protocol_violation;
			err_chunk.parameters.push_back(tlv);
			auto buf = err_chunk.serialize();
			send_data(sctp_chunk_type::operational_error, sctp_chunk_flags::none, buf);
			return;
		}
		std::lock_guard<std::mutex> lock(mutex_);
		for(auto itr= streams_.begin(); itr != streams_.end(); itr++)
		{
			itr->second->process_sack(sack_chunk);
		}
	}

	void sctp::on_heartbeat(const sctp_header& header, const sctp_chunk& chunk)
	{
		sctp_heartbeat_chunk hb_chunk;
		if (!hb_chunk.deserialize(chunk.payload.data(), chunk.payload.size()))
		{
			sctp_error_chunk err_chunk;
			sctp_chunk_tlv tlv;
			tlv.type = (uint16_t)sctp_error_cause::protocol_violation;
			err_chunk.parameters.push_back(tlv);
			auto buf = err_chunk.serialize();
			send_data(sctp_chunk_type::operational_error, sctp_chunk_flags::none, buf);
			return;
		}

		sctp_heartbeat_ack_chunk ack;
		ack.parameters = hb_chunk.parameters;
		auto buf = ack.serialize();
		send_data(sctp_chunk_type::operational_error, sctp_chunk_flags::none, buf);

		on_sctp_heartbeat.invoke();
	}
	void sctp::on_heartbeat_ack(const sctp_header& header, const sctp_chunk& chunk)
	{
		on_sctp_heartbeat.invoke();
	}
	void sctp::on_abort(const sctp_header& header, const sctp_chunk& chunk)
	{
		set_state(sctp_state::closed);
		sctp_abort_chunk abort_chunk;
		if (!abort_chunk.deserialize(chunk.payload.data(), chunk.payload.size()))
		{
			sctp_error_chunk err_chunk;
			sctp_chunk_tlv tlv;
			tlv.type = (uint16_t)sctp_error_cause::protocol_violation;
			err_chunk.parameters.push_back(tlv);
			auto buf = err_chunk.serialize();
			send_data(sctp_chunk_type::operational_error, sctp_chunk_flags::none, buf);
			return;
		}

		std::lock_guard<std::mutex> lock(mutex_);
		for (auto itr = streams_.begin(); itr != streams_.end(); itr++)
		{
			on_stream_closed.invoke(itr->first);
			itr->second->close(local_tsn_);
		}
		streams_.clear();
		on_closed.invoke();
	}
	void sctp::on_shutdown(const sctp_header& header, const sctp_chunk& chunk)
	{
		for (auto itr = streams_.begin(); itr != streams_.end(); itr++)
		{
			on_stream_closed.invoke(itr->first);
			itr->second->close(local_tsn_);
		}
		std::lock_guard<std::mutex> lock(mutex_);
		streams_.clear();

		set_state(sctp_state::closed);
		send_data(sctp_chunk_type::shutdown_ack, sctp_chunk_flags::none, std::vector<uint8_t>());
		on_closed.invoke();
	}
	void sctp::on_shutdown_ack(const sctp_header& header, const sctp_chunk& chunk)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		for (auto itr = streams_.begin(); itr != streams_.end(); itr++)
		{
			on_stream_closed.invoke(itr->first);
			itr->second->close(local_tsn_);
		}
		streams_.clear();
		set_state(sctp_state::closed);
		on_closed.invoke();
	}
	void sctp::on_operational_error(const sctp_header& header, const sctp_chunk& chunk)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		for (auto itr = streams_.begin(); itr != streams_.end(); itr++)
		{
			on_stream_closed.invoke(itr->first);
			itr->second->close(local_tsn_);
		}
		streams_.clear();

		set_state(sctp_state::closed);

		on_closed.invoke();
	}

	void sctp::on_user_message(std::shared_ptr<sctp_stream> stream, const sctp_message& msg)
	{
		auto first_block=msg.blocks.begin();
		if (first_block == msg.blocks.end())
			return;

		if (first_block->second.chunk.payload_protocol_id == sctp_payload_protocol_id::dcep)
		{
			on_dcep_message(stream,msg);
		}
		else if(first_block->second.chunk.payload_protocol_id == sctp_payload_protocol_id::empty_binary
			|| first_block->second.chunk.payload_protocol_id == sctp_payload_protocol_id::empty_string)
		{
			std::lock_guard<std::mutex> lock(mutex_);
			auto itr=streams_.find(msg.stream_id);
			if (itr != streams_.end())
			{
				on_stream_closed.invoke(msg.stream_id);
				itr->second->close(local_tsn_);
				streams_.erase(itr);
			}
		}
		else if (first_block->second.chunk.payload_protocol_id == sctp_payload_protocol_id::binary
			|| first_block->second.chunk.payload_protocol_id == sctp_payload_protocol_id::string)
		{
			std::vector<uint8_t> buf;
			if (!msg.serialize(buf))
				return;
			on_stream_message.invoke(msg.stream_id, buf);
		}
	}

	void sctp::on_dcep_message(std::shared_ptr<sctp_stream> stream,const sctp_message& msg)
	{
		std::vector<uint8_t> buf;
		if (!msg.serialize(buf))
			return;

		sctp_dcep_message_type msg_type = static_cast<sctp_dcep_message_type>(buf[0]);
		if (msg_type == sctp_dcep_message_type::data_channel_open)
		{
			sctp_dcep_open open;
			if (open.deserialize(buf.data(), buf.size()))
			{
				std::vector<uint8_t> buf;
				buf.push_back(static_cast<uint8_t>(sctp_dcep_message_type::data_channel_ack));
				stream->send(local_tsn_, buf, sctp_payload_protocol_id::dcep);
				on_stream_opened.invoke(msg.stream_id);
			}
		}
		else if (msg_type == sctp_dcep_message_type::data_channel_ack)
		{

		}
	}


	bool sctp::send_data(const std::vector<sctp_chunk>& chunks)
	{
		sctp_packet pkt;
		pkt.header.dst_port = remote_port_;
		pkt.header.src_port = local_port_;
		pkt.header.verification_tag = remote_tag_;
		pkt.chunks = chunks;

		auto buf = pkt.serialize();
		if (!sctp_packet::sign_crc32(buf.data(), buf.size()))
			return false;

		on_sctp_send_data.invoke(buf, (const sockaddr*)&remote_addr_,sizeof(remote_addr_));
		return true;
	}

	bool sctp::send_data(const sctp_chunk& chunk)
	{
		sctp_packet pkt;
		pkt.header.dst_port = remote_port_;
		pkt.header.src_port = local_port_;
		pkt.header.verification_tag = remote_tag_;
		pkt.chunks.push_back(chunk);

		auto buf = pkt.serialize();
		if (!sctp_packet::sign_crc32(buf.data(), buf.size()))
			return false;

		on_sctp_send_data.invoke(buf,(const sockaddr*)&remote_addr_,sizeof(remote_addr_));
		return true;
	}

	bool sctp::send_data(sctp_chunk_type type, sctp_chunk_flags flags, const std::vector<uint8_t>& payload)
	{
		sctp_chunk chunk(type, flags, payload);
		return send_data(chunk);
	}


	void sctp::run_timer()
	{
		int interval = 0;
		while (active_)
		{
			signal_.wait(1000);

			
			interval++;
			sctp_state state = sctp_state::none;
			bool timeout = false;
			get_last_state(state, timeout);
			if (state == sctp_state::connecting)
			{
				if (interval >= 3&& timeout)
				{
					interval = 0;
					sctp_init_chunk init_chunk;
					init_chunk.arwnd = local_arwnd_;
					init_chunk.init_tag = local_tag_;
					init_chunk.init_tsn = local_tsn_;
					init_chunk.num_in_stream = 16;
					init_chunk.num_out_stream = 16;

					auto buf = init_chunk.serialize();
					send_data(sctp_chunk_type::init, sctp_chunk_flags::none, buf);
				}
			}
			else if (state_ == sctp_state::connected)
			{
				if (interval >= 5)
				{
					interval = 0;
					set_state(sctp_state::connected);

					uint32_t sec = (uint32_t)(sys::util::cur_time_ms() / 1000);
					uint64_t ts = sys::util::cur_time_ms();
					sctp_heartbeat_chunk hb;
					sctp_chunk_tlv tlv;
					tlv.type = 1;
					tlv.value.resize(4);
					tlv.value[0] = static_cast<uint8_t>(sec >> 24);
					tlv.value[1] = static_cast<uint8_t>(sec >> 16);
					tlv.value[2] = static_cast<uint8_t>(sec >> 8);
					tlv.value[3] = static_cast<uint8_t>(sec);
					hb.parameters.push_back(tlv);
					auto buf = hb.serialize();
					send_data(sctp_chunk_type::heartbeat, sctp_chunk_flags::none, buf);
				}
			}
			


			{
				std::lock_guard<std::mutex> lock(mutex_);

				//Clear timeout streams and messages
				//for (auto itr = streams_.begin(); itr != streams_.end(); )
				//{
				//	if (itr->second->is_timeout())
				//	{
				//		itr = streams_.erase(itr);
				//	}
				//	else
				//	{
				//		itr++;
				//	}
				//}

				
				for (auto itr = streams_.begin(); itr != streams_.end();itr++ )
				{
					itr->second->clear_timeout_recv_messages();
					itr->second->resend_messages();

					sctp_message msg(remote_init_tsn_);
					if (itr->second->first_message(msg))
					{
						send_sack(msg);
					}
				}

			}
		}
	}

	bool sctp::insert_data_chunk(sctp_chunk_flags flags, const sctp_data_chunk& chunk)
	{
		std::lock_guard<std::mutex> lock(mutex_);


		auto stream_itr = streams_.find(chunk.stream_id);
		if(stream_itr == streams_.end())
		{
			auto stream = std::make_shared<sctp_stream>(chunk.stream_id,remote_init_tsn_,
				[this](sctp_chunk_type t, sctp_chunk_flags f, const std::vector<uint8_t>& p) {
				return this->send_data(t, f, p);
			});

			auto r = streams_.insert(std::make_pair(chunk.stream_id, stream));
			stream_itr = r.first;
		}
		
		
		
		stream_itr->second->insert(flags, chunk);
		


		bool got = false;
		sctp_message msg(remote_init_tsn_);
		while (stream_itr->second->next_message(msg, got))
		{
			if (got)
			{
				send_sack(msg);
				on_user_message(stream_itr->second,msg);
			}
		}

		return true;
	}

	void sctp::set_state(sctp_state state)
	{
		state_ = state;
		state_updated_at_ = sys::util::cur_time_ms();
	}

	void sctp::get_last_state(sctp_state& state, bool& timeout)
	{
		state = state_;
		int64_t ts = sys::util::cur_time_ms() - state_updated_at_;
		timeout = ts >= 3000;
	}


	void sctp::send_sack(sctp_message& msg)
	{
		msg.calc_gaps();
		sctp_sack_chunk sack_chunk;
		sack_chunk.arwnd = this->local_arwnd_;
		sack_chunk.tsn = msg.cumulative_tsn;
		sack_chunk.dups.assign(msg.dups.begin(), msg.dups.end());
		sack_chunk.gaps = msg.gaps;
		send_data(sctp_chunk_type::sack, sctp_chunk_flags::unordered, sack_chunk.serialize());
	}

	uint32_t sctp::make_next_stream_id()
	{
		for (uint16_t i = 0; i < 0x7FFF; i++)
		{
			uint16_t stream_id = init_stream_id_;
			init_stream_id_ += 2;
			if(streams_.find(stream_id)==streams_.end())
			{
				return stream_id;
			}
		}
		return 0;
	}

	std::shared_ptr<sctp_stream> sctp::ensure_stream(uint16_t sid)
	{
		auto itr=streams_.find(sid);
		if (itr == streams_.end())
		{
			auto s=std::make_shared<sctp_stream>(sid, remote_init_tsn_,
				[this](sctp_chunk_type t, sctp_chunk_flags f, const std::vector<uint8_t>& p) {
					return this->send_data(t, f, p);
			});
			auto r=streams_.insert(std::make_pair(sid,s));
			itr = r.first;
		}

		return itr->second;
	}
}
