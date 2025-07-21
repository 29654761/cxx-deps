/**
 * @file transport.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2025 Shijie Zhou
 */


#include "transport.h"
#include "../rtpx_core.h"

#include <sys2/util.h>

namespace rtpx
{
	transport::transport(asio::io_context& ioc, uint16_t local_port,spdlogger_ptr log)
		:ioc_(ioc)
	{
		local_port_ = local_port;
		log_ = log;
		tie_breaker_ = sys::util::random_number<uint64_t>(0, 0xFFFFFFFFFFFFFFFF);
	}

	transport::~transport()
	{
	}

	void transport::close()
	{
		if (sctp_)
		{
			sctp_->close();
			sctp_.reset();
		}
	}

	bool transport::dtls_send(const uint8_t* data, size_t size, const asio::ip::udp::endpoint& endpoint)
	{
		if (!dtls_)
			return false;

		std::vector<uint8_t> plain(data, data + size);
		std::vector<uint8_t> cipher;
		if (!dtls_->encrypt(plain, cipher))
			return false;

		return send(cipher.data(), cipher.size(), endpoint);
	}

	bool transport::send_rtp_packet(packet_ptr packet, const asio::ip::udp::endpoint& endpoint)
	{
#ifdef RTPX_SSL
		if (dtls_)
		{
			if (!handshake_)
			{
				return false;
			}
		}
#endif

		std::vector<uint8_t> buf;
		buf.reserve(2048); // size for srtp
		if (!packet->serialize(buf))
		{
			return false;
		}

		int size = (int)buf.size();
#ifdef RTPX_SSL
		{
			std::lock_guard<std::mutex> lk(srtp_mutex_);
			if (srtp_out_)
			{
				auto ret = srtp_protect(srtp_out_, (void*)buf.data(), &size);
				if (ret != srtp_err_status_ok)
				{
					if (log_)
					{
						log_->warn("srtp_protect err = {}", ret)->flush();
					}
					return false;
				}
			}
		}
#endif

		return send(buf.data(), size, endpoint);
	}

	bool transport::send_rtcp_packet(const uint8_t* rtcp_data, size_t size, const asio::ip::udp::endpoint& endpoint)
	{
		int siz = (int)size;
#ifdef RTPX_SSL
		if (dtls_)
		{
			if (!handshake_)
			{
				return false;
			}
		}

		{
			std::lock_guard<std::mutex> lk(srtp_mutex_);
			if (srtp_out_)
			{
				auto ret = srtp_protect_rtcp(srtp_out_, (void*)rtcp_data, &siz);
				if (ret != srtp_err_status_ok)
				{
					if (log_)
					{
						log_->warn("srtp_protect_rtcp err = {}", ret)->flush();
					}
					return false;
				}
			}
		}
#endif

		return send(rtcp_data, siz, endpoint);
	}

	bool transport::send_stun_request(uint32_t local_priority, const asio::ip::udp::endpoint& endpoint)
	{
		stun_message req;
		req.type_ = stun_message_type_binding_request;
		req.make_transaction_id();
		req.attributes_.emplace_back(stun_attribute_type_username, ice_ufrag_remote_ + ":" + ice_ufrag_local_);

		std::string net;
		uint16_t network_id = sys::socket::hton16(local_ice_network_id_);
		net.append((const char*)&network_id, 2);
		uint16_t network_cost = sys::socket::hton16(local_ice_network_cost_);
		net.append((const char*)&network_cost, 2);
		req.attributes_.emplace_back(stun_attribute_type_goog_network_info, net);


		uint64_t v = sys::socket::hton64(tie_breaker_);
		if (ice_role_ == stun_ice_controlling) {
			req.attributes_.emplace_back(stun_attribute_type_ice_controlling, (const char*)&v, sizeof(v));
		}
		else {
			req.attributes_.emplace_back(stun_attribute_type_ice_controlled, (const char*)&v, sizeof(v));
		}

		req.attributes_.emplace_back(stun_attribute_type_use_candidate, "");

		local_priority = sys::socket::hton32(local_priority);
		req.attributes_.emplace_back(stun_attribute_type_priority, (const char*)&local_priority, sizeof(local_priority));

		std::string sreq = req.serialize(ice_pwd_remote_);

		return this->send((const uint8_t*)sreq.data(), (int)sreq.size(), endpoint);
	}

