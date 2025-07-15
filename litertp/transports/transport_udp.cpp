/**
 * @file transport_udp.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */

#include "transport_udp.h"
#include "../proto/rtcp_header.h"
#include "../log.h"
#include "../global.h"

#include <sys2/util.h>
#include <string.h>

namespace litertp {

	transport_udp::transport_udp(int port)
	{
		port_ = port;

	}

	transport_udp::~transport_udp()
	{
		stop();
	}


	bool transport_udp::start()
	{
		if (!transport::start())
		{
			return false;
		}
		
		socket_ = std::make_shared<sys::socket>(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
		sockaddr_in v4 = { 0 };
		sys::socket::ep2addr(AF_INET,nullptr, port_, (sockaddr*)&v4);
		if (!socket_->bind((const sockaddr*)&v4, sizeof(v4))) {
			LOGE("transport start error: bind rtp port failed.\n");
			stop();
			return false;
		}

		socket_->set_recvbuf_size(1024 * 1024);
		socket_->set_sendbuf_size(1024 * 1024);

		if (raw_socket_)
		{
			receiver_ = new std::thread(&transport_udp::run_raw_recever, this);
		}
		else 
		{
			receiver_ = new std::thread(&transport_udp::run_recever, this);
		}

		return true;
	}

	void transport_udp::stop()
	{
		transport::stop();
		if (socket_)
		{
			socket_->close();
		}
		if (receiver_) {
			receiver_->join();
			delete receiver_;
			receiver_ = nullptr;
		}


#ifdef LITERTP_SSL
		if (srtp_out_) {
			srtp_dealloc(srtp_out_);
			srtp_out_ = nullptr;
		}
		if (srtp_in_) {
			srtp_dealloc(srtp_in_);
			srtp_in_ = nullptr;
		}
#endif
	}

	bool transport_udp::enable_security(bool enabled)
	{
#ifdef LITERTP_SSL
		if (enabled) 
		{
			if (!dtls_)
			{
				dtls_ = global::instance()->get_dtls();
			}
			
			return dtls_ != nullptr;
		}
		else
		{
			dtls_.reset();
			return true;
		}
#else
		return false;
#endif
	}

	std::string transport_udp::fingerprint()const
	{
#ifdef LITERTP_SSL
		if (!dtls_)
		{
			return "";
		}
		return dtls_->fingerprint();

#else
		return "";
#endif
	}

	bool transport_udp::send_rtp_packet(packet_ptr packet,const sockaddr* addr,int addr_size)
	{
#ifdef LITERTP_SSL
		if (dtls_)
		{
			if (!handshake)
			{
				return false;
			}
		}
#endif

		std::string b;
		b.reserve(2048); // size for srtp
		if (!packet->serialize(b))
		{
			return false;
		}

		int size = (int)b.size();

#ifdef LITERTP_SSL
		
		if (srtp_out_)
		{
			std::unique_lock<std::recursive_mutex> lk(mutex_);
			auto ret = srtp_protect(srtp_out_, (void*)b.data(), &size);
			if (ret != srtp_err_status_ok)
			{
				LOGE("srtp_protect err = %d\n", ret);
				return false;
			}
		}
#endif

		int r = socket_->sendto(b.data(), size, addr,addr_size);
		return r >= 0;
	}

	bool transport_udp::send_rtcp_packet(const uint8_t* rtcp_data, int size, const sockaddr* addr, int addr_size)
	{
#ifdef LITERTP_SSL
		if (dtls_)
		{
			if (!handshake)
			{
				return false;
			}
		}
		if (srtp_out_)
		{
			std::unique_lock<std::recursive_mutex> lk(mutex_);
			auto ret = srtp_protect_rtcp(srtp_out_, (void*)rtcp_data, &size);
			if (ret != srtp_err_status_ok)
			{
				LOGE("srtp_protect err = %d\n", ret);
				return false;
			}
		}
#endif

		int r = socket_->sendto((const char*)rtcp_data, size, addr, addr_size);
		if (r <= 0) {
			//DWORD err=GetLastError();
			return false;
		}
		return true;
	}

	void transport_udp::run_raw_recever()
	{
		while (active_)
		{
			char buffer[2048] = { 0 };
			sockaddr_storage addr = { 0 };
			socklen_t addr_size = sizeof(addr);
			int size = socket_->recvfrom(buffer, 2048, (sockaddr*)&addr, &addr_size);
			if (size > 0)
			{
				raw_packet_event_.invoke(socket_, (const uint8_t*)buffer, size, (sockaddr*)&addr, addr_size);
			}
			else
			{
				disconnected_event_.invoke(port_);
				//std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
		}
	}

	void transport_udp::run_recever()
	{
		while (active_)
		{
			char buffer[2048] = { 0 };
			sockaddr_storage addr = { 0 };
			socklen_t addr_size = sizeof(addr);
			int size = socket_->recvfrom(buffer, 2048, (sockaddr*)&addr, &addr_size);
			if (size > 0)
			{
				on_data_received_event((const uint8_t*)buffer, size, (sockaddr*)&addr, addr_size);
			}
			else
			{
				disconnected_event_.invoke(port_);
				//std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
		}
	}

	void transport_udp::on_data_received_event(const uint8_t* data, int size, const sockaddr* addr, int addr_size)
	{
		auto proto = test_message(data[0]);
		if (proto == proto_stun)
		{
			on_stun_message(data, size, (const sockaddr*)addr, addr_size);
		}
		else if (proto == proto_dtls)
		{
#ifdef LITERTP_SSL
			on_dtls(data, size, (const sockaddr*)addr, addr_size);
#endif
		}
		else if (proto == proto_rtp)
		{
			on_rtp_data(data, size, (const sockaddr*)addr, addr_size);
		}
		else
		{
			on_app_data(data, size, (const sockaddr*)addr, addr_size);
		}
	}


#ifdef LITERTP_SSL
	void transport_udp::on_dtls(const uint8_t* data, int size, const sockaddr* addr, int addr_size)
	{
		if (!dtls_)
		{
			return;
		}

		if (data[0] == 23) // Application data
		{
			std::vector<uint8_t> plain;
			std::vector<uint8_t> cipher(data, data + size);
			if (dtls_->decrypt(cipher, plain))
			{
				on_app_data(plain.data(), plain.size(), (sockaddr*)&addr, addr_size);
			}
		}
		else
		{
			if (!dtls_->is_established())
			{
				std::string rsp = dtls_->accept(data, size);
				if (rsp.size() > 0)
				{
					socket_->sendto(rsp.data(), (int)rsp.size(), addr, addr_size);
				}
			}
			else
			{
				dtls_->read(data, size);

				std::string rsp = dtls_->flush();
				if (rsp.size() > 0)
				{
					socket_->sendto(rsp.data(), (int)rsp.size(), addr, addr_size);
				}

				if (dtls_->is_init_finished())
				{
					litertp::dtls_info_t info = { 0 };
					if (dtls_->export_key_material(&info))
					{
						memset(&srtp_in_policy_, 0, sizeof(srtp_in_policy_));
						memset(&srtp_out_policy_, 0, sizeof(srtp_out_policy_));


						//srtp_crypto_policy_set_aes_gcm_128_8_auth(&srtp_in_policy_.rtp);
						//srtp_crypto_policy_set_aes_gcm_128_8_auth(&srtp_in_policy_.rtcp);					
						srtp_crypto_policy_set_from_profile_for_rtp(&srtp_in_policy_.rtp, (srtp_profile_t)info.profile_id);
						srtp_crypto_policy_set_from_profile_for_rtcp(&srtp_in_policy_.rtcp, (srtp_profile_t)info.profile_id);

						srtp_in_policy_.ssrc.type = ssrc_any_inbound;
						if (srtp_role_ == srtp_role_client)
						{
							srtp_in_policy_.key = info.server_key;
						}
						else
						{
							srtp_in_policy_.key = info.client_key;
						}

						//srtp_crypto_policy_set_rtp_default(&srtp_out_policy_.rtp);
						//srtp_crypto_policy_set_rtcp_default(&srtp_out_policy_.rtcp);
						srtp_crypto_policy_set_from_profile_for_rtp(&srtp_out_policy_.rtp, (srtp_profile_t)info.profile_id);
						srtp_crypto_policy_set_from_profile_for_rtcp(&srtp_out_policy_.rtcp, (srtp_profile_t)info.profile_id);
						srtp_out_policy_.ssrc.type = ssrc_any_outbound;
						if (srtp_role_ == srtp_role_client)
						{
							srtp_out_policy_.key = info.client_key;
						}
						else
						{
							srtp_out_policy_.key = info.server_key;
						}


						std::unique_lock<std::recursive_mutex> lk(mutex_);

						if (srtp_in_)
						{
							srtp_dealloc(srtp_in_);
							srtp_in_ = nullptr;
						}
						srtp_err_status_t r1 = srtp_create(&srtp_in_, &srtp_in_policy_);

						if (srtp_out_)
						{
							srtp_dealloc(srtp_out_);
							srtp_out_ = nullptr;
						}

						srtp_out_policy_.allow_repeat_tx = 1; // for retransmissions 
						r1 = srtp_create(&srtp_out_, &srtp_out_policy_);


						
						handshake = true;
						on_dtls_handshake(srtp_role_);
						
					}
				}
			}
		}
	}
#endif

	void transport_udp::on_stun_message(const uint8_t* data, int size, const sockaddr* addr, int addr_size)
	{
		stun_message msg;
		uint32_t fp = 0;
		if (!msg.deserialize((const char*)data, size, fp)) {
			return;
		}

		stun_message_event_.invoke(socket_, msg, addr, addr_size);

		if (msg.type_ == stun_message_type_binding_response)
		{
#ifdef LITERTP_SSL
			if (dtls_ && srtp_role_ == srtp_role_client)
			{
				std::string req = dtls_->connect();
				if (req.size() > 0) {
					socket_->sendto(req.data(), (int)req.size(), addr, addr_size);
				}
			}
#endif
		}
		else if (msg.type_ == stun_message_type_binding_request)
		{
			if (ice_role_ == stun_ice_controlling)
			{
				std::string val=msg.get_attribute(stun_attribute_type_ice_controlling);
				if(val.size() > 0) {
					stun_attribute msg_attr;
					if (msg_attr.deserialize(val.data(), val.size()) > 0)
					{
						uint64_t v = 0;
						memcpy(&v, msg_attr.value_.data(), std::min(msg_attr.value_.size(), sizeof(v)));
						v = sys::socket::ntoh64(v);
						if (tie_breaker_ >= v)
						{
							stun_message rsp;
							rsp.type_ = stun_message_type_binding_error_response;
							memcpy(rsp.transaction_id_, msg.transaction_id_, 12);

							stun_attribute_error_code attr;
							attr.class_ = 4;
							attr.number_ = stun_error_code_role_conflict;
							attr.reason_ = "Role Conflict";
							rsp.attributes_.emplace_back(stun_attribute_type_error_code, attr.serialize());
							std::string s = rsp.serialize(ice_pwd_local_);
							socket_->sendto(s.data(), (int)s.size(), addr, addr_size);
						}
						else
						{
							ice_role_ = stun_ice_controlled;
						}
						return;
					}
				}
			}

			{
				stun_message rsp;
				rsp.type_ = stun_message_type_binding_response;
				memcpy(rsp.transaction_id_, msg.transaction_id_, 12);



				auto attr = stun_attribute_xor_mapped_address::create(addr, (const char*)msg.transaction_id_);
				rsp.attributes_.emplace_back(stun_attribute_type_xor_mapped_address, attr.serialize());


				std::string s = rsp.serialize(ice_pwd_local_);
				socket_->sendto(s.data(), (int)s.size(), addr, addr_size);

				// Now a timer will send sturn
				//this->send_stun_request(addr, addr_size, 1853766911);
			}
		}
		else if (msg.type_ == stun_message_type_binding_error_response)
		{
			std::string val = msg.get_attribute(stun_attribute_type_error_code);
			if (val.size() > 0) {
				stun_attribute_error_code err_attr;
				if (err_attr.deserialize(val.data(), val.size()))
				{
					if (err_attr.number_ == stun_error_code_role_conflict)
					{
						ice_role_ = stun_ice_controlled;
					}
				}
			}
		}
	}

	void transport_udp::on_rtp_data(const uint8_t* data, int size, const sockaddr* addr, int addr_size)
	{
		int pt = 0;
		if (this->test_rtcp_packet(data, size, &pt))
		{
#ifdef LITERTP_SSL
			if (srtp_in_)
			{
				srtp_err_status_t ret = srtp_unprotect_rtcp(srtp_in_, (void*)data, &size);
				if (ret != srtp_err_status_ok)
				{
					LOGE("srtp_unprotect failed err=%d\n", ret);
					return;
				}
			}
#endif
			rtcp_packet_event_.invoke(socket_,(uint16_t)pt, (const uint8_t*)data, size,addr,addr_size);
		}
		else
		{
#ifdef LITERTP_SSL
			if (srtp_in_)
			{
				srtp_err_status_t ret = srtp_unprotect(srtp_in_, (void*)data, &size);
				if (ret != srtp_err_status_ok)
				{
					LOGE("srtp_unprotect failed err=%d\n", ret);
					return;
				}
			}
#endif
			auto pkt = std::make_shared<packet>();
			if (pkt->parse((const uint8_t*)data, size)) {
				rtp_packet_event_.invoke(socket_, pkt, addr, addr_size);
			}
		}
	}



	proto_type_t transport_udp::test_message(uint8_t b)
	{
		if (b > 127 && b < 192) {
			return proto_rtp;
		}
		else if (b >= 20 && b <= 64) {
			return proto_dtls;
		}
		else if (b < 2) {
			return proto_stun;
		}
		else {
			return proto_unknown;
		}
	}

	

	void transport_udp::send_stun_request(const sockaddr* addr, int addr_size, uint32_t priority)
	{
		stun_message req;
		req.type_ = stun_message_type_binding_request;
		req.make_transaction_id();
		req.attributes_.emplace_back(stun_attribute_type_username, ice_ufrag_remote_ + ":" + ice_ufrag_local_);

		std::string net;
		uint16_t net_id = local_ice_network_id_;
		net_id = sys::socket::hton16(net_id);
		net.append((const char*)&net_id, 2);
		uint16_t net_cost = this->local_ice_network_cost_;
		net_cost = sys::socket::hton16(net_cost);
		net.append((const char*)&net_cost, 2);
		req.attributes_.emplace_back(stun_attribute_type_goog_network_info, net);

		//uint64_t v = sys::util::random_number<uint64_t>(0, 0xFFFFFFFFFFFFFFFF);
		uint64_t v = sys::socket::hton64(tie_breaker_);
		if (ice_role_ == stun_ice_controlling) {
			req.attributes_.emplace_back(stun_attribute_type_ice_controlling, (const char*)&v, sizeof(v));
		}
		else {
			req.attributes_.emplace_back(stun_attribute_type_ice_controlled, (const char*)&v, sizeof(v));
		}

		req.attributes_.emplace_back(stun_attribute_type_use_candidate, "");

		priority = sys::socket::hton32(priority);
		req.attributes_.emplace_back(stun_attribute_type_priority, (const char*)&priority, sizeof(priority));

		std::string sreq = req.serialize(ice_pwd_remote_);
		socket_->sendto(sreq.data(), (int)sreq.size(), addr, addr_size);

#ifdef LITERTP_SSL
		if (dtls_ && !handshake && srtp_role_ == srtp_role_client)
		{
			std::string req = dtls_->connect();
			if (req.size() > 0) {
				socket_->sendto(req.data(), (int)req.size(), addr, addr_size);
			}
		}
#endif
	}

	bool transport_udp::send_raw_packet(const uint8_t* raw_data, int size, const sockaddr* addr, int addr_size)
	{
		if (dtls_)
		{
			std::vector<uint8_t> plain(raw_data, raw_data + size);
			std::vector<uint8_t> cipher;
			if (!dtls_->encrypt(plain, cipher))
				return false;
			int r = socket_->sendto((const char*)cipher.data(), (int)cipher.size(), addr, addr_size);
			return r > 0;
		}
		else
		{
			int r = socket_->sendto((const char*)raw_data, size, addr, addr_size);
			return r > 0;
		}
		
	}
}


