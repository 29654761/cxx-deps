/**
 * @file sctp_stream.h
 * @brief
 * @author Shijie Zhou
 * @copyright 2025 Shijie Zhou
 */

#pragma once
#include "sctp_chunk.h"
#include <map>
#include <set>
#include <mutex>
#include <functional>

#define TSN_GT(A,B)  ((int32_t)((A) - (B)) > 0)
#define TSN_LT(A,B)  ((int32_t)((A) - (B)) < 0)
#define TSN_GE(A,B)  ((int32_t)((A) - (B)) >= 0)
#define TSN_LE(A,B)  ((int32_t)((A) - (B)) <= 0)

#define MAX_CHUNKS_PER_MESSAGE 256

namespace litertp
{
	enum class sctp_dcep_message_type :uint8_t
	{
		data_channel_ack = 0x02,
		data_channel_open = 0x03,
		data_channel_close = 0x08,
	};

	enum class sctp_dcep_channel_type :uint8_t
	{
		reliable_ordered = 0x00,
		reliable_unordered = 0x01,
		partial_reliable_with_retransmission=0x02,
		partial_reliable_with_timeout=0x03,
		partial_reliable_unordered_retransmission=0x04,
		partial_reliable_unordered_timeout=0x05,
	};


	/*
DATA_CHANNEL_OPEN:
0                   1                   2                   3
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Message Type |  Channel Type |            Priority           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    Reliability Parameter                      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|         Label Length          |       Protocol Length         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
|                            Label                              |
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
|                           Protocol                            |
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+



DATA_CHANNEL_ACK:
0                   1
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Message Type = 0x02        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	*/
	class sctp_dcep_open
	{

	public:
		sctp_dcep_open();
		~sctp_dcep_open();

		bool deserialize(const uint8_t* data, size_t size);
		std::vector<uint8_t> serialize() const;
	public:
		sctp_dcep_message_type message_type= sctp_dcep_message_type::data_channel_open;
		sctp_dcep_channel_type channel_type= sctp_dcep_channel_type::reliable_unordered;
		uint16_t priority= 256;
		uint32_t reliability_parameter = 0;
		std::vector<uint8_t> label;
		std::vector<uint8_t> protocol;
	};



	class sctp_message
	{
	public:
		struct block_t
		{
			bool is_begin = false;
			bool is_end = false;
			sctp_data_chunk chunk;
			int sent_times = 0;
		};

		sctp_message(uint32_t init_tsn);
		~sctp_message();

		bool insert(sctp_chunk_flags flags,const sctp_data_chunk& chunk);
		bool is_completed()const;
		bool is_timeout(uint32_t ms)const;
		void calc_gaps();

		bool serialize(std::vector<uint8_t>& buf)const;

	public:
		uint16_t stream_id = 0;
		uint16_t ssn = 0;
		int64_t updated_at = 0;
		uint32_t begin_tsn = 0;
		uint32_t cumulative_tsn = 0;
		std::map<uint32_t, block_t> blocks;
		std::set<uint32_t> dups;
		std::vector<sctp_sack_chunk::gap_item_t> gaps;
	};

	class sctp_stream
	{
	public:
		typedef std::function<bool(sctp_chunk_type, sctp_chunk_flags, const std::vector<uint8_t>&)> send_data_event_t;
		sctp_stream(uint16_t stream_id,uint32_t init_tsn, send_data_event_t on_send_data);
		~sctp_stream();
		

		
		bool is_timeout()const;
		void set_msg_timeout(uint32_t ms);

		bool send(uint32_t& tsn,const std::vector<uint8_t>& data, sctp_payload_protocol_id ppid = sctp_payload_protocol_id::binary);

		bool insert(sctp_chunk_flags flags, const sctp_data_chunk& chunk);

		std::vector<sctp_message> all_messages()const;
		bool next_message(sctp_message& msg,bool& got);
		bool first_message(sctp_message& msg);

		void resend_messages();
		void process_sack(const sctp_sack_chunk& sack);
		void clear_timeout_recv_messages();

		void close(uint32_t& tsn);
	private:
		bool on_send_data(sctp_chunk_type type, sctp_chunk_flags flags, const std::vector<uint8_t>& payload);
		static std::vector<std::vector<uint8_t>> split_data(const std::vector<uint8_t>& data, size_t chunkSize = 1200);
	
		bool save_sent_message(sctp_chunk_flags flags, const sctp_data_chunk& chunk);
	private:
		mutable std::mutex mutex_;
		int64_t updated_at_ = 0;
		uint16_t stream_id_ = 0;
		uint16_t next_recv_ssn_ = 0;
		uint16_t next_send_ssn_ = 0;
		uint32_t msg_timeout_ = 3000;
		uint32_t init_tsn_ = 0;
		std::map<uint16_t, sctp_message> messages_;
		send_data_event_t on_send_data_;

		std::map<uint16_t, sctp_message> sent_messages_;

	};

}