	bool transport::send_dtls_request(const asio::ip::udp::endpoint& endpoint)
	{
		if (!dtls_)
			return false;
		std::string req = dtls_->connect();
		if (req.size() == 0) {
			return false;
		}

		return send((const uint8_t*)req.data(), req.size(), endpoint);
	}

	bool transport::sctp_send(uint16_t sid, const uint8_t* data, size_t size)
	{
		if (!sctp_)
			return false;

		return sctp_->send_user_message(sid, std::vector<uint8_t>(data, data + size));
	}

	bool transport::enable_dtls(bool enabled)
	{
#ifdef RTPX_SSL
		std::lock_guard<std::mutex> lk(srtp_mutex_);
		if (enabled)
		{
			if (!dtls_)
			{
				dtls_ = rtpx_core::instance()->get_dtls();
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
	std::string transport::fingerprint() const
	{
#ifdef RTPX_SSL
		if (!dtls_)
		{
			return "";
		}
		return dtls_->fingerprint();

#else
		return "";
#endif
	}

	void transport::received_message(const uint8_t* buffer, size_t size, const asio::ip::udp::endpoint& endpoint)
	{
		update_.update();
		auto proto = detect_message(buffer[0]);
		if (proto == proto_rtp)
		{
			handle_rtp_message(buffer, size,endpoint);
		}
		else if (proto == proto_dtls)
		{
			handle_dtls_message(buffer, size, endpoint);
		}
		else if (proto == proto_stun)
		{
			handle_stun_message(buffer, size, endpoint);
		}
		else
		{

		}
	}

	proto_type_t transport::detect_message(uint8_t b)
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

	bool transport::detect_rtcp_packet(const uint8_t* buffer, size_t size, rtcp_header& hdr)
	{
		memset(&hdr, 0, sizeof(hdr));
		int ptv = rtcp_header_parse(&hdr, (const uint8_t*)buffer, size);
		if (ptv < 0)
		{
			return false;
		}
		
		if (ptv == rtcp_packet_type::RTCP_APP ||
			ptv == rtcp_packet_type::RTCP_BYE ||
			ptv == rtcp_packet_type::RTCP_RR ||
			ptv == rtcp_packet_type::RTCP_SR ||
			ptv == rtcp_packet_type::RTCP_SDES ||
			ptv == rtcp_packet_type::RTCP_PSFB ||
			ptv == rtcp_packet_type::RTCP_RTPFB)
		{
			return true;
		}


		return false;
	}

	void transport::handle_rtp_message(const uint8_t* buffer, size_t size, const asio::ip::udp::endpoint& endpoint)
	{
		rtcp_header hdr = {};
		if (detect_rtcp_packet(buffer, size, hdr))
		{
#ifdef RTPX_SSL
			if (srtp_in_)
			{
				int srtp_size = (int)size;
				srtp_err_status_t ret = srtp_unprotect_rtcp(srtp_in_, (void*)buffer, &srtp_size);
				if (ret != srtp_err_status_ok)
				{
					if (log_)
					{
						log_->warn("srtp_unprotect_rtcp failed err={}", ret)->flush();
					}
					return;
				}
				size = srtp_size;
			}
#endif
			handle_rtcp_message(buffer, size,hdr,endpoint);
		}
		else
		{
#ifdef RTPX_SSL
			if (srtp_in_)
			{
				int srtp_size = (int)size;
				srtp_err_status_t ret = srtp_unprotect(srtp_in_, (void*)buffer, &srtp_size);
				if (ret != srtp_err_status_ok)
				{
					if (log_)
					{
						log_->warn("srtp_unprotect failed err={}", ret)->flush();
					}
					return;
				}
				size = srtp_size;
			}
#endif
			auto pkt = std::make_shared<packet>();
			if (pkt->parse((const uint8_t*)buffer, size)) {
				on_rtp_packet(pkt,endpoint);
			}
		
		}
	}

	void transport::handle_rtp_packet(packet_ptr packet, const asio::ip::udp::endpoint& endpoint)
	{
		if (on_rtp_packet)
		{
			on_rtp_packet(packet, endpoint);
		}
	}

	void transport::handle_rtcp_message(const uint8_t* buffer, size_t size, const rtcp_header& hdr, const asio::ip::udp::endpoint& endpoint)
	{
		if (hdr.common.pt == rtcp_packet_type::RTCP_RR)
		{
			auto rr = rtcp_rr_create();
			if (rtcp_rr_parse(rr, buffer, size) >= 0)
			{
				on_rtcp_rr(rr,endpoint);
			}
			rtcp_rr_free(rr);
		}
		else if (hdr.common.pt == rtcp_packet_type::RTCP_SR)
		{
			auto sr = rtcp_sr_create();
			if (rtcp_sr_parse(sr, buffer, size) >= 0)
			{
				on_rtcp_sr(sr,endpoint);
			}
			rtcp_sr_free(sr);
		}
		else if (hdr.common.pt == rtcp_packet_type::RTCP_RTPFB|| hdr.common.pt == rtcp_packet_type::RTCP_PSFB)
		{
			auto fb = rtcp_fb_create();
			if (rtcp_fb_parse(fb, buffer, size) >= 0)
			{
				handle_rtcp_feedback(fb,endpoint);
			}
			rtcp_fb_free(fb);
		}
		else if (hdr.common.pt == rtcp_packet_type::RTCP_SDES)
		{
			auto sdes = rtcp_sdes_create();
			if (rtcp_sdes_parse(sdes, buffer, size) >= 0)
			{
				handle_rtcp_sdes(sdes,endpoint);
			}
			rtcp_sdes_free(sdes);
		}
		else if (hdr.common.pt == rtcp_packet_type::RTCP_BYE)
		{
			auto bye = rtcp_bye_create();
			if (rtcp_bye_parse(bye, buffer, size) >= 0)
			{
				handle_rtcp_bye(bye,endpoint);
			}
			rtcp_bye_free(bye);
		}
		else if (hdr.common.pt == rtcp_packet_type::RTCP_APP)
		{
			auto app = rtcp_app_create();
			if (rtcp_app_parse(app, buffer, size) >= 0)
			{
				handle_rtcp_app(app,endpoint);
			}
			rtcp_app_free(app);
		}

		
	}

	void transport::handle_rtcp_app(const rtcp_app* app, const asio::ip::udp::endpoint& endpoint)
	{
		if (on_rtcp_app)
		{
			on_rtcp_app(app,endpoint);
		}
	}

	void transport::handle_rtcp_bye(const rtcp_bye* bye, const asio::ip::udp::endpoint& endpoint)
	{
		if (on_rtcp_bye)
		{
			for (unsigned int i = 0; i < bye->header.common.count; i++)
			{
				on_rtcp_bye(bye->src_ids[i], bye->message,endpoint);
			}
		}
	}

	void transport::handle_rtcp_sr(const rtcp_sr* sr, const asio::ip::udp::endpoint& endpoint)
	{
		if (on_rtcp_sr)
		{
			on_rtcp_sr(sr,endpoint);
		}
	}

	void transport::handle_rtcp_rr(const rtcp_rr* rr, const asio::ip::udp::endpoint& endpoint)
	{
		if (on_rtcp_rr)
		{
			on_rtcp_rr(rr,endpoint);
		}
	}

	void transport::handle_rtcp_sdes(const rtcp_sdes* sdes, const asio::ip::udp::endpoint& endpoint)
	{
		if (on_rtcp_sdes)
		{
			for (unsigned int i = 0; i < sdes->header.common.count; i++)
			{
				on_rtcp_sdes(&sdes->srcs[i],endpoint);
			}
		}
	}

	void transport::handle_rtcp_feedback(const rtcp_fb* fb, const asio::ip::udp::endpoint& endpoint)
	{
		if (on_rtcp_fb)
		{
			on_rtcp_fb(fb,endpoint);
		}
	}

	void transport::handle_dtls_message(const uint8_t* buffer, size_t size, const asio::ip::udp::endpoint& endpoint)
	{
#ifdef RTPX_SSL


		if (!dtls_)
		{
			return;
		}

		if (buffer[0] == 23) // Application data
		{
			std::vector<uint8_t> plain;
			std::vector<uint8_t> cipher(buffer, buffer + size);
			if (dtls_->decrypt(cipher, plain))
			{
				handle_app_message(plain.data(), plain.size(),endpoint);
			}
		}
		else
		{
			if (!dtls_->is_established())
			{
				std::string rsp = dtls_->accept(buffer, size);
				if (rsp.size() > 0)
				{
					this->send((const uint8_t*)rsp.data(), rsp.size(),endpoint);
				}
			}
			else
			{
				dtls_->read(buffer, size);

				std::string rsp = dtls_->flush();
				if (rsp.size() > 0)
				{
					this->send((const uint8_t*)rsp.data(), rsp.size(), endpoint);
				}

				std::lock_guard<std::mutex> lk(srtp_mutex_);
				{
					if (dtls_->is_init_finished())
					{
						dtls_info_t info = { 0 };
						if (dtls_->export_key_material(&info))
						{
							memset(&srtp_in_policy_, 0, sizeof(srtp_in_policy_));
							memset(&srtp_out_policy_, 0, sizeof(srtp_out_policy_));

							srtp_crypto_policy_set_from_profile_for_rtp(&srtp_in_policy_.rtp, (srtp_profile_t)info.profile_id);
							srtp_crypto_policy_set_from_profile_for_rtcp(&srtp_in_policy_.rtcp, (srtp_profile_t)info.profile_id);

							srtp_in_policy_.ssrc.type = ssrc_any_inbound;
							if (dtls_role_ == dtls_role_client)
							{
								srtp_in_policy_.key = info.server_key;
							}
							else
							{
								srtp_in_policy_.key = info.client_key;
							}

							srtp_crypto_policy_set_from_profile_for_rtp(&srtp_out_policy_.rtp, (srtp_profile_t)info.profile_id);
							srtp_crypto_policy_set_from_profile_for_rtcp(&srtp_out_policy_.rtcp, (srtp_profile_t)info.profile_id);
							srtp_out_policy_.ssrc.type = ssrc_any_outbound;
							if (dtls_role_ == dtls_role_client)
							{
								srtp_out_policy_.key = info.client_key;
							}
							else
							{
								srtp_out_policy_.key = info.server_key;
							}


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


							handshake_ = true;
							handle_dtls_completed(endpoint);

						}
					}
				}
			}
		}

#endif // RTPX_SSL
	}

	void transport::handle_dtls_completed(const asio::ip::udp::endpoint& ep)
	{
		if (sctp_)
		{
			sctp_->set_init_stream_id(dtls_role_ == dtls_role_client ? 1 : 2);
			sctp_->set_remote_endpoint(ep);

			if (dtls_role_ == dtls_role_client)
			{
				sctp_->connect(sctp_remote_port_);
			}
		}


		if (on_dtls_completed)
		{
			on_dtls_completed(dtls_role_);
		}
	}

	void transport::handle_app_message(const uint8_t* buffer, size_t size, const asio::ip::udp::endpoint& endpoint)
	{
		std::lock_guard<std::recursive_mutex> lk(sctp_mutex_);
		if (sctp_)
		{
			sctp_->conn_input(buffer, size);
		}
	}

	void transport::handle_stun_message(const uint8_t* buffer, size_t size, const asio::ip::udp::endpoint& endpoint)
	{
		stun_message msg;
		uint32_t fp = 0;
		if (!msg.deserialize((const char*)buffer, size, fp)) {
			return;
		}

		
		if (msg.type_ == stun_message_type_binding_request)
		{
			if (on_stun_completed)//Save remote address;
			{
				on_stun_completed(endpoint);
			}

			if (ice_role_ == stun_ice_controlling)
			{
				std::string val = msg.get_attribute(stun_attribute_type_ice_controlling);
				if (val.size() > 0) {
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
							send((const uint8_t*)s.data(), s.size(),endpoint);
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


				const sockaddr* addr = reinterpret_cast<const sockaddr*>(endpoint.data());
				auto attr = stun_attribute_xor_mapped_address::create((const sockaddr*)addr, (const char*)msg.transaction_id_);
				rsp.attributes_.emplace_back(stun_attribute_type_xor_mapped_address, attr.serialize());


				std::string s = rsp.serialize(ice_pwd_local_);

				send((const uint8_t*)s.data(), (int)s.size(), endpoint);

			}
		}
		if (msg.type_ == stun_message_type_binding_response)
		{
			handle_stun_completed(endpoint);
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

	void transport::handle_stun_completed(const asio::ip::udp::endpoint& endpoint)
	{
#ifdef RTPX_SSL
		if (!handshake_&& dtls_role_ == dtls_role_client)
		{
			send_dtls_request(endpoint);
		}
#endif
		if (on_stun_completed)
		{
			on_stun_completed(endpoint);
		}
	}

	void transport::sctp_init(uint16_t local_port)
	{
		std::lock_guard<std::recursive_mutex> lock(sctp_mutex_);
		if (!sctp_)
		{
			auto self = shared_from_this();
			sctp_ = std::make_shared<sctp>(ioc_);
			sctp_->bind_connected_handler(std::bind(&transport::handle_sctp_connected, self));
			sctp_->bind_disconnected_handler(std::bind(&transport::handle_sctp_disconnected, self));
			sctp_->bind_heartbeat_handler(std::bind(&transport::handle_sctp_heartbeat, self));
			sctp_->bind_send_data_handler(std::bind(&transport::handle_sctp_send_data, self, std::placeholders::_1, std::placeholders::_2));
			sctp_->bind_stream_closed_handler(std::bind(&transport::handle_sctp_stream_closed, self, std::placeholders::_1));
			sctp_->bind_stream_opened_handler(std::bind(&transport::handle_sctp_stream_opened, self, std::placeholders::_1));
			sctp_->bind_stream_message_handler(std::bind(&transport::handle_sctp_stream_message, self, std::placeholders::_1, std::placeholders::_2));
			sctp_->open(local_port);
		}
	}

	bool transport::sctp_open_stream(uint16_t& sid)
	{
		std::lock_guard<std::recursive_mutex> lock(sctp_mutex_);
		if (!sctp_)
		{
			return false;
		}
		return sctp_->create_stream("signal", sid);
	}
	void transport::sctp_close_stream(uint16_t sid)
	{
		std::lock_guard<std::recursive_mutex> lock(sctp_mutex_);
		if (sctp_)
		{
			sctp_->close_stream(sid);
		}
	}
	
	std::string transport::make_key(const asio::ip::udp::endpoint& endpoint)
	{
		std::error_code ec;
		return make_key(endpoint.address().to_string(ec), endpoint.port());
	}

	std::string transport::make_key(const std::string& ip, uint16_t port)
	{
		std::string key;
		key.reserve(64);
		key.append(ip);
		key.append(":");
		key.append(std::to_string(port));
		return key;
	}

	void transport::handle_sctp_send_data(const std::vector<uint8_t>& data, const asio::ip::udp::endpoint& endpoint)
	{
		dtls_send(data.data(), data.size(), endpoint);
	}

	void transport::handle_sctp_connected()
	{
		if (on_sctp_connected)
		{
			on_sctp_connected();
		}
	}

	void transport::handle_sctp_disconnected()
	{
		if (on_sctp_disconnected)
		{
			on_sctp_disconnected();
		}
	}

	void transport::handle_sctp_heartbeat()
	{
		if (on_sctp_heartbeat)
		{
			on_sctp_heartbeat();
		}
	}

	void transport::handle_sctp_stream_opened(uint16_t stream_id)
	{
		if (on_sctp_stream_opened)
		{
			on_sctp_stream_opened(stream_id);
		}
	}

	void transport::handle_sctp_stream_closed(uint16_t stream_id)
	{
		if (on_sctp_stream_closed)
		{
			on_sctp_stream_closed(stream_id);
		}
	}

	void transport::handle_sctp_stream_message(uint16_t stream_id, const std::vector<uint8_t>& message)
	{
		if (on_sctp_stream_message)
		{
			on_sctp_stream_message(stream_id, message);
		}
	}

}

