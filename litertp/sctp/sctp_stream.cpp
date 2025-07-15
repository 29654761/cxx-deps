/**
 * @file sctp_stream.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2025 Shijie Zhou
 */

#include "sctp_stream.h"
#include <sys2/util.h>

namespace litertp
{
	sctp_dcep_open::sctp_dcep_open()
	{
	}

	sctp_dcep_open::~sctp_dcep_open()
	{
	}

	bool sctp_dcep_open::deserialize(const uint8_t* data, size_t size)
	{
		label.clear();
		protocol.clear();
		if (size < 12)
			return false;

		message_type = static_cast<sctp_dcep_message_type>(data[0]);
		channel_type = static_cast<sctp_dcep_channel_type>(data[1]);
		priority = (data[2] << 8) | data[3];
		reliability_parameter = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | (data[7]);
		uint16_t label_length = (data[8] << 8) | data[9];
		uint16_t protocol_length = (data[10] << 8) | data[11];


		if (size < (size_t)(label_length + protocol_length + 12))
			return false;

		label.insert(label.end(), data + 12, data + 12 + label_length);
		protocol.insert(protocol.end(),
			data + 12 + label_length,
			data + 12 + label_length + protocol_length);
		return true;
	}

	std::vector<uint8_t> sctp_dcep_open::serialize() const
	{
		std::vector<uint8_t> buf;
		buf.reserve(1500);
		buf.push_back(static_cast<uint8_t>(message_type));
		buf.push_back(static_cast<uint8_t>(channel_type));

		buf.push_back(static_cast<uint8_t>(priority>>8));
		buf.push_back(static_cast<uint8_t>(priority));

		buf.push_back(static_cast<uint8_t>(reliability_parameter >> 24));
		buf.push_back(static_cast<uint8_t>(reliability_parameter >> 16));
		buf.push_back(static_cast<uint8_t>(reliability_parameter >> 8));
		buf.push_back(static_cast<uint8_t>(reliability_parameter));

		uint16_t label_length = static_cast<uint16_t>(label.size());
		uint16_t protocol_length = static_cast<uint16_t>(protocol.size());
		buf.push_back(static_cast<uint8_t>(label_length >> 8));
		buf.push_back(static_cast<uint8_t>(label_length));
		buf.push_back(static_cast<uint8_t>(protocol_length >> 8));
		buf.push_back(static_cast<uint8_t>(protocol_length));

		buf.insert(buf.end(), label.begin(), label.end());
		buf.insert(buf.end(), protocol.begin(), protocol.end());

		return buf;
	}

	//========================================================

	sctp_message::sctp_message(uint32_t init_tsn)
	{
		cumulative_tsn = init_tsn - 1;
		begin_tsn = cumulative_tsn;
	}

	sctp_message::~sctp_message()
	{
	}

	bool sctp_message::insert(sctp_chunk_flags flags, const sctp_data_chunk& chunk)
	{
		if (chunk.stream_id != this->stream_id)
			return false;

		if (chunk.stream_sequence != this->ssn)
			return false;


		updated_at = sys::util::cur_time_ms();
		block_t block;
		block.is_begin = (((uint16_t)flags) & ((uint16_t)sctp_chunk_flags::begin)) != 0;
		block.is_end = (((uint16_t)flags) & ((uint16_t)sctp_chunk_flags::end)) != 0;
		block.chunk = chunk;

		if (block.is_begin)
		{
			begin_tsn = block.chunk.tsn;
		}

		auto r=this->blocks.insert(std::make_pair(block.chunk.tsn, block));
		if (!r.second)
		{
			dups.insert(block.chunk.tsn);
		}

		return true;
	}

	bool sctp_message::is_completed()const
	{
		bool has_begin = false, has_end=false;
		
		for (uint32_t tsn = begin_tsn, i = 0; i < MAX_CHUNKS_PER_MESSAGE; tsn++, i++)
		{
			auto itr = blocks.find(tsn);
			if (itr == blocks.end())
			{
				return false;
			}
			if (itr->second.is_begin)
			{
				has_begin = true;
			}

			if (itr->second.is_end)
			{
				has_end = true;
				break;
			}
		}

		if (!has_begin || !has_end)
			return false;

		return true;
	}

	bool sctp_message::is_timeout(uint32_t ms)const
	{
		int64_t now = sys::util::cur_time_ms();
		return now - updated_at >= ms;
	}

	void sctp_message::calc_gaps()
	{
		// Calc gaps
		gaps.clear();
		if (blocks.size() == 0)
			return;

		std::vector<sctp_sack_chunk::gap_item_t> tmp;
		sctp_sack_chunk::gap_item_t item = {};
		bool has_begin = false, has_end = false, has_first = false;
		for (uint32_t tsn = begin_tsn, i = 0; i < MAX_CHUNKS_PER_MESSAGE; tsn++, i++)
		{
			auto itr = blocks.find(tsn);
			if (itr == blocks.end())
			{
				if (has_first)
				{
					tmp.push_back(item);
					has_first = false;
				}
				continue;
			}

			if (!has_first)
			{
				item.begin = itr->first;
				has_first = true;
			}
			if (itr->second.is_begin)
			{
				has_begin = true;
			}
			if (itr->second.is_end)
			{
				has_end = true;
			}


			item.end = itr->first;

			if (has_end)
			{
				break;
			}
		}

		if (has_first)
		{
			tmp.push_back(item);
		}

		if (tmp.size() > 0&&has_begin)
		{
			cumulative_tsn = tmp.begin()->end;
			tmp.erase(tmp.begin());
		}

		for (auto itr = tmp.begin(); itr != tmp.end(); itr++)
		{
			itr->begin -= cumulative_tsn;
			itr->end -= cumulative_tsn;
		}

		gaps = tmp;
	}

	bool sctp_message::serialize(std::vector<uint8_t>& buf)const
	{
		buf.clear();
		buf.reserve(blocks.size() * 1500);

		bool has_begin = false, has_end = false;
		for (uint32_t tsn = begin_tsn, i = 0; i < MAX_CHUNKS_PER_MESSAGE; tsn++, i++)
		{
			auto itr = blocks.find(tsn);
			if (itr == blocks.end())
				return false;
			
			if (itr->second.is_begin)
			{
				has_begin = true;
			}

			if (has_begin)
			{
				buf.insert(buf.end(), itr->second.chunk.payload.begin(), itr->second.chunk.payload.end());
			}

			if (itr->second.is_end)
			{
				has_end = true;
				break;
			}
		}

		if (!has_begin || !has_end)
			return false;

		return true;
	}


	//====================================================


	sctp_stream::sctp_stream(uint16_t stream_id, uint32_t init_tsn, send_data_event_t on_send_data)
	{
		stream_id_ = stream_id;
		on_send_data_ = on_send_data;
		init_tsn_ = init_tsn;
	}

	sctp_stream::~sctp_stream()
	{
	}

	bool sctp_stream::is_timeout()const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		int64_t now = sys::util::cur_time_ms();
		return now - updated_at_ >= 300000;
	}

	void sctp_stream::set_msg_timeout(uint32_t ms)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		msg_timeout_ = ms;
	}

	bool sctp_stream::send(uint32_t& tsn, const std::vector<uint8_t>& data, sctp_payload_protocol_id ppid)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if(sent_messages_.size()>10000)
		{
			return false;
		}

		if(data.size() < 1200)
		{
			sctp_data_chunk data_chunk;
			data_chunk.payload_protocol_id = ppid;
			data_chunk.stream_id = stream_id_;
			data_chunk.stream_sequence = next_send_ssn_++;
			data_chunk.tsn = tsn++;
			data_chunk.payload = data;
			sctp_chunk_flags flags = sctp_chunk_flags::begin| sctp_chunk_flags::end;
			on_send_data(sctp_chunk_type::data, flags, data_chunk.serialize());
			save_sent_message(flags, data_chunk);
		}
		else
		{
			auto fragments=split_data(data);
			uint16_t ssn = next_send_ssn_++;
			for (auto itr = fragments.begin(); itr != fragments.end(); itr++)
			{
				sctp_data_chunk data_chunk;
				data_chunk.payload_protocol_id = ppid;
				data_chunk.payload = *itr;
				data_chunk.stream_id = stream_id_;
				data_chunk.stream_sequence = ssn;
				data_chunk.tsn = tsn++;

				sctp_chunk_flags flags = sctp_chunk_flags::none;
				if (itr == fragments.begin())
				{
					flags = flags|sctp_chunk_flags::begin;
				}
				if(itr==fragments.end() - 1)
				{
					flags = flags | sctp_chunk_flags::end;
				}

				on_send_data(sctp_chunk_type::data, flags, data_chunk.serialize());
				save_sent_message(flags, data_chunk);
			}
		}

		return true;
	}

	bool sctp_stream::insert(sctp_chunk_flags flags, const sctp_data_chunk& chunk)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		
		auto itr = messages_.find(chunk.stream_sequence);
		if(itr==messages_.end())
		{
			sctp_message msg(init_tsn_);
			msg.stream_id = this->stream_id_;
			msg.ssn = chunk.stream_sequence;
			auto r = messages_.insert(std::make_pair(msg.ssn, msg));
			itr = r.first;
		}
		
		return itr->second.insert(flags, chunk);

	}

	bool sctp_stream::save_sent_message(sctp_chunk_flags flags, const sctp_data_chunk& chunk)
	{
		auto itr = sent_messages_.find(chunk.stream_sequence);
		if (itr == sent_messages_.end())
		{
			sctp_message msg(init_tsn_);
			msg.stream_id = this->stream_id_;
			msg.ssn = chunk.stream_sequence;
			auto r = sent_messages_.insert(std::make_pair(msg.ssn, msg));
			itr = r.first;
		}
		return itr->second.insert(flags, chunk);
	}

	std::vector<sctp_message> sctp_stream::all_messages()const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		std::vector<sctp_message> msgs;
		msgs.reserve(messages_.size());
		for (auto itr = messages_.begin(); itr != messages_.end(); itr++)
		{
			msgs.push_back(itr->second);
		}
		return msgs;
	}

	bool sctp_stream::next_message(sctp_message& msg, bool& got)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		got = false;
		auto itr = messages_.find(next_recv_ssn_);
		if (itr == messages_.end())
			return false;

		if (!itr->second.is_completed())
		{
			return false;
		}
		got = true;
		msg= itr->second;
		messages_.erase(itr);
		next_recv_ssn_++;
		return true;
	}

	bool sctp_stream::first_message(sctp_message& msg)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		auto itr = messages_.find(next_recv_ssn_);
		if (itr == messages_.end())
			return false;

		msg = itr->second;
		return true;
	}



	void sctp_stream::resend_messages()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		for (auto itr = sent_messages_.begin(); itr != sent_messages_.end(); )
		{
			for(auto itr2=itr->second.blocks.begin(); itr2!=itr->second.blocks.end(); )
			{
				sctp_chunk_flags flags = sctp_chunk_flags::none;
				if(itr2->second.is_begin)
					flags = flags | sctp_chunk_flags::begin;
				if (itr2->second.is_end)
					flags = flags | sctp_chunk_flags::end;
				
				on_send_data(sctp_chunk_type::data, flags, itr2->second.chunk.serialize());
				itr2->second.sent_times++;



				if (itr2->second.sent_times >= 3)
				{
					itr2 = itr->second.blocks.erase(itr2);
				}
				else
				{
					itr2++;
				}
			}

			if (itr->second.blocks.size() == 0)
				itr = sent_messages_.erase(itr);
			else
				itr++;
		}
	}

	void sctp_stream::process_sack(const sctp_sack_chunk& sack)
	{
		std::set<uint32_t> tsns;
		for(uint32_t tsn = sack.tsn-MAX_CHUNKS_PER_MESSAGE; tsn != sack.tsn; tsn++)
		{
			tsns.insert(tsn);
		}
		tsns.insert(sack.tsn);
		for(auto itr = sack.gaps.begin(); itr != sack.gaps.end(); itr++)
		{
			tsns.insert(itr->begin + sack.tsn);
			tsns.insert(itr->end + sack.tsn);
		}


		std::lock_guard<std::mutex> lock(mutex_);
		for (auto itr = sent_messages_.begin(); itr != sent_messages_.end(); )
		{
			for (auto itr2 = itr->second.blocks.begin(); itr2 != itr->second.blocks.end(); )
			{
				if (tsns.find(itr2->first)!=tsns.end())
				{
					itr2 = itr->second.blocks.erase(itr2);
				}
				else
				{
					itr2++;
				}
			}

			if (itr->second.blocks.size() == 0)
				itr = sent_messages_.erase(itr);
			else
				itr++;
		}
	}

	void sctp_stream::clear_timeout_recv_messages()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		for(auto itr= messages_.begin(); itr != messages_.end(); )
		{
			if (itr->second.is_timeout(msg_timeout_))
			{
				itr = messages_.erase(itr);
			}
			else
			{
				itr++;
			}
		}
	}

	void sctp_stream::close(uint32_t& tsn)
	{
		sctp_data_chunk data_chunk;
		data_chunk.payload_protocol_id = sctp_payload_protocol_id::empty_string;
		data_chunk.stream_id = stream_id_;
		data_chunk.stream_sequence = next_send_ssn_++;
		data_chunk.tsn = tsn++;
		on_send_data(sctp_chunk_type::data, sctp_chunk_flags::begin|sctp_chunk_flags::end, data_chunk.serialize());
	}

	bool sctp_stream::on_send_data(sctp_chunk_type type, sctp_chunk_flags flags, const std::vector<uint8_t>& payload)
	{
		if (!on_send_data_)
		{
			return false;
		}

		return on_send_data_(type, flags, payload);
	}

	std::vector<std::vector<uint8_t>> sctp_stream::split_data(const std::vector<uint8_t>& data, size_t chunkSize)
	{
		std::vector<std::vector<uint8_t>> result;

		size_t totalSize = data.size();
		for (size_t i = 0; i < totalSize; i += chunkSize) {
			size_t end = std::min(i + chunkSize, totalSize);
			result.emplace_back(data.begin() + i, data.begin() + end);
		}

		return result;
	}
}
