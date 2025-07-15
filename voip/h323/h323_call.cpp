#include "h323_call.h"
#include "h323_util.h"
#include "endpoint.h"
#include "tpkt.h"
#include <h460/h4601.h>
#include <h460/h46018.h>
#include <sys2/util.h>
#include <ptclib/random.h>
#include <h460/h46019.h>


namespace voip
{
	namespace h323
	{
		static PConstString const H239MessageOID("0.0.8.239.2");



		h323_call::h323_call(endpoint_ptr ep, tcp_socket_ptr h225_skt, const std::string& local_alias,const std::string& remote_alias, direction_t direction,
			const std::string& nat_address, int port,
			litertp::port_range_ptr h245_ports, litertp::port_range_ptr rtp_ports)
			:call(local_alias,remote_alias, direction, nat_address, port, rtp_ports)
			, ep_(ep)
			,h225_skt_(h225_skt)
			,h245_listen_(ep->io_context())
			, timer_(ep_->io_context())
			,h225_buffer_(10240)
			,h245_buffer_(10240)
			, request_seq_(1)
			, active_(false)
		{
			h245_ports_ = h245_ports;
			determination_number_ = PRandom::Number() % 16777216;
			set_media_crypto_suites();
			h225_recv_buffer_.fill(0);
			h245_recv_buffer_.fill(0);
			url_.scheme = "h323";
			call_type_ = call_type_t::h323;

			h225_sending_queue_ = std::make_shared<asio::sending_queue>();
			h245_sending_queue_ = std::make_shared<asio::sending_queue>();
		}

		h323_call::h323_call(endpoint_ptr ep,const asio::ip::tcp::endpoint& h225_addr, const std::string& local_alias, const std::string& remote_alias, direction_t direction,
			const std::string& nat_address, int port,
			litertp::port_range_ptr h245_ports, litertp::port_range_ptr rtp_ports)
			:call(local_alias,remote_alias, direction, nat_address, port, rtp_ports)
			, ep_(ep)
			, h225_addr_(h225_addr)
			, h245_listen_(ep->io_context())
			, timer_(ep_->io_context())
			, h225_buffer_(10240)
			, h245_buffer_(10240)
			, request_seq_(1)
			, active_(false)
		{
			h245_ports_ = h245_ports;
			determination_number_ = PRandom::Number() % 16777216;
			set_media_crypto_suites();
			h225_recv_buffer_.fill(0);
			h245_recv_buffer_.fill(0);
			url_.scheme = "h323";
			call_type_ = call_type_t::h323;

			h225_sending_queue_ = std::make_shared<asio::sending_queue>();
			h245_sending_queue_ = std::make_shared<asio::sending_queue>();
		}

		h323_call::~h323_call()
		{
			if (log_)
			{
				std::string url = url_.to_string();
				log_->info("H.323 call freed. url={}", url)->flush();
			}
		}

		void h323_call::set_media_crypto_suites()
		{
			PStringArray crypto_suites;
			crypto_suites += "AES_128";
			crypto_suites += "AES_192";
			crypto_suites += "AES_256";
			crypto_suites += "Clear";
			OpalMediaCryptoSuite::List cryptoSuites = OpalMediaCryptoSuite::FindAll(crypto_suites, "H.235");
			for (OpalMediaCryptoSuite::List::iterator it = cryptoSuites.begin(); it != cryptoSuites.end(); ++it)
				dh_.AddForAlgorithm(*it);
		}

		void h323_call::add_audio_channel_g711()
		{
			media_channel_t chann;
			chann.payload_type = 8;
			chann.session_id = 1;
			chann.type = media_channel_type_t::notmal;
			chann.media_type = media_type_audio;
			chann.codec_type = codec_type_pcma;
			chann.frequency = 8000;
			chann.mid = "audio";
			local_channels_.push_back(chann);
		}

		void h323_call::add_video_channel_h264()
		{
			media_channel_t chann;
			chann.session_id = 2;
			chann.media_type = media_type_video;
			chann.codec_type = codec_type_h264;
			chann.payload_type = 96;
			chann.frequency = 90000;
			chann.type = media_channel_type_t::notmal;
			chann.profile = 0x40;
			chann.level = 0x005c;
			chann.mbps = 540;
			chann.mfs = 34;
			chann.max_nal_size = 1400;
			chann.mid = "video";
			local_channels_.push_back(chann);
		}

		void h323_call::add_extend_video_channel_h264()
		{
			{
				media_channel_t chann;
				chann.payload_type = 97;
				chann.session_id = 32;
				chann.media_type = media_type_video;
				chann.codec_type = codec_type_h264;
				chann.type = media_channel_type_t::extend;
				chann.profile = 0x40;
				chann.level = 0x005c;
				chann.mbps = 540;
				chann.mfs = 34;
				chann.max_nal_size = 1400;
				chann.mid = "extend_video";
				local_channels_.push_back(chann);
			}
			{
				media_channel_t chann;
				chann.media_type = media_type_unknown;
				chann.type = media_channel_type_t::h239_control;
				local_channels_.push_back(chann);
			}
		}

		std::string h323_call::id()const
		{
			return (const std::string&)call_id_.AsString();
		}

		bool h323_call::start(bool audio, bool video)
		{
			bool experted = false;
			if(!active_.compare_exchange_strong(experted, true))
				return true;

			//h245_sending_queue_->set_logger(log_);
			if (audio)
			{
				add_audio_channel_g711();
			}
			if (video)
			{
				add_video_channel_h264();
				add_extend_video_channel_h264();
			}

			h225_buffer_.clear();
			h245_buffer_.clear();

			if (!h225_skt_)
			{
				std::error_code ec;
				h225_skt_ = std::make_shared<asio::ip::tcp::socket>(ep_->io_context());
				h225_skt_->open(h225_addr_.protocol(), ec);
				if (ec)
				{
					if (log_)
					{
						std::string url = url_.to_string();
						log_->error("H.323 call error: Open h225 socket failed. url={}", url)->flush();
					}
					stop(call::reason_code_t::h245_listen_failed);
					return false;
				}
				h225_skt_->connect(h225_addr_, ec);
				if (ec)
				{
					if (log_)
					{
						std::string url = url_.to_string();
						std::string h225ip = h225_addr_.address().to_string(ec);
						log_->error("H.323 call error: Connect h225 addr failed. url={},addr={}:{}", url, h225ip,h225_addr_.port())->flush();
					}
					stop(call::reason_code_t::h225_connect_failed);
					return false;
				}
			}

			auto self = shared_from_this();

			h225_skt_->async_read_some(asio::buffer(h225_recv_buffer_), std::bind(&h323_call::handle_read_h225, this, self, std::placeholders::_1, std::placeholders::_2));
			

			if (init_setup_)
			{
				if (!this->send_setup(url_))
				{
					if (log_)
					{
						std::string url = url_.to_string();
						log_->error("H.323 call error: h225 send setup failed. url={}", url)->flush();
					}
					stop(call::reason_code_t::error);
					return false;
				}
			}
			if (init_facility_)
			{
				if (!this->send_facility(H225_FacilityReason::Choices::e_undefinedReason, false))
				{
					if (log_)
					{
						std::string url = url_.to_string();
						log_->error("H.323 call error: h225 send setup facility. url={}", url)->flush();
					}
					stop(call::reason_code_t::h245_listen_failed);
					return false;
				}
			}

			return true;
		}

		void h323_call::stop(voip::call::reason_code_t reason)
		{
			bool experted = true;
			if (!active_.compare_exchange_strong(experted, false))
				return;
			
			rtp_.stop();
			std::error_code ec;
			timer_.cancel(ec);
			if (h245_skt_)
			{
				this->close_local_channels();
				this->close_remote_channels();
				std::this_thread::sleep_for(std::chrono::milliseconds(500));
				this->send_end_session_command();
				std::this_thread::sleep_for(std::chrono::milliseconds(500));
				if (gkclient_)
				{
					gkclient_->send_disengage_request(H225_DisengageReason::e_undefinedReason, call_id_, conf_id_, call_ref_);
				}
				h245_skt_->cancel(ec);
			}
			h245_listen_.close(ec);

			if (h225_skt_)
			{
				this->send_release_complete(H225_ReleaseCompleteReason::Choices::e_undefinedReason, false);
				std::this_thread::sleep_for(std::chrono::milliseconds(500));
				h225_skt_->cancel(ec);
			}
			if (on_destroy)
			{
				auto self = shared_from_this();
				on_destroy(self, reason);
			}

		}



		bool h323_call::require_keyframe()
		{
			std::lock_guard<std::recursive_mutex> lk(opened_remote_channels_mutex_);
			for (auto itr = opened_remote_channels_.begin(); itr != opened_remote_channels_.end(); itr++)
			{
				if (itr->media_type == media_type_video)
				{
					std::lock_guard<std::recursive_mutex> lk2(mutex_);
					send_miscellaneous_command(itr->channel_number);
					if (log_)
					{
						std::string url = url_.to_string();
						log_->debug("H.323 require keyframe. url={}",url )->flush();
					}
				}
			}
			return true;
		}

		bool h323_call::answer()
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			if (!this->start_h245_listen())
			{
				return false;
			}
			if (!send_connect())
			{
				return false;
			}

			return true;
		}

		bool h323_call::refuse()
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			send_release_complete(H225_ReleaseCompleteReason::Choices::e_destinationRejection,false);
			return true;
		}

		bool h323_call::request_presentation_role()
		{
			if (!has_h239_control())
				return false;

			media_channel_t remote,local;
			if (!find_extend_video(remote,local))
				return false;

			if (h239_token_owned_)
				return false;
			
			h239_token_channel_ = local.channel_number;
			h239_terminal_label_ = get_local_terminal_label();
			h239_symmetry_breaking_ = PRandom::Number(1, 127);
			return this->send_h239_presentation_request(h239_token_channel_, h239_symmetry_breaking_, h239_terminal_label_);
		}

		void h323_call::release_presentation_role()
		{
			if (h239_token_owned_)
			{
				h239_token_owned_ = false;
				send_h239_presentation_release(h239_token_channel_, h239_terminal_label_);
				h239_token_channel_ = 0;
				h239_terminal_label_ = 0;
				h239_symmetry_breaking_ = 0;
				on_h239_presentation_role_changed(true, false);
				{
					std::lock_guard<std::recursive_mutex> lk(opened_local_channels_mutex_);
					for (auto itr = opened_local_channels_.begin(); itr != opened_local_channels_.end(); )
					{
						if (itr->type == media_channel_type_t::extend)
						{
							this->send_close_logical_channel(itr->channel_number, H245_CloseLogicalChannel_reason::e_unknown);
							itr = opened_local_channels_.erase(itr);
						}
						else
						{
							itr++;
						}
					}
				}
				remove_media_stream("extend_video");
			}
		}

		bool h323_call::has_presentation_role()
		{
			return h239_token_owned_;
		}



		void h323_call::setup(const voip_uri& url, const std::string& call_id, const std::string& conf_id, int call_ref)
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			if (call_id.empty())
			{
				call_id_ = (PString)sys::util::uuid(sys::uuid_fmt_t::bar);
			}
			else
			{
				call_id_ = (PString)call_id;
			}
			if (conf_id.empty())
			{
				conf_id_ = (PString)sys::util::uuid(sys::uuid_fmt_t::bar);
			}
			else
			{
				conf_id_ = (PString)conf_id;
			}
			call_ref_ = call_ref;
			url_ = url;
			remote_alias_ = url.username;
			init_setup_ = true;
		}

		void h323_call::facility(const voip_uri& url, const PGloballyUniqueID& call_id, const PGloballyUniqueID& conf_id)
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			call_id_ = call_id;
			conf_id_ = conf_id;
			url_ = url;
			init_facility_ = true;
		}

		bool h323_call::start_h245_listen()
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);

			auto self = shared_from_this();

			asio::error_code ec;
			h245_listen_.open(asio::ip::tcp::v4(),ec);
			if (ec)
			{
				if (log_)
				{
					std::string url = url_.to_string();
					log_->error("H.323 start h245 listen failed. url={}", url)->flush();
				}
				return false;
			}
			h245_listen_.set_option(asio::socket_base::reuse_address(true), ec);

			bool has_listened = false;
			for (int i = 0; i < h245_ports_->count(); i++) {
				h245_port_ = h245_ports_->get_next_port();
				asio::ip::tcp::endpoint ep(asio::ip::tcp::v4(), h245_port_);
				h245_listen_.bind(ep, ec);
				if (!ec)
				{
					has_listened = true;
					break;
				}

			}
			if (!has_listened)
			{
				if (log_)
				{
					std::string url = url_.to_string();
					log_->error("H.323 start h245 listen failed:port bind failed. url={},port={}", url, h245_port_)->flush();
				}
				return false;
			}


			h245_listen_.listen(1,ec);
			if (ec) {
				if (log_)
				{
					std::string url = url_.to_string();
					log_->error("H.323 start h245 listen failed:port listen failed. url={},port={}", url, h245_port_)->flush();
				}
				return false;
			}
			h245_address_ = nat_address_;

			h245_listen_.async_accept(std::bind(&h323_call::handle_accept_h245,this,self,std::placeholders::_1,std::placeholders::_2));


			if (log_)
			{
				std::string url = url_.to_string();
				log_->debug("H.323 start h245 listen. url={},port={}", url, h245_port_)->flush();
			}
			return true;
		}

		bool h323_call::start_h245_connect(const std::string& h245_addr, int h245_port)
		{
			auto self = shared_from_this();
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			std::error_code ec;

			h245_connect_ep_.address(asio::ip::address::from_string(h245_addr, ec));
			h245_connect_ep_.port(h245_port);
			if (ec)
				return false;

			h245_skt_ = std::make_shared<asio::ip::tcp::socket>(ep_->io_context());
			h245_skt_->open(h245_connect_ep_.protocol(), ec);
			if (ec) 
			{
				if (log_)
				{
					std::string url = url_.to_string();
					log_->error("H.323 start h245 connect failed: url={},h245addr={}:{}", url, h245_addr, h245_port)->flush();
				}
				return false;
			}

			h245_skt_->async_connect(h245_connect_ep_, std::bind(&h323_call::handle_connect_h245, this, self, std::placeholders::_1));
			

			if (log_)
			{
				std::string url = url_.to_string();
				log_->debug("H.323 start h245 connect url={},h245addr={}:{}", url, h245_addr, h245_port)->flush();
			}
			return true;
		}

		bool h323_call::find_compatible_channel(const media_channel_t& chann, media_channel_t& cmp)
		{
			for (auto itr = local_channels_.begin(); itr != local_channels_.end(); itr++)
			{
				if (itr->media_type == chann.media_type 
					&& itr->codec_type == chann.codec_type
					&& itr->type== chann.type)
				{
					if (chann.media_type == media_type_video)
					{
						if (itr->profile == chann.profile)
						{
							cmp = *itr;
							return true;
						}
					}
					else
					{
						cmp = *itr;
						return true;
					}
				}
			}

			return false;
		}

		void h323_call::start_open_local_chnnnels()
		{
			bool has_audio = false, has_video = false;
			//opened_
			{
				std::lock_guard<std::recursive_mutex> lk(opened_local_channels_mutex_);
				opened_local_channels_.clear();
			}
			for (auto itr = remote_channels_.begin(); itr != remote_channels_.end(); itr++)
			{
				media_channel_t local_chann;
				if (find_compatible_channel(*itr, local_chann))
				{
					if (itr->media_type == media_type_audio&&!has_audio)
					{
						has_audio = true;
						auto codec = h323_util::convert_audio_codec_type_back(itr->codec_type);
						itr->session_id = local_chann.session_id;
						auto ms = get_media_stream(local_chann.media_type, local_chann.mid);
						if (ms)
						{
							{
								std::lock_guard<std::recursive_mutex> lk(opened_local_channels_mutex_);
								opened_local_channels_.push_back(*itr);
							}
							auto sdp=ms->get_local_sdp();
							this->send_open_audio_logical_channel(local_chann.channel_number, codec, itr->session_id, local_chann.payload_type,
								PIPSocket::Address(sdp.rtcp_address), sdp.rtcp_port);
						}
					}
					else if (itr->media_type == media_type_video&&!has_video&&itr->type==media_channel_type_t::notmal)
					{
						has_video = true;
						auto codec =  H245_VideoCapability::Choices::e_genericVideoCapability;
						itr->session_id = local_chann.session_id;

						auto ms = get_media_stream(local_chann.media_type,local_chann.mid);
						if (ms)
						{
							{
								std::lock_guard<std::recursive_mutex> lk(opened_local_channels_mutex_);
								opened_local_channels_.push_back(*itr);
							}
							auto sdp = ms->get_local_sdp();
							H245_ArrayOf_GenericParameter collapsing;
							h323_util::fill_video_collapsing(collapsing, itr->profile, itr->level, itr->mbps, itr->mfs, itr->max_nal_size);

							this->send_open_video_logical_channel(local_chann.channel_number, codec, itr->session_id, local_chann.max_bitrate, local_chann.payload_type,
								PIPSocket::Address(sdp.rtcp_address), sdp.rtcp_port, collapsing);

						}
					}
				}
			}


			rtp_.start();
		}

		void h323_call::close_remote_channels()
		{
			std::lock_guard<std::recursive_mutex> lk(opened_remote_channels_mutex_);
			for (auto itr = opened_remote_channels_.begin(); itr != opened_remote_channels_.end(); itr++)
			{
				this->send_request_channel_close(itr->channel_number, H245_RequestChannelClose_reason::e_unknown);
			}
			opened_remote_channels_.clear();
		}

		void h323_call::close_local_channels()
		{
			std::lock_guard<std::recursive_mutex> lk(opened_local_channels_mutex_);
			for (auto itr = opened_local_channels_.begin(); itr != opened_local_channels_.end(); itr++)
			{
				this->send_close_logical_channel(itr->channel_number, H245_CloseLogicalChannel_reason::e_unknown);
			}
			opened_local_channels_.clear();
		}


		bool h323_call::get_local_channel(int chann_num, media_channel_t& chann)
		{
			for (auto itr = local_channels_.begin(); itr != local_channels_.end(); itr++)
			{
				if (itr->channel_number == chann_num)
				{
					chann = *itr;
					return true;
				}
			}
			return false;
		}

		bool h323_call::get_remote_channel(int chann_num, media_channel_t& chann)
		{
			for (auto itr = remote_channels_.begin(); itr != remote_channels_.end(); itr++)
			{
				if (itr->channel_number == chann_num)
				{
					chann = *itr;
					return true;
				}
			}
			return false;
		}

		litertp::media_stream_ptr h323_call::get_media_stream(media_type_t mt, const std::string& mid)
		{
			auto ms=rtp_.get_media_stream(mid);
			if (ms)
				return ms;

			ms = rtp_.create_media_stream(mid, mt, 0, nat_address_.c_str(), rtp_ports_, false);
			if (ms)
			{
				ms->use_rtp_address(true);
				if (mt == media_type_video)
				{
					ms->on_require_keyframe.add(s_litertp_on_require_keyframe, this);
				}
			}
			return ms;
		}

		bool h323_call::remove_media_stream(const std::string& mid)
		{
			return rtp_.remove_media_stream(mid);
		}
		
		bool h323_call::has_h239_control()
		{
			for (auto itr = remote_channels_.begin(); itr != remote_channels_.end(); itr++)
			{
				if (itr->type == media_channel_type_t::h239_control)
					return true;
			}
			return false;
		}

		bool h323_call::find_extend_video(media_channel_t& remote, media_channel_t& local)
		{
			for (auto itr = remote_channels_.begin(); itr != remote_channels_.end(); itr++)
			{
				if (itr->media_type == media_type_video && itr->type == media_channel_type_t::extend)
				{
					media_channel_t local_chann;
					if (find_compatible_channel(*itr, local_chann))
					{
						remote = *itr;
						local = local_chann;
						return true;
					}
				}
			}
			return false;
		}

		bool h323_call::send_setup(const voip_uri& url)
		{
			H225_H323_UserInformation ui;
			ui.SetTag(H225_H323_UserInformation::e_user_data);
			ui.m_h323_uu_pdu.m_h323_message_body.SetTag(H225_H323_UU_PDU_h323_message_body::e_setup);
			ui.m_h323_uu_pdu.IncludeOptionalField(H225_H323_UU_PDU::e_h245Tunneling);
			ui.m_h323_uu_pdu.m_h245Tunneling = h245_tunneling;

			H225_Setup_UUIE& uuie = (H225_Setup_UUIE&)ui.m_h323_uu_pdu.m_h323_message_body;
			uuie.m_protocolIdentifier.SetValue(H225_ProtocolID, PARRAYSIZE(H225_ProtocolID));
			
			uuie.IncludeOptionalField(H225_Setup_UUIE::e_sourceAddress);
			H323SetAliasAddresses((PStringArray)local_alias_, uuie.m_sourceAddress);

			uuie.m_sourceInfo.IncludeOptionalField(H225_EndpointType::e_terminal);
			uuie.m_sourceInfo.IncludeOptionalField(H225_EndpointType::e_vendor);
			h323_util::fill_vendor(uuie.m_sourceInfo.m_vendor);

			PString dst_alias = url.username;
			if (!dst_alias.IsEmpty())
			{
				uuie.IncludeOptionalField(H225_Setup_UUIE::e_destinationAddress);
				PStringArray aliases;
				aliases += dst_alias;
				H323SetAliasAddresses(aliases, uuie.m_destinationAddress);
			}
			
			PString dst_addr = url.host;
			if (!dst_addr.IsEmpty())
			{
				uuie.IncludeOptionalField(H225_Setup_UUIE::e_destCallSignalAddress);
				h323_util::h225_transport_address_set(uuie.m_destCallSignalAddress, PIPSocket::Address(dst_addr), url.port!=0 ? url.port : 1720);
			}

			
			

			uuie.m_conferenceID = conf_id_;
			uuie.m_conferenceGoal.SetTag(H225_Setup_UUIE_conferenceGoal::e_create);
			uuie.m_callType.SetTag(H225_CallType::e_pointToPoint);
			//H225_Setup_UUIE::e_sourceAddress

			std::error_code ec;
			auto h225_local_ep=h225_skt_->local_endpoint(ec);
			int h225_port = h225_local_ep.port();
			if (!ec)
			{
				uuie.IncludeOptionalField(H225_Setup_UUIE::e_sourceCallSignalAddress);
				h323_util::h225_transport_address_set(uuie.m_sourceCallSignalAddress, PIPSocket::Address(nat_address_), h225_port);
			}
			uuie.IncludeOptionalField(H225_Setup_UUIE::e_callIdentifier);
			uuie.m_callIdentifier.m_guid = call_id_;


			//uuie.IncludeOptionalField(H225_Setup_UUIE::e_tokens);
			//dh_.ToTokens(uuie.m_tokens);

			uuie.IncludeOptionalField(H225_Setup_UUIE::e_mediaWaitForConnect);
			uuie.m_mediaWaitForConnect = false;

			uuie.IncludeOptionalField(H225_Setup_UUIE::e_canOverlapSend);
			uuie.m_canOverlapSend = false;

			uuie.IncludeOptionalField(H225_Setup_UUIE::e_multipleCalls);
			uuie.m_multipleCalls = false;

			uuie.IncludeOptionalField(H225_Setup_UUIE::e_maintainConnection);
			uuie.m_maintainConnection = false;

			uuie.IncludeOptionalField(H225_Setup_UUIE::e_supportedFeatures);
			H225_FeatureDescriptor* item=new H225_FeatureDescriptor();
			item->m_id.SetTag(H225_GenericIdentifier::e_standard);
			(PASN_Integer&)item->m_id = 19;
			item->IncludeOptionalField(H225_FeatureDescriptor::e_parameters);
			H460_FeatureID* supportTransmitMultiplexedMedia_ID=new H460_FeatureID(1);
			item->m_parameters.RemoveAll();
			item->m_parameters.Append(supportTransmitMultiplexedMedia_ID);
			uuie.m_supportedFeatures.Append(item);
			

			PPER_Stream stream;
			ui.Encode(stream);
			stream.CompleteEncoding();
			Q931 q931;
			q931.BuildSetup((int)call_ref_);
			unsigned int transfer_rate = max_bitrate_ / 64;
			q931.SetBearerCapabilities(Q931::InformationTransferCapability::TransferSpeech, transfer_rate); // 30 or 8  * 64Kbps
			q931.SetDisplayName(local_alias_);
			q931.SetIE(Q931::InformationElementCodes::UserUserIE, stream);
			call_ref_ = q931.GetCallReference();
			PBYTEArray buffer;
			q931.Encode(buffer);
			PBYTEArray tpktbuf=tpkt::serialize(buffer);
			return h225_sending_queue_->send(h225_skt_, std::string((const char*)tpktbuf.GetPointer(), tpktbuf.GetSize()));
		}

		bool h323_call::send_facility(H225_FacilityReason::Choices reason, bool from_dest,bool heartbeat)
		{
			H225_H323_UserInformation ui;
			ui.SetTag(H225_H323_UserInformation::e_user_data);
			ui.m_h323_uu_pdu.m_h323_message_body.SetTag(H225_H323_UU_PDU_h323_message_body::e_facility);
			ui.m_h323_uu_pdu.IncludeOptionalField(H225_H323_UU_PDU::e_h245Tunneling);
			ui.m_h323_uu_pdu.m_h245Tunneling = h245_tunneling;

			H225_Facility_UUIE& uuie = (H225_Facility_UUIE&)ui.m_h323_uu_pdu.m_h323_message_body;
			uuie.m_protocolIdentifier.SetValue(H225_ProtocolID, PARRAYSIZE(H225_ProtocolID));
			uuie.m_reason.SetTag(reason);

			uuie.IncludeOptionalField(H225_Facility_UUIE::e_callIdentifier);
			uuie.m_callIdentifier.m_guid = call_id_;

			uuie.IncludeOptionalField(H225_Facility_UUIE::e_multipleCalls);
			uuie.m_multipleCalls = false;

			uuie.IncludeOptionalField(H225_Facility_UUIE::e_maintainConnection);
			uuie.m_maintainConnection = false;

			if (heartbeat)
			{
				uuie.IncludeOptionalField(H225_Facility_UUIE::e_fastConnectRefused);
			}
			else
			{
				H225_GenericData* gen = new H225_GenericData();
				gen->m_id.SetTag(H225_GenericIdentifier::e_standard);
				((PASN_Integer&)gen->m_id) = 18;
				gen->IncludeOptionalField(H225_GenericData::e_parameters);
				gen->m_parameters.RemoveAll();
				H225_EnumeratedParameter* gen_pm = new H225_EnumeratedParameter();
				gen_pm->m_id.SetTag(H225_GenericIdentifier::e_standard);
				((PASN_Integer&)gen_pm->m_id) = 1; // IncomingCallIndication
				gen_pm->IncludeOptionalField(H225_EnumeratedParameter::e_content);
				gen_pm->m_content.SetTag(H225_Content::e_raw);

				H46018_IncomingCallIndication incall;
				incall.m_callID.m_guid = call_id_;

				std::error_code ec;
				auto h225_local_ep = h225_skt_->local_endpoint(ec);
				int h225_port = h225_local_ep.port();
				h323_util::h225_transport_address_set(incall.m_callSignallingAddress, PIPSocket::Address(nat_address_), (WORD)h225_port);

				((PASN_OctetString&)(gen_pm->m_content)).EncodeSubType(incall);

				gen->m_parameters.Append(gen_pm);

				ui.m_h323_uu_pdu.IncludeOptionalField(H225_H323_UU_PDU::e_genericData);
				ui.m_h323_uu_pdu.m_genericData.RemoveAll();
				ui.m_h323_uu_pdu.m_genericData.Append(gen);
			}
			PPER_Stream stream;
			ui.Encode(stream);
			stream.CompleteEncoding();
			Q931 q931;
			q931.BuildFacility(call_ref_, from_dest);
			q931.SetDisplayName(local_alias_);
			q931.SetIE(Q931::InformationElementCodes::UserUserIE, stream);

			PBYTEArray buffer;
			q931.Encode(buffer);
			PBYTEArray tpktbuf = tpkt::serialize(buffer);

			return h225_sending_queue_->send(h225_skt_, std::string((const char*)tpktbuf.GetPointer(), tpktbuf.GetSize()));
		}

		bool h323_call::send_call_proceeding()
		{
			H225_H323_UserInformation ui;
			ui.SetTag(H225_H323_UserInformation::e_user_data);
			ui.m_h323_uu_pdu.m_h323_message_body.SetTag(H225_H323_UU_PDU_h323_message_body::e_callProceeding);
			ui.m_h323_uu_pdu.IncludeOptionalField(H225_H323_UU_PDU::e_h245Tunneling);
			ui.m_h323_uu_pdu.m_h245Tunneling = h245_tunneling;

			H225_CallProceeding_UUIE& uuie = (H225_CallProceeding_UUIE&)ui.m_h323_uu_pdu.m_h323_message_body;
			uuie.m_protocolIdentifier.SetValue(H225_ProtocolID, PARRAYSIZE(H225_ProtocolID));

			uuie.m_destinationInfo.IncludeOptionalField(H225_EndpointType::e_terminal);
			uuie.m_destinationInfo.IncludeOptionalField(H225_EndpointType::e_vendor);
			h323_util::fill_vendor(uuie.m_destinationInfo.m_vendor);

			uuie.IncludeOptionalField(H225_CallProceeding_UUIE::e_callIdentifier);
			uuie.m_callIdentifier.m_guid = call_id_;

			uuie.IncludeOptionalField(H225_CallProceeding_UUIE::e_multipleCalls);
			uuie.m_multipleCalls = false;
			uuie.IncludeOptionalField(H225_CallProceeding_UUIE::e_maintainConnection);
			uuie.m_maintainConnection = false;



			PPER_Stream stream;
			ui.Encode(stream);
			stream.CompleteEncoding();
			Q931 q931;
			q931.BuildCallProceeding(call_ref_);
			q931.SetDisplayName(local_alias_);
			q931.SetIE(Q931::InformationElementCodes::UserUserIE, stream);

			PBYTEArray buffer;
			q931.Encode(buffer);
			PBYTEArray tpktbuf = tpkt::serialize(buffer);

			return h225_sending_queue_->send(h225_skt_, std::string((const char*)tpktbuf.GetPointer(), tpktbuf.GetSize()));
		}

		bool h323_call::send_alerting()
		{
			H225_H323_UserInformation ui;
			ui.SetTag(H225_H323_UserInformation::e_user_data);
			ui.m_h323_uu_pdu.m_h323_message_body.SetTag(H225_H323_UU_PDU_h323_message_body::e_alerting);
			ui.m_h323_uu_pdu.IncludeOptionalField(H225_H323_UU_PDU::e_h245Tunneling);
			ui.m_h323_uu_pdu.m_h245Tunneling = h245_tunneling;

			H225_Alerting_UUIE& uuie = (H225_Alerting_UUIE&)ui.m_h323_uu_pdu.m_h323_message_body;
			uuie.m_protocolIdentifier.SetValue(H225_ProtocolID, PARRAYSIZE(H225_ProtocolID));
			uuie.m_destinationInfo.IncludeOptionalField(H225_EndpointType::e_terminal);
			uuie.m_destinationInfo.IncludeOptionalField(H225_EndpointType::e_vendor);
			h323_util::fill_vendor(uuie.m_destinationInfo.m_vendor);

			uuie.IncludeOptionalField(H225_CallProceeding_UUIE::e_callIdentifier);
			uuie.m_callIdentifier.m_guid = call_id_;

			uuie.IncludeOptionalField(H225_CallProceeding_UUIE::e_multipleCalls);
			uuie.m_multipleCalls = false;
			uuie.IncludeOptionalField(H225_CallProceeding_UUIE::e_maintainConnection);
			uuie.m_maintainConnection = false;


			PPER_Stream stream;
			ui.Encode(stream);
			stream.CompleteEncoding();
			Q931 q931;
			q931.BuildAlerting(call_ref_);
			q931.SetDisplayName(local_alias_);
			q931.SetIE(Q931::InformationElementCodes::UserUserIE, stream);

			PBYTEArray buffer;
			q931.Encode(buffer);
			PBYTEArray tpktbuf = tpkt::serialize(buffer);
			return h225_sending_queue_->send(h225_skt_, std::string((const char*)tpktbuf.GetPointer(), tpktbuf.GetSize()));
		}

		bool h323_call::send_connect()
		{
			H225_H323_UserInformation ui;
			ui.SetTag(H225_H323_UserInformation::e_user_data);
			ui.m_h323_uu_pdu.m_h323_message_body.SetTag(H225_H323_UU_PDU_h323_message_body::e_connect);
			ui.m_h323_uu_pdu.IncludeOptionalField(H225_H323_UU_PDU::e_h245Tunneling);
			ui.m_h323_uu_pdu.m_h245Tunneling = h245_tunneling;

			H225_Connect_UUIE& uuie = (H225_Connect_UUIE&)ui.m_h323_uu_pdu.m_h323_message_body;
			uuie.m_protocolIdentifier.SetValue(H225_ProtocolID, PARRAYSIZE(H225_ProtocolID));
			uuie.m_destinationInfo.IncludeOptionalField(H225_EndpointType::e_terminal);
			uuie.m_destinationInfo.IncludeOptionalField(H225_EndpointType::e_vendor);
			h323_util::fill_vendor(uuie.m_destinationInfo.m_vendor);

			uuie.IncludeOptionalField(H225_CallProceeding_UUIE::e_h245Address);
			h323_util::h225_transport_address_set(uuie.m_h245Address, PIPSocket::Address(h245_address_), h245_port_);


			uuie.m_conferenceID = conf_id_;
			uuie.IncludeOptionalField(H225_CallProceeding_UUIE::e_callIdentifier);
			uuie.m_callIdentifier.m_guid = call_id_;

			uuie.IncludeOptionalField(H225_CallProceeding_UUIE::e_multipleCalls);
			uuie.m_multipleCalls = false;
			uuie.IncludeOptionalField(H225_CallProceeding_UUIE::e_maintainConnection);
			uuie.m_maintainConnection = false;


			PPER_Stream stream;
			ui.Encode(stream);
			stream.CompleteEncoding();
			Q931 q931;
			q931.BuildConnect(call_ref_);
			unsigned int transfer_rate = max_bitrate_ / 64;
			q931.SetBearerCapabilities(Q931::InformationTransferCapability::TransferSpeech, transfer_rate); // 30 or 8  * 64Kbps
			q931.SetDisplayName(local_alias_);
			q931.SetIE(Q931::InformationElementCodes::UserUserIE, stream);

			PBYTEArray buffer;
			q931.Encode(buffer);
			PBYTEArray tpktbuf = tpkt::serialize(buffer);
			return h225_sending_queue_->send(h225_skt_, std::string((const char*)tpktbuf.GetPointer(), tpktbuf.GetSize()));
		}

		bool h323_call::send_release_complete(H225_ReleaseCompleteReason::Choices reason, bool from_dest)
		{
			H225_H323_UserInformation ui;
			ui.SetTag(H225_H323_UserInformation::e_user_data);
			ui.m_h323_uu_pdu.m_h323_message_body.SetTag(H225_H323_UU_PDU_h323_message_body::e_releaseComplete);
			ui.m_h323_uu_pdu.IncludeOptionalField(H225_H323_UU_PDU::e_h245Tunneling);
			ui.m_h323_uu_pdu.m_h245Tunneling = h245_tunneling;

			H225_ReleaseComplete_UUIE& uuie = (H225_ReleaseComplete_UUIE&)ui.m_h323_uu_pdu.m_h323_message_body;
			uuie.m_protocolIdentifier.SetValue(H225_ProtocolID, PARRAYSIZE(H225_ProtocolID));
			uuie.IncludeOptionalField(H225_ReleaseComplete_UUIE::e_callIdentifier);
			uuie.m_callIdentifier.m_guid = call_id_;
			uuie.IncludeOptionalField(H225_ReleaseComplete_UUIE::e_reason);
			uuie.m_reason.SetTag(reason);

			H225_GenericData* item=new H225_GenericData();
			item->m_id.SetTag(H225_GenericIdentifier::e_standard);
			(PASN_Integer&)item->m_id = 19; // Signalling Traversal

			item->IncludeOptionalField(H225_GenericData::e_parameters);
			H460_FeatureID* supportTransmitMultiplexedMedia_ID=new H460_FeatureID(1);
			item->m_parameters.RemoveAll();
			item->m_parameters.Append(supportTransmitMultiplexedMedia_ID);
			ui.m_h323_uu_pdu.IncludeOptionalField(H225_H323_UU_PDU::e_genericData);
			ui.m_h323_uu_pdu.m_genericData.Append(item);


			PPER_Stream stream;
			ui.Encode(stream);
			stream.CompleteEncoding();
			Q931 q931;
			q931.BuildReleaseComplete(call_ref_, from_dest);
			q931.SetDisplayName(local_alias_);
			q931.SetIE(Q931::InformationElementCodes::UserUserIE, stream);

			PBYTEArray buffer;
			q931.Encode(buffer);
			PBYTEArray tpktbuf = tpkt::serialize(buffer);
			return h225_sending_queue_->send(h225_skt_, std::string((const char*)tpktbuf.GetPointer(), tpktbuf.GetSize()));
		}

		bool h323_call::send_empty()
		{
			H225_H323_UserInformation ui;
			ui.SetTag(H225_H323_UserInformation::e_user_data);
			ui.m_h323_uu_pdu.m_h323_message_body.SetTag(H225_H323_UU_PDU_h323_message_body::e_empty);
			ui.m_h323_uu_pdu.IncludeOptionalField(H225_H323_UU_PDU::e_h245Tunneling);
			ui.m_h323_uu_pdu.m_h245Tunneling = h245_tunneling;


			PPER_Stream stream;
			ui.Encode(stream);
			stream.CompleteEncoding();
			Q931 q931;
			q931.SetDisplayName(local_alias_);
			q931.SetIE(Q931::InformationElementCodes::UserUserIE, stream);

			PBYTEArray buffer;
			q931.Encode(buffer);
			PBYTEArray tpktbuf = tpkt::serialize(buffer);
			return h225_sending_queue_->send(h225_skt_, std::string((const char*)tpktbuf.GetPointer(), tpktbuf.GetSize()));
		}

		bool h323_call::send_terminal_capability_set()
		{
			if (!h245_skt_)
				return false;

			H245_MultimediaSystemControlMessage msg;
			msg.SetTag(H245_MultimediaSystemControlMessage::e_request);
			H245_RequestMessage& req = (H245_RequestMessage&)msg;
			req.SetTag(H245_RequestMessage::e_terminalCapabilitySet);
			H245_TerminalCapabilitySet& body = (H245_TerminalCapabilitySet&)req;
			body.m_sequenceNumber = request_seq_.fetch_add(1);

			body.m_protocolIdentifier.SetValue(H245_ProtocolID, PARRAYSIZE(H245_ProtocolID));
			
			body.IncludeOptionalField(H245_TerminalCapabilitySet::e_multiplexCapability);
			body.m_multiplexCapability.SetTag(H245_MultiplexCapability::e_h2250Capability);
			H245_H2250Capability& cap = (H245_H2250Capability&)body.m_multiplexCapability;
			cap.m_maximumAudioDelayJitter = 250;
			
			H245_MediaDistributionCapability* mdc = new H245_MediaDistributionCapability();
			mdc->m_centralizedAudio = false;
			mdc->m_centralizedControl = false;
			mdc->m_centralizedVideo = false;
			mdc->m_distributedAudio = false;
			mdc->m_distributedControl = false;
			mdc->m_distributedVideo = false;
			
			cap.m_receiveMultipointCapability.m_multicastCapability = false;
			cap.m_receiveMultipointCapability.m_multiUniCastConference = false;
			cap.m_receiveMultipointCapability.m_mediaDistributionCapability.RemoveAll();
			cap.m_receiveMultipointCapability.m_mediaDistributionCapability.Append(mdc);

			cap.m_transmitMultipointCapability.m_multicastCapability = false;
			cap.m_transmitMultipointCapability.m_multiUniCastConference = false;
			cap.m_transmitMultipointCapability.m_mediaDistributionCapability.RemoveAll();
			cap.m_transmitMultipointCapability.m_mediaDistributionCapability.RemoveAll();
			cap.m_transmitMultipointCapability.m_mediaDistributionCapability.Append(mdc->CloneAs<H245_MediaDistributionCapability>());

			cap.m_receiveAndTransmitMultipointCapability.m_multicastCapability = false;
			cap.m_receiveAndTransmitMultipointCapability.m_multiUniCastConference = false;
			cap.m_receiveAndTransmitMultipointCapability.m_mediaDistributionCapability.RemoveAll();
			cap.m_receiveAndTransmitMultipointCapability.m_mediaDistributionCapability.RemoveAll();
			cap.m_receiveAndTransmitMultipointCapability.m_mediaDistributionCapability.Append(mdc->CloneAs<H245_MediaDistributionCapability>());

			cap.m_mcCapability.m_centralizedConferenceMC = false;
			cap.m_mcCapability.m_decentralizedConferenceMC = false;

			cap.m_rtcpVideoControlCapability = false;

			cap.m_mediaPacketizationCapability.m_h261aVideoPacketization = false;
			cap.m_mediaPacketizationCapability.IncludeOptionalField(H245_MediaPacketizationCapability::e_rtpPayloadType);
			cap.m_mediaPacketizationCapability.m_rtpPayloadType.RemoveAll();
			H245_RTPPayloadType* pt = new H245_RTPPayloadType();
			pt->m_payloadDescriptor.SetTag(H245_RTPPayloadType_payloadDescriptor::e_oid);
			((PASN_ObjectId&)pt->m_payloadDescriptor) = OpalPluginCodec_Identifer_H264_NonInterleaved;
			cap.m_mediaPacketizationCapability.m_rtpPayloadType.Append(pt);
			H245_RTPPayloadType* pt2 = new H245_RTPPayloadType();
			pt2->m_payloadDescriptor.SetTag(H245_RTPPayloadType_payloadDescriptor::e_oid);
			((PASN_ObjectId&)pt2->m_payloadDescriptor) = OpalPluginCodec_Identifer_H264_Aligned;
			cap.m_mediaPacketizationCapability.m_rtpPayloadType.Append(pt2);

			cap.IncludeOptionalField(H245_H2250Capability::e_logicalChannelSwitchingCapability);
			cap.m_logicalChannelSwitchingCapability = false;
			cap.IncludeOptionalField(H245_H2250Capability::e_t120DynamicPortCapability);
			cap.m_t120DynamicPortCapability = true;
			
			body.IncludeOptionalField(H245_TerminalCapabilitySet::e_capabilityTable);
			body.IncludeOptionalField(H245_TerminalCapabilitySet::e_capabilityDescriptors);
			fill_capability(body.m_capabilityTable, body.m_capabilityDescriptors);

			
			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			PBYTEArray buf = tpkt::serialize(stream);

			return h245_sending_queue_->send(h245_skt_, std::string((const char*)buf.GetPointer(), buf.GetSize()));
		}

		bool h323_call::send_terminal_capability_set_ack(uint32_t seq)
		{
			if (!h245_skt_)
				return false;

			H245_MultimediaSystemControlMessage msg;
			msg.SetTag(H245_MultimediaSystemControlMessage::e_response);
			H245_ResponseMessage& rsp = (H245_ResponseMessage&)msg;
			rsp.SetTag(H245_ResponseMessage::e_terminalCapabilitySetAck);
			H245_TerminalCapabilitySetAck& body = (H245_TerminalCapabilitySetAck&)rsp;

			body.m_sequenceNumber=seq;

			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			PBYTEArray buf = tpkt::serialize(stream);
			return h245_sending_queue_->send(h245_skt_, std::string((const char*)buf.GetPointer(), buf.GetSize()));
		}

		bool h323_call::send_terminal_capability_set_reject(uint32_t seq, H245_TerminalCapabilitySetReject_cause::Choices cause)
		{
			if (!h245_skt_)
				return false;

			H245_MultimediaSystemControlMessage msg;
			msg.SetTag(H245_MultimediaSystemControlMessage::e_response);
			H245_ResponseMessage& rsp = (H245_ResponseMessage&)msg;
			rsp.SetTag(H245_ResponseMessage::e_terminalCapabilitySetReject);
			H245_TerminalCapabilitySetReject& body = (H245_TerminalCapabilitySetReject&)rsp;

			body.m_sequenceNumber = seq;
			body.m_cause.SetTag(cause);

			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			PBYTEArray buf = tpkt::serialize(stream);
			return h245_sending_queue_->send(h245_skt_, std::string((const char*)buf.GetPointer(), buf.GetSize()));
		}

		bool h323_call::send_master_slave_determination()
		{
			if (!h245_skt_)
				return false;
			H245_MultimediaSystemControlMessage msg;
			msg.SetTag(H245_MultimediaSystemControlMessage::e_request);
			H245_RequestMessage& req = (H245_RequestMessage&)msg;
			req.SetTag(H245_RequestMessage::e_masterSlaveDetermination);
			H245_MasterSlaveDetermination& body = (H245_MasterSlaveDetermination&)req;

			body.m_terminalType = terminal_type_;
			body.m_statusDeterminationNumber = determination_number_;
			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			PBYTEArray buf = tpkt::serialize(stream);
			return h245_sending_queue_->send(h245_skt_, std::string((const char*)buf.GetPointer(), buf.GetSize()));
		}

		bool h323_call::send_master_slave_determination_ack(bool is_master)
		{
			if (!h245_skt_)
				return false;

			H245_MultimediaSystemControlMessage msg;
			msg.SetTag(H245_MultimediaSystemControlMessage::e_response);
			H245_ResponseMessage& rsp = (H245_ResponseMessage&)msg;
			rsp.SetTag(H245_ResponseMessage::e_masterSlaveDeterminationAck);
			H245_MasterSlaveDeterminationAck& body = (H245_MasterSlaveDeterminationAck&)rsp;

			if (is_master)
			{
				body.m_decision.SetTag(H245_MasterSlaveDeterminationAck_decision::Choices::e_master);
			}
			else
			{
				body.m_decision.SetTag(H245_MasterSlaveDeterminationAck_decision::Choices::e_slave);
			}
			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			PBYTEArray buf = tpkt::serialize(stream);
			return h245_sending_queue_->send(h245_skt_, std::string((const char*)buf.GetPointer(), buf.GetSize()));
		}

		bool h323_call::send_master_slave_determination_reject()
		{
			if (!h245_skt_)
				return false;

			H245_MultimediaSystemControlMessage msg;
			msg.SetTag(H245_MultimediaSystemControlMessage::e_response);
			H245_ResponseMessage& rsp = (H245_ResponseMessage&)msg;
			rsp.SetTag(H245_ResponseMessage::e_masterSlaveDeterminationReject);
			H245_MasterSlaveDeterminationReject& body = (H245_MasterSlaveDeterminationReject&)rsp;

			body.m_cause.SetTag(H245_MasterSlaveDeterminationReject_cause::e_identicalNumbers);

			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			PBYTEArray buf = tpkt::serialize(stream);
			return h245_sending_queue_->send(h245_skt_, std::string((const char*)buf.GetPointer(), buf.GetSize()));
		}

		bool h323_call::send_generic_indication()
		{
			if (!h245_skt_)
				return false;

			H245_MultimediaSystemControlMessage msg;
			msg.SetTag(H245_MultimediaSystemControlMessage::e_indication);
			H245_IndicationMessage& ind = (H245_IndicationMessage&)msg;
			ind.SetTag(H245_IndicationMessage::e_genericIndication);
			H245_GenericMessage& body = (H245_GenericMessage&)ind;

			body.m_messageIdentifier.SetTag(H245_CapabilityIdentifier::e_standard);
			((PASN_ObjectId&)body.m_messageIdentifier).SetValue("0.0.8.460.18.0.1");
			body.IncludeOptionalField(H245_GenericMessage::e_subMessageIdentifier);
			body.m_subMessageIdentifier = 1;
			body.IncludeOptionalField(H245_GenericMessage::e_messageContent);

			H245_GenericParameter* pm = new H245_GenericParameter();
			pm->m_parameterIdentifier.SetTag(H245_ParameterIdentifier::e_standard);
			((PASN_Integer&)pm->m_parameterIdentifier) = 1;
			pm->m_parameterValue.SetTag(H245_ParameterValue::e_octetString);
			((PASN_OctetString&)pm->m_parameterValue).SetValue(call_id_);
			body.m_messageContent.Append(pm);

			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			PBYTEArray buf = tpkt::serialize(stream);
			return h245_sending_queue_->send(h245_skt_, std::string((const char*)buf.GetPointer(), buf.GetSize()));
		}

		bool h323_call::send_miscellaneous_indication(int logical_channel)
		{
			if (!h245_skt_)
				return false;

			H245_MultimediaSystemControlMessage msg;
			msg.SetTag(H245_MultimediaSystemControlMessage::e_indication);
			H245_IndicationMessage& ind = (H245_IndicationMessage&)msg;
			ind.SetTag(H245_IndicationMessage::e_miscellaneousIndication);
			H245_MiscellaneousIndication& body = (H245_MiscellaneousIndication&)ind;
			
			body.m_logicalChannelNumber = logical_channel;
			body.m_type.SetTag(H245_MiscellaneousIndication_type::e_logicalChannelActive);

			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			PBYTEArray buf = tpkt::serialize(stream);
			return h245_sending_queue_->send(h245_skt_, std::string((const char*)buf.GetPointer(), buf.GetSize()));
		}

		bool h323_call::send_miscellaneous_command(int logical_channel)
		{
			if (!h245_skt_)
				return false;

			H245_MultimediaSystemControlMessage msg;
			msg.SetTag(H245_MultimediaSystemControlMessage::e_command);
			H245_CommandMessage& cmd = (H245_CommandMessage&)msg;
			cmd.SetTag(H245_CommandMessage::e_miscellaneousCommand);
			H245_MiscellaneousCommand& body = (H245_MiscellaneousCommand&)cmd;


			body.m_logicalChannelNumber = logical_channel;
			body.m_type.SetTag(H245_MiscellaneousCommand_type::e_videoFastUpdatePicture);

			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			PBYTEArray buf = tpkt::serialize(stream);
			return h245_sending_queue_->send(h245_skt_, std::string((const char*)buf.GetPointer(), buf.GetSize()));
		}

		bool h323_call::send_flow_control_indication()
		{
			if (!h245_skt_)
				return false;
			H245_MultimediaSystemControlMessage msg;
			msg.SetTag(H245_MultimediaSystemControlMessage::e_indication);
			H245_IndicationMessage& ind = (H245_IndicationMessage&)msg;
			ind.SetTag(H245_IndicationMessage::e_flowControlIndication);
			H245_FlowControlIndication& body = (H245_FlowControlIndication&)ind;

			body.m_scope.SetTag(H245_FlowControlIndication_scope::e_logicalChannelNumber);
			body.m_scope = 2;
			body.m_restriction.SetTag(H245_FlowControlIndication_restriction::e_maximumBitRate);
			body.m_restriction = 640;

			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			PBYTEArray buf = tpkt::serialize(stream);
			return h245_sending_queue_->send(h245_skt_, std::string((const char*)buf.GetPointer(), buf.GetSize()));
		}

		bool h323_call::send_request_channel_close(int logical_channel, H245_RequestChannelClose_reason::Choices reason)
		{
			if (!h245_skt_)
				return false;
			H245_MultimediaSystemControlMessage msg;
			msg.SetTag(H245_MultimediaSystemControlMessage::e_request);
			H245_RequestMessage& req = (H245_RequestMessage&)msg;
			req.SetTag(H245_RequestMessage::e_requestChannelClose);
			H245_RequestChannelClose& body = (H245_RequestChannelClose&)req;

			body.m_forwardLogicalChannelNumber = logical_channel;
			body.IncludeOptionalField(H245_RequestChannelClose::e_reason);
			body.m_reason = reason;

			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			PBYTEArray buf = tpkt::serialize(stream);
			return h245_sending_queue_->send(h245_skt_, std::string((const char*)buf.GetPointer(), buf.GetSize()));
		}

		bool h323_call::send_request_channel_close_ack(int logical_channel)
		{
			if (!h245_skt_)
				return false;
			H245_MultimediaSystemControlMessage msg;
			msg.SetTag(H245_MultimediaSystemControlMessage::e_response);
			H245_ResponseMessage& rsp = (H245_ResponseMessage&)msg;
			rsp.SetTag(H245_ResponseMessage::e_requestChannelCloseAck);
			H245_RequestChannelCloseAck& body = (H245_RequestChannelCloseAck&)rsp;
			
			body.m_forwardLogicalChannelNumber = logical_channel;

			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			PBYTEArray buf = tpkt::serialize(stream);
			return h245_sending_queue_->send(h245_skt_, std::string((const char*)buf.GetPointer(), buf.GetSize()));
		}

		bool h323_call::send_request_channel_close_reject(int logical_channel, H245_RequestChannelCloseReject_cause::Choices cause)
		{
			if (!h245_skt_)
				return false;
			H245_MultimediaSystemControlMessage msg;
			msg.SetTag(H245_MultimediaSystemControlMessage::e_response);
			H245_ResponseMessage& rsp = (H245_ResponseMessage&)msg;
			rsp.SetTag(H245_ResponseMessage::e_requestChannelCloseReject);
			H245_RequestChannelCloseReject& body = (H245_RequestChannelCloseReject&)rsp;
			
			body.m_forwardLogicalChannelNumber = logical_channel;
			body.m_cause = cause;

			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			PBYTEArray buf = tpkt::serialize(stream);
			return h245_sending_queue_->send(h245_skt_, std::string((const char*)buf.GetPointer(), buf.GetSize()));
		}

		bool h323_call::send_open_audio_logical_channel(int logical_channel, H245_AudioCapability::Choices codec, int session_id,
			int payload_type,const PIPSocket::Address& local_media_ctrl_address, WORD local_media_ctrl_port)
		{
			if (!h245_skt_)
				return false;
			H245_MultimediaSystemControlMessage msg;
			msg.SetTag(H245_MultimediaSystemControlMessage::e_request);
			H245_RequestMessage& req = (H245_RequestMessage&)msg;
			req.SetTag(H245_RequestMessage::e_openLogicalChannel);
			H245_OpenLogicalChannel& body = (H245_OpenLogicalChannel&)req;
			
			body.m_forwardLogicalChannelNumber = logical_channel;
			body.m_forwardLogicalChannelParameters.m_dataType.SetTag(H245_DataType::e_audioData);
			H245_AudioCapability& cap = (H245_AudioCapability&)body.m_forwardLogicalChannelParameters.m_dataType;
			cap.SetTag(codec);
			((PASN_Integer&)cap) = 20;

			body.m_forwardLogicalChannelParameters.m_multiplexParameters.SetTag(H245_OpenLogicalChannel_forwardLogicalChannelParameters_multiplexParameters::e_h2250LogicalChannelParameters);
			H245_H2250LogicalChannelParameters& pms = (H245_H2250LogicalChannelParameters&)body.m_forwardLogicalChannelParameters.m_multiplexParameters;
			pms.m_sessionID = session_id;
			pms.IncludeOptionalField(H245_H2250LogicalChannelParameters::e_mediaGuaranteedDelivery);

			if (payload_type >= pms.m_dynamicRTPPayloadType.GetLowerLimit()) {
				pms.IncludeOptionalField(H245_H2250LogicalChannelParameters::e_dynamicRTPPayloadType);
				pms.m_dynamicRTPPayloadType = payload_type;
			}

			pms.m_mediaGuaranteedDelivery = false;
			pms.IncludeOptionalField(H245_H2250LogicalChannelParameters::e_mediaControlChannel);
			h323_util::h245_transport_unicast_address_set(pms.m_mediaControlChannel, local_media_ctrl_address, local_media_ctrl_port);

			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			PBYTEArray buf = tpkt::serialize(stream);

			if (log_)
			{
				std::string url = url_.to_string();
				std::string addr = (const std::string&)local_media_ctrl_address.AsString();
				log_->debug("H.323 send_open_audio_logical_channel: url={}, chann={},code={},pt={},rtcp={}:{}",
					url, logical_channel, codec, payload_type, addr, local_media_ctrl_port)->flush();
			}

			return h245_sending_queue_->send(h245_skt_, std::string((const char*)buf.GetPointer(), buf.GetSize()));
		}

		bool h323_call::send_open_video_logical_channel(int logical_channel, H245_VideoCapability::Choices codec, int session_id,int max_bitrate,
			int payload_type,const PIPSocket::Address& local_media_ctrl_address,WORD local_media_ctrl_port, const H245_ArrayOf_GenericParameter& collapsing)
		{
			if (!h245_skt_)
				return false;
			H245_MultimediaSystemControlMessage msg;
			msg.SetTag(H245_MultimediaSystemControlMessage::e_request);
			H245_RequestMessage& req = (H245_RequestMessage&)msg;
			req.SetTag(H245_RequestMessage::e_openLogicalChannel);
			H245_OpenLogicalChannel& body = (H245_OpenLogicalChannel&)req;
			
			body.m_forwardLogicalChannelNumber = logical_channel;
			body.m_forwardLogicalChannelParameters.m_dataType.SetTag(H245_DataType::e_videoData);

			if (codec == H245_VideoCapability::e_genericVideoCapability)
			{
				H245_VideoCapability& cap = (H245_VideoCapability&)body.m_forwardLogicalChannelParameters.m_dataType;
				cap.SetTag(codec);
				H245_GenericCapability& gen = (H245_GenericCapability&)cap;
				gen.m_capabilityIdentifier.SetTag(H245_CapabilityIdentifier::e_standard);
				((PASN_ObjectId&)gen.m_capabilityIdentifier) = OpalPluginCodec_Identifer_H264_Generic;
				gen.IncludeOptionalField(H245_GenericCapability::e_maxBitRate);
				gen.m_maxBitRate = max_bitrate;
				gen.IncludeOptionalField(H245_GenericCapability::e_collapsing);
				for (PINDEX i = 0; i < collapsing.GetSize(); i++) {
					gen.m_collapsing.Append(collapsing[i].CloneAs<H245_GenericParameter>());
				}

			}
			else if (codec == H245_VideoCapability::e_extendedVideoCapability)
			{
				H245_VideoCapability& cap = (H245_VideoCapability&)body.m_forwardLogicalChannelParameters.m_dataType;
				cap.SetTag(codec);

				H245_ExtendedVideoCapability& exvcap = (H245_ExtendedVideoCapability&)cap;
				exvcap.IncludeOptionalField(H245_ExtendedVideoCapability::e_videoCapabilityExtension);
				exvcap.m_videoCapability.RemoveAll();
				exvcap.m_videoCapabilityExtension.RemoveAll();

				H245_VideoCapability* vcap = new H245_VideoCapability();
				vcap->SetTag(H245_VideoCapability::e_genericVideoCapability);
				H245_GenericCapability& genvcap = (H245_GenericCapability&)(*vcap);
				H323SetCapabilityIdentifier(OpalPluginCodec_Identifer_H264_Generic, genvcap.m_capabilityIdentifier);
				genvcap.IncludeOptionalField(H245_GenericCapability::e_maxBitRate);
				genvcap.m_maxBitRate = max_bitrate;
				genvcap.IncludeOptionalField(H245_GenericCapability::e_collapsing);

				genvcap.IncludeOptionalField(H245_GenericCapability::e_collapsing);
				for (PINDEX i = 0; i < collapsing.GetSize(); i++) {
					genvcap.m_collapsing.Append(collapsing[i].CloneAs<H245_GenericParameter>());
				}
				exvcap.m_videoCapability.Append(vcap);

				H245_GenericCapability* excap = new H245_GenericCapability();
				H323SetCapabilityIdentifier(OpalPluginCodec_Identifer_H264_Extend, excap->m_capabilityIdentifier);

				excap->IncludeOptionalField(H245_GenericCapability::e_collapsing);
				excap->m_collapsing.RemoveAll();
				H323AddGenericParameterAs<PASN_Integer>(excap->m_collapsing, 1, H245_ParameterValue::Choices::e_booleanArray) = 1;

				exvcap.m_videoCapabilityExtension.Append(excap);

			}
			else
			{
				return false;
			}


			body.m_forwardLogicalChannelParameters.m_multiplexParameters.SetTag(H245_OpenLogicalChannel_forwardLogicalChannelParameters_multiplexParameters::e_h2250LogicalChannelParameters);
			H245_H2250LogicalChannelParameters& pms = (H245_H2250LogicalChannelParameters&)body.m_forwardLogicalChannelParameters.m_multiplexParameters;
			pms.m_sessionID = session_id;
			pms.IncludeOptionalField(H245_H2250LogicalChannelParameters::e_mediaGuaranteedDelivery);
			pms.m_mediaGuaranteedDelivery = false;
			pms.IncludeOptionalField(H245_H2250LogicalChannelParameters::e_mediaControlChannel);
			h323_util::h245_transport_unicast_address_set(pms.m_mediaControlChannel, local_media_ctrl_address, local_media_ctrl_port);


			pms.IncludeOptionalField(H245_H2250LogicalChannelParameters::e_dynamicRTPPayloadType);
			pms.m_dynamicRTPPayloadType = payload_type;

			pms.IncludeOptionalField(H245_H2250LogicalChannelParameters::e_mediaPacketization);
			pms.m_mediaPacketization.SetTag(H245_H2250LogicalChannelParameters_mediaPacketization::e_rtpPayloadType);
			H245_RTPPayloadType& pt = (H245_RTPPayloadType&)pms.m_mediaPacketization;
			pt.IncludeOptionalField(H245_RTPPayloadType::e_payloadType);
			pt.m_payloadType = payload_type;
			pt.m_payloadDescriptor.SetTag(H245_RTPPayloadType_payloadDescriptor::e_oid);
			((PASN_ObjectId&)pt.m_payloadDescriptor) = OpalPluginCodec_Identifer_H264_NonInterleaved;


			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			PBYTEArray buf = tpkt::serialize(stream);

			if (log_)
			{
				std::string url = url_.to_string();
				std::string addr = (const std::string&)local_media_ctrl_address.AsString();
				log_->debug("H.323 send_open_video_logical_channel: url={}, chann={},code={},pt={},rtcp={}:{}",
					url, logical_channel, codec,payload_type, addr, local_media_ctrl_port)->flush();
			}

			return h245_sending_queue_->send(h245_skt_, std::string((const char*)buf.GetPointer(), buf.GetSize()));
		}


		bool h323_call::send_open_logical_channel_ack(int logical_channel, int session_id,
			const PString& media_addr, WORD media_port,
			const PString& media_ctrl_addr, WORD media_ctrl_port)
		{
			if (!h245_skt_)
				return false;
			H245_MultimediaSystemControlMessage msg;
			msg.SetTag(H245_MultimediaSystemControlMessage::e_response);
			H245_ResponseMessage& rsp = (H245_ResponseMessage&)msg;
			rsp.SetTag(H245_ResponseMessage::e_openLogicalChannelAck);
			H245_OpenLogicalChannelAck& body = (H245_OpenLogicalChannelAck&)rsp;

			body.m_forwardLogicalChannelNumber = logical_channel;
			
			body.IncludeOptionalField(H245_OpenLogicalChannelAck::e_forwardMultiplexAckParameters);
			body.m_forwardMultiplexAckParameters.SetTag(H245_OpenLogicalChannelAck_forwardMultiplexAckParameters::e_h2250LogicalChannelAckParameters);
			H245_H2250LogicalChannelAckParameters& pms = (H245_H2250LogicalChannelAckParameters&)body.m_forwardMultiplexAckParameters;
			pms.IncludeOptionalField(H245_H2250LogicalChannelAckParameters::e_sessionID);
			pms.m_sessionID = session_id;
			pms.IncludeOptionalField(H245_H2250LogicalChannelAckParameters::e_mediaChannel);
			pms.m_mediaChannel.SetTag(H245_TransportAddress::e_unicastAddress);
			h323_util::h245_transport_unicast_address_set(pms.m_mediaChannel, PIPSocket::Address(media_addr), media_port);

			pms.IncludeOptionalField(H245_H2250LogicalChannelAckParameters::e_mediaControlChannel);
			h323_util::h245_transport_unicast_address_set(pms.m_mediaControlChannel, PIPSocket::Address(media_ctrl_addr), media_ctrl_port);

			pms.IncludeOptionalField(H245_H2250LogicalChannelAckParameters::e_flowControlToZero);
			pms.m_flowControlToZero = false;

			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			PBYTEArray buf = tpkt::serialize(stream);

			if (log_)
			{
				std::string url = url_.to_string();
				std::string addr1 = (const std::string&)media_addr;
				std::string addr2 = (const std::string&)media_ctrl_addr;
				log_->debug("H.323 send_open_logical_channel_ack: url={}, chann={},rtp={}:{},rtcp={}:{}", 
					url, logical_channel, addr1,media_port, addr2,media_ctrl_port)->flush();
			}

			return h245_sending_queue_->send(h245_skt_, std::string((const char*)buf.GetPointer(), buf.GetSize()));
		}

		bool h323_call::send_open_logical_channel_reject(int logical_channel, H245_OpenLogicalChannelReject_cause::Choices cause)
		{
			if (!h245_skt_)
				return false;
			H245_MultimediaSystemControlMessage msg;
			msg.SetTag(H245_MultimediaSystemControlMessage::e_response);
			H245_ResponseMessage& rsp = (H245_ResponseMessage&)msg;
			rsp.SetTag(H245_ResponseMessage::e_openLogicalChannelReject);
			H245_OpenLogicalChannelReject& body = (H245_OpenLogicalChannelReject&)rsp;

			
			body.m_forwardLogicalChannelNumber = logical_channel;
			body.m_cause = cause;

			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			PBYTEArray buf = tpkt::serialize(stream);

			if (log_)
			{
				std::string url = url_.to_string();
				log_->debug("H.323 send_open_logical_channel_reject: url={}, chann={}",url, logical_channel)->flush();
			}

			return h245_sending_queue_->send(h245_skt_, std::string((const char*)buf.GetPointer(), buf.GetSize()));
		}

		bool h323_call::send_close_logical_channel(int logical_channel, H245_CloseLogicalChannel_reason::Choices reason)
		{
			if (!h245_skt_)
				return false;
			
			H245_MultimediaSystemControlMessage msg;
			msg.SetTag(H245_MultimediaSystemControlMessage::e_request);
			H245_RequestMessage& req = (H245_RequestMessage&)msg;
			req.SetTag(H245_RequestMessage::e_closeLogicalChannel);
			H245_CloseLogicalChannel& body = (H245_CloseLogicalChannel&)req;
			
			body.m_forwardLogicalChannelNumber = logical_channel;
			body.IncludeOptionalField(H245_CloseLogicalChannel::e_reason);
			body.m_reason = reason;
			body.m_source.SetTag(H245_CloseLogicalChannel_source::e_lcse);

			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			PBYTEArray buf = tpkt::serialize(stream);

			if (log_)
			{
				std::string url = url_.to_string();
				log_->debug("H.323 send_close_logical_channel: url={}, chann={}, reason={}", url, logical_channel, (int)reason)->flush();
			}

			return h245_sending_queue_->send(h245_skt_, std::string((const char*)buf.GetPointer(), buf.GetSize()));
		}

		bool h323_call::send_close_logical_channel_ack(int logical_channel)
		{
			if (!h245_skt_)
				return false;
			H245_MultimediaSystemControlMessage msg;
			msg.SetTag(H245_MultimediaSystemControlMessage::e_response);
			H245_ResponseMessage& rsp = (H245_ResponseMessage&)msg;
			rsp.SetTag(H245_ResponseMessage::e_closeLogicalChannelAck);
			H245_CloseLogicalChannelAck& body = (H245_CloseLogicalChannelAck&)rsp;
			
			body.m_forwardLogicalChannelNumber = logical_channel;
			
			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			PBYTEArray buf = tpkt::serialize(stream);

			if (log_)
			{
				std::string url = url_.to_string();
				log_->debug("H.323 send_close_logical_channel_ack: url={}, chann={}", url, logical_channel)->flush();
			}

			return h245_sending_queue_->send(h245_skt_, std::string((const char*)buf.GetPointer(), buf.GetSize()));
		}


		bool h323_call::send_end_session_command()
		{
			if (!h245_skt_)
				return false;
			H245_MultimediaSystemControlMessage msg;
			msg.SetTag(H245_MultimediaSystemControlMessage::e_command);
			H245_CommandMessage& cmd = (H245_CommandMessage&)msg;
			cmd.SetTag(H245_CommandMessage::e_endSessionCommand);
			H245_EndSessionCommand& body = (H245_EndSessionCommand&)cmd;
			
			body.SetTag(H245_EndSessionCommand::e_disconnect);
			
			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			PBYTEArray buf = tpkt::serialize(stream);

			if (log_)
			{
				std::string url = url_.to_string();
				log_->debug("H.323 send_end_session_command: url={}", url)->flush();
			}
			return h245_sending_queue_->send(h245_skt_, std::string((const char*)buf.GetPointer(), buf.GetSize()));
		}

		bool h323_call::send_round_trip_delay_request()
		{
			if (!h245_skt_)
				return false;
			H245_MultimediaSystemControlMessage msg;
			msg.SetTag(H245_MultimediaSystemControlMessage::e_request);
			H245_RequestMessage& req = (H245_RequestMessage&)msg;
			req.SetTag(H245_RequestMessage::e_roundTripDelayRequest);
			H245_RoundTripDelayRequest& body = (H245_RoundTripDelayRequest&)req;
			body.m_sequenceNumber = request_seq_.fetch_add(1);

			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			PBYTEArray buf = tpkt::serialize(stream);
			return h245_sending_queue_->send(h245_skt_, std::string((const char*)buf.GetPointer(), buf.GetSize()));
		}

		bool h323_call::send_round_trip_delay_response(uint32_t seq)
		{
			if (!h245_skt_)
				return false;
			H245_MultimediaSystemControlMessage msg;
			msg.SetTag(H245_MultimediaSystemControlMessage::e_response);
			H245_ResponseMessage& rsp = (H245_ResponseMessage&)msg;
			rsp.SetTag(H245_ResponseMessage::e_roundTripDelayResponse);
			H245_RoundTripDelayResponse& body = (H245_RoundTripDelayResponse&)rsp;

			body.m_sequenceNumber = seq;

			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			PBYTEArray buf = tpkt::serialize(stream);
			return h245_sending_queue_->send(h245_skt_, std::string((const char*)buf.GetPointer(), buf.GetSize()));
		}

		bool h323_call::send_h239_flow_control_release_request(unsigned logical_channel, unsigned bit_rate)
		{
			if (!h245_skt_)
				return false;
			H245_MultimediaSystemControlMessage msg;
			msg.SetTag(H245_MultimediaSystemControlMessage::e_request);
			H245_RequestMessage& req = (H245_RequestMessage&)msg;
			req.SetTag(H245_RequestMessage::e_genericRequest);
			H245_GenericMessage& body = (H245_GenericMessage&)req;
			H323SetCapabilityIdentifier(H239MessageOID, body.m_messageIdentifier);
			body.IncludeOptionalField(H245_GenericMessage::e_subMessageIdentifier);
			body.m_subMessageIdentifier = 1;
			body.IncludeOptionalField(H245_GenericMessage::e_messageContent);

			H245_ArrayOf_GenericParameter& params = (H245_ArrayOf_GenericParameter&)body.m_messageContent;
			// Note order is important (Table 12/H.239)
			H323AddGenericParameterInteger(params, 42, logical_channel, H245_ParameterValue::e_unsignedMin);
			H323AddGenericParameterInteger(params, 41, bit_rate, H245_ParameterValue::e_unsignedMin);


			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			PBYTEArray buf = tpkt::serialize(stream);
			return h245_sending_queue_->send(h245_skt_, std::string((const char*)buf.GetPointer(), buf.GetSize()));
		}

		bool h323_call::send_h239_flow_control_release_response(unsigned logical_channel, bool acknowledge)
		{
			if (!h245_skt_)
				return false;
			H245_MultimediaSystemControlMessage msg;
			msg.SetTag(H245_MultimediaSystemControlMessage::e_response);
			H245_ResponseMessage& rsp = (H245_ResponseMessage&)msg;
			rsp.SetTag(H245_ResponseMessage::e_genericResponse);
			H245_GenericMessage& body = (H245_GenericMessage&)rsp;



			H323SetCapabilityIdentifier(H239MessageOID, body.m_messageIdentifier);
			body.IncludeOptionalField(H245_GenericMessage::e_subMessageIdentifier);
			body.m_subMessageIdentifier = 2;
			body.IncludeOptionalField(H245_GenericMessage::e_messageContent);

			H245_ArrayOf_GenericParameter& params = (H245_ArrayOf_GenericParameter&)body.m_messageContent;
			// Note order is important (Table 12/H.239)
			H323AddGenericParameterBoolean(params, acknowledge ? 126 : 127, true); // Acknowledge
			H323AddGenericParameterInteger(params, 42, logical_channel, H245_ParameterValue::e_unsignedMin);

			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			PBYTEArray buf = tpkt::serialize(stream);
			return h245_sending_queue_->send(h245_skt_, std::string((const char*)buf.GetPointer(), buf.GetSize()));
		}
		bool h323_call::send_h239_presentation_request(unsigned logical_channel, unsigned symmetry_breaking, unsigned terminal_label)
		{
			if (!h245_skt_)
				return false;
			H245_MultimediaSystemControlMessage msg;
			msg.SetTag(H245_MultimediaSystemControlMessage::e_request);
			H245_RequestMessage& req = (H245_RequestMessage&)msg;
			req.SetTag(H245_RequestMessage::e_genericRequest);
			H245_GenericMessage& body = (H245_GenericMessage&)req;
			H323SetCapabilityIdentifier(H239MessageOID, body.m_messageIdentifier);
			body.IncludeOptionalField(H245_GenericMessage::e_subMessageIdentifier);
			body.m_subMessageIdentifier = 3;
			body.IncludeOptionalField(H245_GenericMessage::e_messageContent);
			
			H245_ArrayOf_GenericParameter& params = (H245_ArrayOf_GenericParameter&)body.m_messageContent;
			// Note order is important (Table 12/H.239)
			H323AddGenericParameterInteger(params, 44, terminal_label, H245_ParameterValue::e_unsignedMin);
			H323AddGenericParameterInteger(params, 42, logical_channel, H245_ParameterValue::e_unsignedMin);
			H323AddGenericParameterInteger(params, 43, symmetry_breaking, H245_ParameterValue::e_unsignedMin);


			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			PBYTEArray buf = tpkt::serialize(stream);
			return h245_sending_queue_->send(h245_skt_, std::string((const char*)buf.GetPointer(), buf.GetSize()));
		}

		bool h323_call::send_h239_presentation_response(unsigned logical_channel,bool acknowledge, unsigned terminal_label)
		{
			if (!h245_skt_)
				return false;
			H245_MultimediaSystemControlMessage msg;
			msg.SetTag(H245_MultimediaSystemControlMessage::e_response);
			H245_ResponseMessage& rsp = (H245_ResponseMessage&)msg;
			rsp.SetTag(H245_ResponseMessage::e_genericResponse);
			H245_GenericMessage& body = (H245_GenericMessage&)rsp;



			H323SetCapabilityIdentifier(H239MessageOID, body.m_messageIdentifier);
			body.IncludeOptionalField(H245_GenericMessage::e_subMessageIdentifier);
			body.m_subMessageIdentifier = 4;
			body.IncludeOptionalField(H245_GenericMessage::e_messageContent);

			H245_ArrayOf_GenericParameter& params = (H245_ArrayOf_GenericParameter&)body.m_messageContent;
			// Note order is important (Table 12/H.239)
			H323AddGenericParameterBoolean(params, acknowledge ? 126 : 127, true); // Acknowledge/Reject
			H323AddGenericParameterInteger(params, 42, logical_channel, H245_ParameterValue::e_unsignedMin);

			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			PBYTEArray buf = tpkt::serialize(stream);
			return h245_sending_queue_->send(h245_skt_, std::string((const char*)buf.GetPointer(), buf.GetSize()));
		}

		bool h323_call::send_h239_presentation_release(unsigned logical_channel, unsigned terminal_label)
		{
			if (!h245_skt_)
				return false;
			H245_MultimediaSystemControlMessage msg;
			msg.SetTag(H245_MultimediaSystemControlMessage::e_command);
			H245_CommandMessage& cmd = (H245_CommandMessage&)msg;
			cmd.SetTag(H245_CommandMessage::e_genericCommand);
			H245_GenericMessage& body = (H245_GenericMessage&)cmd;
			H323SetCapabilityIdentifier(H239MessageOID, body.m_messageIdentifier);
			body.IncludeOptionalField(H245_GenericMessage::e_subMessageIdentifier);
			body.m_subMessageIdentifier = 5;
			body.IncludeOptionalField(H245_GenericMessage::e_messageContent);

			H245_ArrayOf_GenericParameter& params = (H245_ArrayOf_GenericParameter&)body.m_messageContent;
			// Note order is important (Table 12/H.239)
			H323AddGenericParameterInteger(params, 44, terminal_label, H245_ParameterValue::e_unsignedMin);
			H323AddGenericParameterInteger(params, 42, logical_channel, H245_ParameterValue::e_unsignedMin);


			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			PBYTEArray buf = tpkt::serialize(stream);
			return h245_sending_queue_->send(h245_skt_, std::string((const char*)buf.GetPointer(), buf.GetSize()));
		}

		bool h323_call::send_h239_presentation_owner(unsigned logical_channel, unsigned terminal_label)
		{
			if (!h245_skt_)
				return false;
			H245_MultimediaSystemControlMessage msg;
			msg.SetTag(H245_MultimediaSystemControlMessage::e_indication);
			H245_IndicationMessage& ind = (H245_IndicationMessage&)msg;
			ind.SetTag(H245_IndicationMessage::e_genericIndication);
			H245_GenericMessage& body = (H245_GenericMessage&)ind;

			H323SetCapabilityIdentifier(H239MessageOID, body.m_messageIdentifier);
			body.IncludeOptionalField(H245_GenericMessage::e_subMessageIdentifier);
			body.m_subMessageIdentifier = 6;
			body.IncludeOptionalField(H245_GenericMessage::e_messageContent);

			H245_ArrayOf_GenericParameter& params = (H245_ArrayOf_GenericParameter&)body.m_messageContent;
			// Note order is important (Table 12/H.239)
			H323AddGenericParameterInteger(params, 44, terminal_label, H245_ParameterValue::e_unsignedMin);
			H323AddGenericParameterInteger(params, 42, logical_channel, H245_ParameterValue::e_unsignedMin);

			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			PBYTEArray buf = tpkt::serialize(stream);
			return h245_sending_queue_->send(h245_skt_, std::string((const char*)buf.GetPointer(), buf.GetSize()));
		}

		void h323_call::fill_capability(H245_ArrayOf_CapabilityTableEntry& entries, H245_ArrayOf_CapabilityDescriptor& descs)
		{
			entries.RemoveAll();
			descs.RemoveAll();

			H245_CapabilityDescriptor* desc = new H245_CapabilityDescriptor();
			desc->m_capabilityDescriptorNumber = 1;
			desc->IncludeOptionalField(H245_CapabilityDescriptor::e_simultaneousCapabilities);
			desc->m_simultaneousCapabilities.RemoveAll();
			int chann_num = 1;
			for (auto itr = local_channels_.begin(); itr != local_channels_.end(); itr++)
			{
				itr->channel_number=chann_num++;
				if (itr->media_type == media_type_audio)
				{
					H245_CapabilityTableEntry* entry = new H245_CapabilityTableEntry();
					entry->IncludeOptionalField(H245_CapabilityTableEntry::e_capability);
					entry->m_capabilityTableEntryNumber = itr->channel_number;
					entry->m_capability.SetTag(H245_Capability::e_receiveAndTransmitAudioCapability);
					auto codec = h323_util::convert_audio_codec_type_back(itr->codec_type);
					H245_AudioCapability& acap = (H245_AudioCapability&)entry->m_capability;
					acap.SetTag(codec);
					(PASN_Integer&)acap = 240;
					entries.Append(entry);


				}
				else if (itr->media_type == media_type_video)
				{
					H245_CapabilityTableEntry* entry = new H245_CapabilityTableEntry();
					entry->IncludeOptionalField(H245_CapabilityTableEntry::e_capability);
					entry->m_capabilityTableEntryNumber = itr->channel_number;
					entry->m_capability.SetTag(H245_Capability::e_receiveAndTransmitVideoCapability);
					if (itr->type==media_channel_type_t::extend)
					{
						((H245_VideoCapability&)entry->m_capability).SetTag(H245_VideoCapability::e_extendedVideoCapability);
						H245_ExtendedVideoCapability& exvcap = (H245_ExtendedVideoCapability&)((H245_VideoCapability&)entry->m_capability);
						exvcap.m_videoCapability.RemoveAll();
						exvcap.m_videoCapabilityExtension.RemoveAll();

						H245_VideoCapability* vcap = new H245_VideoCapability();
						vcap->SetTag(H245_VideoCapability::e_genericVideoCapability);
						H245_GenericCapability& genvcap = (H245_GenericCapability&)(*vcap);
						H323SetCapabilityIdentifier(OpalPluginCodec_Identifer_H264_Generic, genvcap.m_capabilityIdentifier);
						genvcap.IncludeOptionalField(H245_GenericCapability::e_maxBitRate);
						genvcap.m_maxBitRate = itr->max_bitrate;
						genvcap.IncludeOptionalField(H245_GenericCapability::e_collapsing);
						h323_util::fill_video_collapsing(genvcap.m_collapsing, itr->profile, itr->level, itr->mbps, itr->mfs, itr->max_nal_size);

						exvcap.m_videoCapability.Append(vcap);

						H245_GenericCapability* excap = new H245_GenericCapability();
						H323SetCapabilityIdentifier(OpalPluginCodec_Identifer_H264_Extend, excap->m_capabilityIdentifier);
						
						excap->IncludeOptionalField(H245_GenericCapability::e_collapsing);
						excap->m_collapsing.RemoveAll();
						H323AddGenericParameterAs<PASN_Integer>(excap->m_collapsing, 1, H245_ParameterValue::Choices::e_booleanArray) = 1;
						
						exvcap.IncludeOptionalField(H245_ExtendedVideoCapability::e_videoCapabilityExtension);
						exvcap.m_videoCapabilityExtension.Append(excap);



					}
					else if(itr->type==media_channel_type_t::notmal)
					{
						((H245_VideoCapability&)entry->m_capability).SetTag(H245_VideoCapability::e_genericVideoCapability);
						H245_GenericCapability& vcap = (H245_GenericCapability&)((H245_VideoCapability&)entry->m_capability);
						H323SetCapabilityIdentifier(OpalPluginCodec_Identifer_H264_Generic, vcap.m_capabilityIdentifier);
						vcap.IncludeOptionalField(H245_GenericCapability::e_maxBitRate);
						vcap.m_maxBitRate = itr->max_bitrate;
						vcap.IncludeOptionalField(H245_GenericCapability::e_collapsing);
						vcap.m_collapsing.RemoveAll();
						h323_util::fill_video_collapsing(vcap.m_collapsing, itr->profile, itr->level, itr->mbps, itr->mfs, itr->max_nal_size);
					}

					entries.Append(entry);
				}
				else if(itr->type == media_channel_type_t::h239_control)
				{
					H245_CapabilityTableEntry* entry = new H245_CapabilityTableEntry();
					entry->IncludeOptionalField(H245_CapabilityTableEntry::e_capability);
					entry->m_capabilityTableEntryNumber = itr->channel_number;
					entry->m_capability.SetTag(H245_Capability::e_genericControlCapability);

					H245_GenericCapability& gen = (H245_GenericCapability&)entry->m_capability;
					H323SetCapabilityIdentifier("0.0.8.239.1.1", gen.m_capabilityIdentifier);
					entries.Append(entry);
				}
			}

			
			int security_media_capability = 1;
			for (auto itr = local_channels_.begin(); itr != local_channels_.end(); itr++)
			{
				H245_CapabilityTableEntry* entry_sec = new H245_CapabilityTableEntry();
				entry_sec->m_capabilityTableEntryNumber = chann_num++;
				entry_sec->IncludeOptionalField(H245_CapabilityTableEntry::e_capability);
				entry_sec->m_capability.SetTag(H245_Capability::e_h235SecurityCapability);
				H245_H235SecurityCapability& sec_cap = (H245_H235SecurityCapability&)entry_sec->m_capability;
				sec_cap.m_encryptionAuthenticationAndIntegrity.IncludeOptionalField(H245_EncryptionAuthenticationAndIntegrity::e_encryptionCapability);
				H245_EncryptionCapability& auth = (H245_EncryptionCapability&)sec_cap.m_encryptionAuthenticationAndIntegrity.m_encryptionCapability;
				auth.RemoveAll();
				H245_MediaEncryptionAlgorithm* algorithm = new H245_MediaEncryptionAlgorithm();
				algorithm->SetTag(H245_MediaEncryptionAlgorithm::e_algorithm);
				((PASN_ObjectId&)(*algorithm)).SetValue("2.16.840.1.101.3.4.1.2");
				auth.Append(algorithm);
				algorithm = new H245_MediaEncryptionAlgorithm();
				algorithm->SetTag(H245_MediaEncryptionAlgorithm::e_algorithm);
				((PASN_ObjectId&)(*algorithm)).SetValue("2.16.840.1.101.3.4.1.22");
				auth.Append(algorithm);
				algorithm = new H245_MediaEncryptionAlgorithm();
				algorithm->SetTag(H245_MediaEncryptionAlgorithm::e_algorithm);
				((PASN_ObjectId&)(*algorithm)).SetValue("2.16.840.1.101.3.4.1.42");
				auth.Append(algorithm);
				sec_cap.m_mediaCapability = security_media_capability++;

				entries.Append(entry_sec);

				H245_AlternativeCapabilitySet* acs = new H245_AlternativeCapabilitySet();
				acs->SetSize(2);
				(*acs)[0] = itr->channel_number;
				(*acs)[1] = entry_sec->m_capabilityTableEntryNumber;
				desc->m_simultaneousCapabilities.Append(acs);

			}
			
			H245_CapabilityTableEntry* hookflash = new H245_CapabilityTableEntry();
			hookflash->m_capabilityTableEntryNumber = chann_num++;
			hookflash->IncludeOptionalField(H245_CapabilityTableEntry::e_capability);
			hookflash->m_capability.SetTag(H245_Capability::e_receiveAndTransmitUserInputCapability);
			H245_UserInputCapability& uicap_hookflash = (H245_UserInputCapability&)hookflash->m_capability;
			uicap_hookflash.SetTag(H245_UserInputCapability::e_hookflash);
			entries.Append(hookflash);
			H245_AlternativeCapabilitySet* acs = new H245_AlternativeCapabilitySet();
			(*acs)[0] = hookflash->m_capabilityTableEntryNumber;
			desc->m_simultaneousCapabilities.Append(acs);

			H245_CapabilityTableEntry* basic_string = new H245_CapabilityTableEntry();
			basic_string->m_capabilityTableEntryNumber = chann_num++;
			basic_string->IncludeOptionalField(H245_CapabilityTableEntry::e_capability);
			basic_string->m_capability.SetTag(H245_Capability::e_receiveAndTransmitUserInputCapability);
			H245_UserInputCapability& uicap_basic_string = (H245_UserInputCapability&)basic_string->m_capability;
			uicap_basic_string.SetTag(H245_UserInputCapability::e_basicString);
			entries.Append(basic_string);

			H245_CapabilityTableEntry* dtmf = new H245_CapabilityTableEntry();
			dtmf->m_capabilityTableEntryNumber = chann_num++;
			dtmf->IncludeOptionalField(H245_CapabilityTableEntry::e_capability);
			dtmf->m_capability.SetTag(H245_Capability::e_receiveAndTransmitUserInputCapability);
			H245_UserInputCapability& uicap_dtmf = (H245_UserInputCapability&)dtmf->m_capability;
			uicap_dtmf.SetTag(H245_UserInputCapability::e_dtmf);
			entries.Append(dtmf);

			acs = new H245_AlternativeCapabilitySet();
			acs->SetSize(2);
			(*acs)[0] = basic_string->m_capabilityTableEntryNumber;
			(*acs)[1] = dtmf->m_capabilityTableEntryNumber;
			desc->m_simultaneousCapabilities.Append(acs);

			descs.Append(desc);
			
		}

		void h323_call::s_litertp_on_require_keyframe(void* ctx, uint32_t ssrc, uint32_t ssrc_media)
		{
			h323_call* p = (h323_call*)ctx;
			p->require_keyframe();
			if (p->log_)
			{
				std::string url = p->url_.to_string();
				p->log_->trace("H.323:litertp require keyframe, url={}", url)->flush();
			}
		}

		void h323_call::on_h225_tpkt(const PBYTEArray& tpkt)
		{
			Q931 q931;
			if (!q931.Decode(tpkt))
			{
				if (log_)
				{
					std::string url = url_.to_string();
					log_->error("H.323: h225 decode q931 failed, url={}", url)->flush();
				}
				return;
			}
			update();
			
			Q931::MsgTypes msg_type=q931.GetMessageType();
			if (msg_type == Q931::MsgTypes::SetupMsg)
			{
				PBYTEArray uuie_data = q931.GetIE(Q931::InformationElementCodes::UserUserIE);
				H225_H323_UserInformation ui;
				PPER_Stream pper_stream(uuie_data);
				if (ui.Decode(pper_stream))
				{
					if (ui.m_h323_uu_pdu.m_h323_message_body.GetTag() == H225_H323_UU_PDU_h323_message_body::e_setup)
					{
						H225_Setup_UUIE& uuie = (H225_Setup_UUIE&)ui.m_h323_uu_pdu.m_h323_message_body;
						on_setup(uuie, q931);
					}
				}
			}
			else if (msg_type == Q931::MsgTypes::CallProceedingMsg)
			{
				PBYTEArray uuie_data = q931.GetIE(Q931::InformationElementCodes::UserUserIE);
				H225_H323_UserInformation ui;
				PPER_Stream pper_stream(uuie_data);
				if (ui.Decode(pper_stream))
				{
					if (ui.m_h323_uu_pdu.m_h323_message_body.GetTag() == H225_H323_UU_PDU_h323_message_body::e_callProceeding)
					{
						H225_CallProceeding_UUIE& uuie = (H225_CallProceeding_UUIE&)ui.m_h323_uu_pdu.m_h323_message_body;
						on_call_proceeding(uuie);
					}
				}
			}
			else if (msg_type == Q931::MsgTypes::AlertingMsg)
			{
				PBYTEArray uuie_data = q931.GetIE(Q931::InformationElementCodes::UserUserIE);
				H225_H323_UserInformation ui;
				PPER_Stream pper_stream(uuie_data);
				if (ui.Decode(pper_stream))
				{
					if (ui.m_h323_uu_pdu.m_h323_message_body.GetTag() == H225_H323_UU_PDU_h323_message_body::e_alerting)
					{
						H225_Alerting_UUIE& uuie = (H225_Alerting_UUIE&)ui.m_h323_uu_pdu.m_h323_message_body;
						on_alerting(uuie);
					}
				}
			}
			else if (msg_type == Q931::MsgTypes::FacilityMsg)
			{
				PBYTEArray uuie_data = q931.GetIE(Q931::InformationElementCodes::UserUserIE);
				H225_H323_UserInformation ui;
				PPER_Stream pper_stream(uuie_data);
				if (ui.Decode(pper_stream))
				{
					if (ui.m_h323_uu_pdu.m_h323_message_body.GetTag() == H225_H323_UU_PDU_h323_message_body::e_facility)
					{
						on_facility(ui.m_h323_uu_pdu,q931);
					}
				}
			}
			else if (msg_type == Q931::MsgTypes::ConnectMsg)
			{
				
				PBYTEArray uuie_data = q931.GetIE(Q931::InformationElementCodes::UserUserIE);
				H225_H323_UserInformation ui;
				PPER_Stream pper_stream(uuie_data);
				if (ui.Decode(pper_stream))
				{
					if (ui.m_h323_uu_pdu.m_h323_message_body.GetTag() == H225_H323_UU_PDU_h323_message_body::e_connect)
					{
						H225_Connect_UUIE& uuie = (H225_Connect_UUIE&)ui.m_h323_uu_pdu.m_h323_message_body;
						on_connect(uuie,q931);
					}
				}
				
			}
			else if (msg_type == Q931::MsgTypes::ReleaseCompleteMsg)
			{
				PBYTEArray uuie_data = q931.GetIE(Q931::InformationElementCodes::UserUserIE);
				H225_H323_UserInformation ui;
				PPER_Stream pper_stream(uuie_data);
				if (ui.Decode(pper_stream))
				{
					if (ui.m_h323_uu_pdu.m_h323_message_body.GetTag() == H225_H323_UU_PDU_h323_message_body::e_releaseComplete)
					{
						H225_ReleaseComplete_UUIE& uuie = (H225_ReleaseComplete_UUIE&)ui.m_h323_uu_pdu.m_h323_message_body;
						on_release_complete(uuie);
					}
				}
			}
			
		}

		void h323_call::on_h245_tpkt(const PBYTEArray& tpkt)
		{
			H245_MultimediaSystemControlMessage msg;
			PPER_Stream pper(tpkt);
			if (!msg.Decode(pper))
			{
				if (log_)
				{
					std::string url = url_.to_string();
					log_->error("H.323: h225 decode h245 tpkt failed, url={}", url)->flush();
				}
				return;
			}
			update();
			if (msg.GetTag() == H245_MultimediaSystemControlMessage::e_request)
			{
				const H245_RequestMessage& req = (const H245_RequestMessage&)msg;
				if (req.GetTag() == H245_RequestMessage::e_terminalCapabilitySet)
				{
					const H245_TerminalCapabilitySet& body = (const H245_TerminalCapabilitySet&)req;
					on_terminal_capability_set(body);
				}
				else if (req.GetTag() == H245_RequestMessage::e_masterSlaveDetermination)
				{
					const H245_MasterSlaveDetermination& body = (const H245_MasterSlaveDetermination&)req;
					on_master_slave_determination(body);
				}
				else if (req.GetTag() == H245_RequestMessage::e_openLogicalChannel)
				{
					const H245_OpenLogicalChannel& body = (const H245_OpenLogicalChannel&)req;
					on_open_logical_channel(body);
				}
				else if (req.GetTag() == H245_RequestMessage::e_closeLogicalChannel)
				{
					const H245_CloseLogicalChannel& body = (const H245_CloseLogicalChannel&)req;
					on_close_logical_channel(body);
				}
				else if (req.GetTag() == H245_RequestMessage::e_roundTripDelayRequest)
				{
					const H245_RoundTripDelayRequest& body = (const H245_RoundTripDelayRequest&)req;
					on_round_trip_delay_request(body);
				}
				else if (req.GetTag() == H245_RequestMessage::e_genericRequest)
				{
					const H245_GenericMessage& gen = (const H245_GenericMessage&)req;
					if (H323GetCapabilityIdentifier(gen.m_messageIdentifier) == H239MessageOID)
					{
						on_h239_message(gen.m_subMessageIdentifier, gen.m_messageContent);
					}
				}
			}
			else if (msg.GetTag() == H245_MultimediaSystemControlMessage::e_response)
			{
				const H245_ResponseMessage& rsp = (const H245_ResponseMessage&)msg;
				if (rsp.GetTag() == H245_ResponseMessage::e_terminalCapabilitySetAck)
				{
					const H245_TerminalCapabilitySetAck& body = (const H245_TerminalCapabilitySetAck&)rsp;
					on_terminal_capability_set_ack(body);
				}
				else if (rsp.GetTag() == H245_ResponseMessage::e_terminalCapabilitySetReject)
				{
					const H245_TerminalCapabilitySetReject& body = (const H245_TerminalCapabilitySetReject&)rsp;
					on_terminal_capability_set_reject(body);
				}
				else if (rsp.GetTag() == H245_ResponseMessage::e_masterSlaveDeterminationAck)
				{
					const H245_MasterSlaveDeterminationAck& body = (const H245_MasterSlaveDeterminationAck&)rsp;
					on_master_slave_determination_ack(body);
				}
				else if (rsp.GetTag() == H245_ResponseMessage::e_masterSlaveDeterminationReject)
				{
					const H245_MasterSlaveDeterminationReject& body = (const H245_MasterSlaveDeterminationReject&)rsp;
					on_master_slave_determination_reject(body);
				}
				else if (rsp.GetTag() == H245_ResponseMessage::e_openLogicalChannelAck)
				{
					const H245_OpenLogicalChannelAck& body = (const H245_OpenLogicalChannelAck&)rsp;
					on_open_logical_channel_ack(body);
				}
				else if (rsp.GetTag() == H245_ResponseMessage::e_openLogicalChannelReject)
				{
					const H245_OpenLogicalChannelReject& body = (const H245_OpenLogicalChannelReject&)rsp;
					on_open_logical_channel_reject(body);
				}
				else if (rsp.GetTag() == H245_ResponseMessage::e_genericResponse)
				{
					const H245_GenericMessage& gen = (const H245_GenericMessage&)rsp;
					if (H323GetCapabilityIdentifier(gen.m_messageIdentifier) == H239MessageOID)
					{
						on_h239_message(gen.m_subMessageIdentifier, gen.m_messageContent);
					}
				}
			}
			else if (msg.GetTag() == H245_MultimediaSystemControlMessage::e_command)
			{
				const H245_CommandMessage& cmd = (const H245_CommandMessage&)msg;
				if (cmd.GetTag() == H245_CommandMessage::e_miscellaneousCommand)
				{
					const H245_MiscellaneousCommand& body = (const H245_MiscellaneousCommand&)cmd;
					on_miscellaneous_command(body);
				}
				else if (cmd.GetTag() == H245_CommandMessage::e_endSessionCommand)
				{
					const H245_EndSessionCommand& body = (const H245_EndSessionCommand&)cmd;
					on_end_session_command(body);
				}
				else if (cmd.GetTag() == H245_CommandMessage::e_genericCommand)
				{
					const H245_GenericMessage& gen = (const H245_GenericMessage&)cmd;
					if (H323GetCapabilityIdentifier(gen.m_messageIdentifier) == H239MessageOID)
					{
						on_h239_message(gen.m_subMessageIdentifier, gen.m_messageContent);
					}
				}
			}
			else if (msg.GetTag() == H245_MultimediaSystemControlMessage::e_indication)
			{
				const H245_IndicationMessage& ind = (const H245_IndicationMessage&)msg;
				if (ind.GetTag() == H245_IndicationMessage::e_miscellaneousIndication)
				{
					const H245_MiscellaneousIndication& body = (const H245_MiscellaneousIndication&)ind;
					on_miscellaneous_indication(body);
				}
				else if (ind.GetTag() == H245_IndicationMessage::e_flowControlIndication)
				{
					const H245_FlowControlIndication& body = (const H245_FlowControlIndication&)ind;
					on_flow_control_indication(body);
				}
				else if (ind.GetTag() == H245_IndicationMessage::e_genericIndication)
				{
					const H245_GenericMessage& body = (const H245_GenericMessage&)ind;
					if (H323GetCapabilityIdentifier(body.m_messageIdentifier) == H239MessageOID)
					{
						on_h239_message(body.m_subMessageIdentifier, body.m_messageContent);
					}
				}
				else if (ind.GetTag() == H245_IndicationMessage::e_conferenceIndication)
				{
					const H245_ConferenceIndication& body = (const H245_ConferenceIndication&)ind;
					on_conference_indication(body);
				}
			}
		}


		void h323_call::on_setup(const H225_Setup_UUIE& uuie, const Q931& q931)
		{
			auto self = shared_from_this();
			call_ref_ = (int)q931.GetCallReference();
			std::string called_alias;
			std::string remote_ip;
			WORD remote_port = 0;

			if(uuie.HasOptionalField(H225_Setup_UUIE::e_callIdentifier))
			{
				call_id_=uuie.m_callIdentifier.m_guid;
			}
			conf_id_ = uuie.m_conferenceID;

			if (uuie.HasOptionalField(H225_Setup_UUIE::e_sourceAddress))
			{
				PStringArray arr = H323GetAliasAddressStrings(uuie.m_sourceAddress);
				if (arr.GetSize() > 0)
				{
					remote_alias_ = arr[0].c_str();
				}
			}
			if (uuie.HasOptionalField(H225_Setup_UUIE::e_sourceCallSignalAddress))
			{
				PIPSocket::Address addr;
				if (h323_util::h225_transport_address_get(uuie.m_sourceCallSignalAddress, addr, remote_port))
				{
					remote_ip = addr.AsString().c_str();
				}
			}
			if (uuie.HasOptionalField(H225_Setup_UUIE::e_destinationAddress))
			{
				PStringArray aliases= H323GetAliasAddressStrings(uuie.m_destinationAddress);
				if (aliases.GetSize() > 0) {
					called_alias = aliases[0].c_str();
				}
			}

			voip_uri called_alias_url(called_alias);
			

			url_.scheme = "h323";
			url_.username = called_alias_url.username;
			url_.host = nat_address_;
			url_.port = local_port_;

			this->send_call_proceeding();
			this->send_alerting();

			if (on_incoming_call)
			{
				on_incoming_call(self, local_alias_, remote_alias_, remote_ip, remote_port);
			}

			if (log_)
			{
				log_->debug("H.323: on_setup, called_alias={},remote_alias={},remote_addr={}:{}", called_alias_url.username, remote_alias_,remote_ip,remote_port)->flush();
			}
		}
		void h323_call::on_call_proceeding(const H225_CallProceeding_UUIE& uuie)
		{
			if (log_)
			{
				std::string url = url_.to_string();
				log_->debug("H.323: on_call_proceeding, url={}",url)->flush();
			}
		}

		void h323_call::on_alerting(const H225_Alerting_UUIE& uuie)
		{
			if (log_)
			{
				std::string url = url_.to_string();
				log_->debug("H.323: on_alerting, url={}", url)->flush();
			}

			if (on_remote_ringing)
			{
				auto self = shared_from_this();
				on_remote_ringing(self);
			}
		}

		void h323_call::on_facility(const H225_H323_UU_PDU& pdu, const Q931& q931)
		{
			const H225_Facility_UUIE& uuie = (const H225_Facility_UUIE&)pdu.m_h323_message_body;
			
			H225_GloballyUniqueID call_id,conf_id;

			PIPSocket::Address dst_ip;
			WORD dst_port = 0;

			uint32_t call_ref=q931.GetCallReference();

			
			if (uuie.HasOptionalField(H225_Facility_UUIE::e_conferenceID))
			{
				conf_id = uuie.m_conferenceID;
			}
			if (uuie.HasOptionalField(H225_Facility_UUIE::e_callIdentifier))
			{
				call_id = uuie.m_callIdentifier.m_guid;
			}
			if (uuie.HasOptionalField(H225_Facility_UUIE::e_fastConnectRefused))
			{
				return;  // This seems is heartbeat
			}

			/*
			if (pdu.HasOptionalField(H225_H323_UU_PDU::e_genericData))
			{
				for (PINDEX i = 0; i < pdu.m_genericData.GetSize(); i++)
				{
					H225_GenericData& gen = pdu.m_genericData[i];
					if (gen.m_id.GetTag() == H225_GenericIdentifier::e_standard && ((PASN_Integer&)gen.m_id) == 18)
					{
						if (gen.HasOptionalField(H225_GenericData::e_parameters))
						{
							for (PINDEX i = 0; i < gen.m_parameters.GetSize(); i++)
							{
								H225_EnumeratedParameter& gen_pm=gen.m_parameters[i];
								if (gen_pm.m_id.GetTag() == H225_GenericIdentifier::e_standard && ((PASN_Integer&)gen_pm.m_id) == 1)
								{
									if (gen_pm.HasOptionalField(H225_EnumeratedParameter::e_content))
									{
										if (gen_pm.m_content.GetTag() == H225_Content::e_raw)
										{
											H46018_IncomingCallIndication incall;
											if (((PASN_OctetString&)gen_pm.m_content).DecodeSubType(incall))
											{
												call_id=incall.m_callID.m_guid;
												h323_util::h225_transport_address_get(incall.m_callSignallingAddress, dst_ip, dst_port);
											}
										}
									}
								}
							}
						}
					}
				}
			}
			*/

			if (on_facility_event)
			{
				auto self = shared_from_this();
				on_facility_event(self, call_id, conf_id);
			}
			conf_id_ = conf_id;
			call_id_ = call_id;
			call_ref_ = call_ref;
			voip_uri url;
			std::string dest_addr;
			int dest_port = 0;
			std::error_code ec;
			auto dst_ep=h225_skt_->remote_endpoint(ec);
			dest_addr = dst_ep.address().to_string(ec);
			dest_port = dst_ep.port();

			url.host=dest_addr;
			url.port=dest_port;
			url.scheme="h323";
			url_ = url;

			if (uuie.m_reason.GetTag() == H225_FacilityReason::e_startH245)
			{
				if (uuie.HasOptionalField(H225_Facility_UUIE::e_h245Address))
				{
					PIPSocket::Address ip;
					WORD port = 0;
					h323::h323_util::h225_transport_address_get(uuie.m_h245Address, ip, port);
					this->start_h245_connect(ip.AsString(), port);
				}
			}
			else
			{
				this->send_setup(url);
			}

			if (log_)
			{
				std::string s1 = url_.to_string();
				std::string s2 = (const std::string&)call_id_.AsString();
				log_->debug("H.323: on_facility,url={}, call_id={}, h225addr={}:{}", s1,s2, dest_addr,dest_port)->flush();
			}
		}


		void h323_call::on_connect(const H225_Connect_UUIE& uuie,const Q931& q931)
		{
			if (remote_alias_.empty()) {
				remote_alias_ = (const std::string&)q931.GetDisplayName();
			}
			auto self = shared_from_this();
			if (!uuie.HasOptionalField(H225_Connect_UUIE::e_h245Address)){
				if (on_hangup)
				{
					auto self = shared_from_this();
					on_hangup(self, call::reason_code_t::h225_connect_failed);
				}
				return;
			}

			PIPSocket::Address h245addr;
			if(!h323_util::h225_transport_address_get(uuie.m_h245Address, h245addr, h245_port_)){
				if (on_hangup)
				{
					auto self = shared_from_this();
					on_hangup(self, call::reason_code_t::h225_connect_failed);
				}
				return;
			}
			h245_address_ = h245addr.AsString();
			
			if (!this->start_h245_connect(h245_address_,h245_port_))
			{
				if (on_hangup)
				{
					auto self = shared_from_this();
					on_hangup(self, call::reason_code_t::h225_connect_failed);
				}
				return;
			}
			
			if (log_)
			{
				std::string url = url_.to_string();
				std::string call_id = (const std::string&)call_id_.AsString();
				std::string addr = (const std::string&)h245_address_;
				log_->debug("H.323: on_connect,url={}, call_id={}, h245addr={}:{}", url, call_id, addr, h245_port_)->flush();
			}
			

		}
		void h323_call::on_release_complete(const H225_ReleaseComplete_UUIE& uuie)
		{
			if (log_)
			{
				std::string url = url_.to_string();
				log_->debug("H.323: on_release_complete,url={}", url)->flush();
			}
		}



		void h323_call::on_terminal_capability_set(const H245_TerminalCapabilitySet& body)
		{
			if (!body.HasOptionalField(H245_TerminalCapabilitySet::e_capabilityTable))
			{
				this->send_terminal_capability_set_reject(body.m_sequenceNumber, H245_TerminalCapabilitySetReject_cause::Choices::e_undefinedTableEntryUsed);

				if (log_)
				{
					std::string url = url_.to_string();
					log_->error("H.323: on_terminal_capability_set failed ,url={}", url)->flush();
				}
				return;
			}

			remote_channels_.clear();
			for (PINDEX i = 0; i < body.m_capabilityTable.GetSize(); i++)
			{
				media_channel_t remote_chann;
				const H245_CapabilityTableEntry& entry = body.m_capabilityTable[i];
				remote_chann.channel_number = entry.m_capabilityTableEntryNumber;
				if (entry.HasOptionalField(H245_CapabilityTableEntry::e_capability))
				{
					if (entry.m_capability.GetTag() == H245_Capability::e_receiveAndTransmitAudioCapability
						|| entry.m_capability.GetTag() == H245_Capability::e_transmitAudioCapability
						|| entry.m_capability.GetTag() == H245_Capability::e_receiveAudioCapability)
					{
						remote_chann.type = media_channel_type_t::notmal;
						remote_chann.media_type = media_type_audio;
						const H245_AudioCapability& cap = (const H245_AudioCapability&)entry.m_capability;
						if (cap.GetTag() == H245_AudioCapability::e_g711Alaw64k)
						{
							remote_chann.codec_type = codec_type_pcma;
						}
					}
					else if (entry.m_capability.GetTag() == H245_Capability::e_receiveAndTransmitVideoCapability
						|| entry.m_capability.GetTag() == H245_Capability::e_transmitVideoCapability
						|| entry.m_capability.GetTag() == H245_Capability::e_receiveVideoCapability)
					{
						 remote_chann.media_type = media_type_video;

						 const H245_VideoCapability& cap = (const H245_VideoCapability&)entry.m_capability;

						 if (cap.GetTag() == H245_VideoCapability::e_genericVideoCapability)
						 {
							 remote_chann.type = media_channel_type_t::notmal;
						 	 const H245_GenericCapability& gen = (const H245_GenericCapability&)cap;
							 if (gen.m_capabilityIdentifier.GetTag() == H245_CapabilityIdentifier::e_standard)
							 {
								if (((const PASN_ObjectId&)gen.m_capabilityIdentifier) == OpalPluginCodec_Identifer_H264_Generic)
								{
									remote_chann.codec_type = codec_type_h264;
									if (gen.HasOptionalField(H245_GenericCapability::e_collapsing))
									{
										h323_util::get_video_collapsing(gen.m_collapsing, remote_chann.profile, remote_chann.level, remote_chann.mbps, remote_chann.mfs, remote_chann.max_nal_size);
									}
								}
							 }

						 }
						 else if (cap.GetTag() == H245_VideoCapability::e_extendedVideoCapability)
						 {
							 remote_chann.type = media_channel_type_t::extend;
							 const H245_ExtendedVideoCapability& ext = (const H245_ExtendedVideoCapability&)cap;
							 for (PINDEX j=0;j<ext.m_videoCapability.GetSize();j++)
							 {
								 auto& cap2 = ext.m_videoCapability[j];
								 if (cap2.GetTag() == H245_VideoCapability::e_genericVideoCapability)
								 {
									 const H245_GenericCapability& gen = (const H245_GenericCapability&)cap2;
									 if (gen.m_capabilityIdentifier.GetTag() == H245_CapabilityIdentifier::e_standard)
									 {
										 if (((const PASN_ObjectId&)(gen.m_capabilityIdentifier)) == OpalPluginCodec_Identifer_H264_Generic)
										 {
											 remote_chann.codec_type = codec_type_h264;
											 if (gen.HasOptionalField(H245_GenericCapability::e_collapsing))
											 {
												 h323_util::get_video_collapsing(gen.m_collapsing, remote_chann.profile, remote_chann.level, remote_chann.mbps, remote_chann.mfs, remote_chann.max_nal_size);
											 }
										 }
									 }
								 }
								 break;
							 }
						 }
					}
					else if (entry.m_capability.GetTag() == H245_Capability::e_genericControlCapability)
					{
						remote_chann.media_type = media_type_unknown;
						const H245_GenericCapability& gen = (const H245_GenericCapability&)entry.m_capability;
						if (((const PASN_ObjectId&)(gen.m_capabilityIdentifier)) == "0.0.8.239.1.1")
						{
							remote_chann.type = media_channel_type_t::h239_control;
						}
					}
				}


				remote_channels_.push_back(remote_chann);
			}

			this->send_terminal_capability_set_ack(body.m_sequenceNumber);
			rtp_.start();
			start_open_local_chnnnels();

			if (log_)
			{
				std::string url = url_.to_string();

				std::stringstream ss;
				ss << "H.323: on_terminal_capability_set" << " url=" << url << std::endl;
				for (auto itr = remote_channels_.begin(); itr != remote_channels_.end(); itr++)
				{
					ss << "chan=" << itr->channel_number << ",";
					ss << "codec=" << itr->codec_type << ",";
					ss << "media=" << itr->media_type << ",";
					ss << "pt=" << itr->media_type << ",";
				}
				ss << std::endl;
				std::string s = ss.str();
				log_->debug(s)->flush();
			}
		}

		void h323_call::on_terminal_capability_set_ack(const H245_TerminalCapabilitySetAck& body)
		{
			if (log_)
			{
				std::string url = url_.to_string();
				log_->debug("H.323: on_terminal_capability_set ack ,url={}", url)->flush();
			}
			
		}

		void h323_call::on_terminal_capability_set_reject(const H245_TerminalCapabilitySetReject& body)
		{
			if (log_)
			{
				std::string url = url_.to_string();
				log_->error("H.323: on_terminal_capability_set_reject ,url={},chann={},cause={}", url,(int)body.m_sequenceNumber, (int)body.m_cause.GetTag())->flush();
			}
		}


		void h323_call::on_master_slave_determination(const H245_MasterSlaveDetermination& body)
		{
			if (body.m_terminalType < terminal_type_)
			{
				master_slave_status_ = master_slave_status_t::master;
			}
			else if (body.m_terminalType > terminal_type_)
			{
				master_slave_status_ = master_slave_status_t::slave;
			}
			else
			{
				DWORD modulo_diff = (body.m_statusDeterminationNumber - determination_number_) & 0xffffff;
				if (modulo_diff == 0 || modulo_diff == 0x800000)
				{
					master_slave_status_ = master_slave_status_t::indeterminate;
				}
				else if (modulo_diff < 0x800000)
				{
					master_slave_status_ = master_slave_status_t::master;
				}
				else
				{
					master_slave_status_ = master_slave_status_t::slave;
				}
			}

			if (master_slave_status_ != master_slave_status_t::indeterminate)
			{
				send_master_slave_determination_ack(master_slave_status_ != master_slave_status_t::master);
				if (log_)
				{
					std::string url = url_.to_string();
					log_->debug("H.323: master_slave_determination ,url={},myrole={}", url, (int)master_slave_status_)->flush();
				}
			}
			else
			{
				// retry
				determination_tries++;
				if (determination_tries <= 5)
				{
					determination_number_ = PRandom::Number() % 16777216;
					send_master_slave_determination();
					if (log_)
					{
						std::string url = url_.to_string();
						log_->warn("H.323: master_slave_determination retry...,url={}", url)->flush();
					}
				}
				else
				{
					this->send_master_slave_determination_reject();
					if (log_)
					{
						std::string url = url_.to_string();
						log_->error("H.323: send_master_slave_determination_reject,retry count exceed ,url={}", url)->flush();
					}
				}
			}

		}

		void h323_call::on_master_slave_determination_ack(const H245_MasterSlaveDeterminationAck& body)
		{
			master_slave_status_t status;
			if (body.m_decision.GetTag() == H245_MasterSlaveDeterminationAck_decision::e_master)
				status = master_slave_status_t::master;
			else
				status = master_slave_status_t::slave;

			if (master_slave_status_ == master_slave_status_t::indeterminate)
			{
				master_slave_status_ = status;
				send_master_slave_determination_ack(master_slave_status_ != master_slave_status_t::master);
			}

			if (log_)
			{
				std::string url = url_.to_string();
				log_->debug("H.323: on_master_slave_determination_ack,url={}, myrole={}", url, (int)master_slave_status_)->flush();
			}
		}

		void h323_call::on_master_slave_determination_reject(const H245_MasterSlaveDeterminationReject& body)
		{
			if (log_)
			{
				std::string url = url_.to_string();
				log_->debug("H.323: on_master_slave_determination_reject ,url={}", url)->flush();
			}
		}


		void h323_call::on_open_logical_channel(const H245_OpenLogicalChannel& body)
		{
			media_channel_t open_chann;
			PIPSocket::Address media_address, media_ctrl_address;
			WORD media_port = 0, media_ctrl_port = 0;
			open_chann.channel_number = body.m_forwardLogicalChannelNumber;
			if (body.m_forwardLogicalChannelParameters.m_dataType.GetTag() == H245_DataType::e_audioData)
			{
				open_chann.media_type = media_type_audio;
				open_chann.mid = "audio";
				open_chann.type = media_channel_type_t::notmal;
				const H245_AudioCapability& cap = (const H245_AudioCapability&)body.m_forwardLogicalChannelParameters.m_dataType;
				open_chann.codec_type = h323_util::convert_audio_codec_type((H245_AudioCapability::Choices)cap.GetTag());
			}
			else if (body.m_forwardLogicalChannelParameters.m_dataType.GetTag() == H245_DataType::e_videoData)
			{
				open_chann.media_type = media_type_video;
				open_chann.codec_type = codec_type_h264;
				const H245_VideoCapability& cap = (const H245_VideoCapability&)body.m_forwardLogicalChannelParameters.m_dataType;
				H245_VideoCapability::Choices vcodec = (H245_VideoCapability::Choices)cap.GetTag();
				if (vcodec == H245_VideoCapability::Choices::e_genericVideoCapability)
				{
					const H245_GenericCapability& vcap = (const H245_GenericCapability&)cap;
					open_chann.codec_type = codec_type_h264;
					open_chann.type = media_channel_type_t::notmal;
					open_chann.mid = "video";
				}
				else if (vcodec == H245_VideoCapability::Choices::e_extendedVideoCapability)
				{
					const H245_ExtendedVideoCapability& excap = (const H245_ExtendedVideoCapability&)cap;
					open_chann.codec_type = codec_type_h264;
					open_chann.type = media_channel_type_t::extend;
					open_chann.mid = "extend_video";
				}
			}
			else if (body.m_forwardLogicalChannelParameters.m_dataType.GetTag() == H245_DataType::e_h235Media)
			{
				const H245_H235Media& h325_media = (const H245_H235Media&)body.m_forwardLogicalChannelParameters.m_dataType;
				
				if (h325_media.m_mediaType.GetTag() == H245_H235Media_mediaType::e_audioData)
				{
					open_chann.mid = "audio";
					open_chann.media_type = media_type_t::media_type_audio;
					const H245_AudioCapability& cap = (const H245_AudioCapability&)h325_media.m_mediaType;
					open_chann.type = media_channel_type_t::notmal;
					open_chann.codec_type = h323_util::convert_audio_codec_type((H245_AudioCapability::Choices)cap.GetTag());

				}
				else if (h325_media.m_mediaType.GetTag() == H245_H235Media_mediaType::e_videoData)
				{
					open_chann.media_type = media_type_video;
					open_chann.codec_type = codec_type_h264;
					const H245_VideoCapability& cap = (const H245_VideoCapability&)h325_media.m_mediaType;
					H245_VideoCapability::Choices vcodec = (H245_VideoCapability::Choices)cap.GetTag();
					if (vcodec == H245_VideoCapability::Choices::e_genericVideoCapability)
					{
						const H245_GenericCapability& vcap = (const H245_GenericCapability&)cap;
						open_chann.codec_type = codec_type_h264;
						open_chann.type = media_channel_type_t::notmal;
						open_chann.mid = "video";
					}
					else if (vcodec == H245_VideoCapability::Choices::e_extendedVideoCapability)
					{
						const H245_ExtendedVideoCapability& excap = (const H245_ExtendedVideoCapability&)cap;
						open_chann.codec_type = codec_type_h264;
						open_chann.type = media_channel_type_t::extend;
						open_chann.mid = "extend_video";
					}
				}
			}

			if (body.m_forwardLogicalChannelParameters.m_multiplexParameters.GetTag() == H245_OpenLogicalChannel_forwardLogicalChannelParameters_multiplexParameters::e_h2250LogicalChannelParameters)
			{
				const H245_H2250LogicalChannelParameters& h2250 = (const H245_H2250LogicalChannelParameters&)(body.m_forwardLogicalChannelParameters.m_multiplexParameters);
				open_chann.session_id=h2250.m_sessionID;

				if (h2250.HasOptionalField(H245_H2250LogicalChannelParameters::e_dynamicRTPPayloadType))
				{
					open_chann.payload_type = h2250.m_dynamicRTPPayloadType;
				}
				if (h2250.HasOptionalField(H245_H2250LogicalChannelParameters::e_mediaChannel))
				{
					h323_util::h245_transport_unicast_address_get(h2250.m_mediaChannel, media_address,media_port);
				}
				if (h2250.HasOptionalField(H245_H2250LogicalChannelParameters::e_mediaControlChannel))
				{
					h323_util::h245_transport_unicast_address_get(h2250.m_mediaControlChannel, media_ctrl_address, media_ctrl_port);
				}
			}

			if (body.HasOptionalField(H245_OpenLogicalChannel::e_genericInformation))
			{
				for (PINDEX i = 0; i < body.m_genericInformation.GetSize(); i++)
				{
					const H245_GenericInformation& info = (const H245_GenericInformation&)body.m_genericInformation[i];
					if (info.HasOptionalField(H245_GenericInformation::e_messageContent))
					{
						for (PINDEX j = 0; j < info.m_messageContent.GetSize(); j++)
						{
							const H245_GenericParameter& pms = (const H245_GenericParameter&)info.m_messageContent[j];
							if (pms.m_parameterValue.GetTag() == H245_ParameterValue::e_octetString)
							{
								const PASN_OctetString& val = (const PASN_OctetString&)pms.m_parameterValue;
								H46019_TraversalParameters traversal;
								if (val.DecodeSubType(traversal))
								{
									if (traversal.HasOptionalField(H46019_TraversalParameters::e_keepAliveChannel))
									{
										h323::h323_util::h245_transport_unicast_address_get(traversal.m_keepAliveChannel, media_address, media_port);
									}
								}
							}
						}
					}
				}
			}

			//int defpt=h323_util::get_default_payloadtype(open_chann.codec_type);
			//if (defpt >= 0) {
			//	open_chann.payload_type = defpt;
			//}
			if (open_chann.payload_type == 0) {
				if (open_chann.media_type == media_type_audio) {
					open_chann.payload_type = h323_util::get_default_payloadtype(open_chann.codec_type);
				}
			}

			if (open_chann.channel_number<0||media_ctrl_address.IsAny() || open_chann.payload_type == 0 || open_chann.session_id == -1)
			{
				send_open_logical_channel_reject(open_chann.channel_number, H245_OpenLogicalChannelReject_cause::e_unspecified);
				return;
			}

			//create_local_channel()
			auto ms = get_media_stream(open_chann.media_type, open_chann.mid);
			if (!ms)
			{
				send_open_logical_channel_reject(open_chann.channel_number, H245_OpenLogicalChannelReject_cause::e_dataTypeNotSupported);
				return;
			}

			if (open_chann.media_type == media_type_audio) {
				ms->add_remote_audio_track(open_chann.codec_type,open_chann.payload_type,8000,1);
			}
			else if (open_chann.media_type == media_type_video) {
				ms->add_remote_video_track(open_chann.codec_type,open_chann.payload_type);
			}
			else
			{
				send_open_logical_channel_reject(open_chann.channel_number, H245_OpenLogicalChannelReject_cause::e_unknownDataType);
				return;
			}

			{
				std::lock_guard<std::recursive_mutex> lk(opened_remote_channels_mutex_);
				opened_remote_channels_.push_back(open_chann);
			}

			auto sdp=ms->get_local_sdp();
			send_open_logical_channel_ack(open_chann.channel_number, open_chann.session_id,sdp.rtp_address,sdp.rtp_port,sdp.rtcp_address,sdp.rtcp_port);

			PString media_address_str = media_address.AsString();
			if (!media_address_str.IsEmpty() && media_port > 0)
			{
				sockaddr_storage rtp_addr = {};
				sys::socket::ep2addr(media_address.AsString().c_str(), media_port, (sockaddr*)&rtp_addr);
				ms->set_remote_rtp_endpoint((const sockaddr*)&rtp_addr, sizeof(rtp_addr), 0);
			}

			sockaddr_storage rtcp_addr = {};
			sys::socket::ep2addr(media_ctrl_address.AsString().c_str(), media_ctrl_port, (sockaddr*)&rtcp_addr);
			ms->set_remote_rtcp_endpoint((const sockaddr*)&rtcp_addr, sizeof(rtcp_addr), 0);

			// Now can send meida data to remote.
			ms->run_stun_request();

			if (on_open_media)
			{
				auto self = shared_from_this();
				on_open_media(self, open_chann.media_type, open_chann.mid);
			}

			if (log_)
			{
				std::string url = url_.to_string();
				log_->debug("H.323: on_open_logical_channel ,url={}, chann={}, media={},code={}, rtp={}:{}, rtcp={}:{}",
					url, open_chann.channel_number,open_chann.media_type,open_chann.codec_type, sdp.rtp_address, sdp.rtp_port, sdp.rtcp_address, sdp.rtcp_port)->flush();
			}
		}

		void h323_call::on_open_logical_channel_ack(const H245_OpenLogicalChannelAck& body)
		{
			int channel_number = body.m_forwardLogicalChannelNumber;
			PIPSocket::Address media_address, media_ctrl_address;
			WORD media_port = 0, media_ctrl_port = 0;

			if (!body.HasOptionalField(H245_OpenLogicalChannelAck::e_forwardMultiplexAckParameters))
			{
				if (log_)
				{
					std::string url = url_.to_string();
					log_->error("H.323: on_open_logical_channel_ack failed: forwardMultiplexAckParameters is required ,url={}, chann={}",
						url, channel_number)->flush();
				}
				return;
			}
			media_channel_t local_chann;
			if (!get_local_channel(channel_number, local_chann))
			{
				if (local_chann.type == media_channel_type_t::extend)
				{
					on_h239_presentation_role_changed(true, false);
				}
				if (log_)
				{
					std::string url = url_.to_string();
					log_->error("H.323: on_open_logical_channel_ack failed: channel not exist ,url={}, chann={}",
						url, channel_number)->flush();
				}
				return;
			}
			auto ms = get_media_stream(local_chann.media_type,local_chann.mid);
			if (!ms)
			{
				if (local_chann.type == media_channel_type_t::extend)
				{
					on_h239_presentation_role_changed(true, false);
				}
				if (log_)
				{
					std::string url = url_.to_string();
					std::string addr1 = (const std::string&)media_address.AsString();
					std::string addr2 = (const std::string&)media_ctrl_address.AsString();
					log_->error("H.323: on_open_logical_channel_ack failed: create media stream failed ,url={}, chann={}, media={},code={}, rtp={}:{}, rtcp={}:{}",
						url, channel_number, (int)local_chann.media_type, (int)local_chann.codec_type,
						addr1, media_port, addr2, media_ctrl_port)->flush();
				}
				return;
			}

			const H245_H2250LogicalChannelAckParameters& h2250 = (const H245_H2250LogicalChannelAckParameters&)body.m_forwardMultiplexAckParameters;
			if (h2250.HasOptionalField(H245_H2250LogicalChannelAckParameters::e_dynamicRTPPayloadType))
			{
				local_chann.payload_type = h2250.m_dynamicRTPPayloadType;
			}
			if (h2250.HasOptionalField(H245_H2250LogicalChannelAckParameters::e_mediaChannel))
			{
				h323_util::h245_transport_unicast_address_get(h2250.m_mediaChannel, media_address, media_port);
			}
			if (h2250.HasOptionalField(H245_H2250LogicalChannelAckParameters::e_mediaControlChannel))
			{
				h323_util::h245_transport_unicast_address_get(h2250.m_mediaControlChannel, media_ctrl_address, media_ctrl_port);
			}

			if (local_chann.media_type == media_type_audio) {

				ms->add_local_audio_track(local_chann.codec_type, local_chann.payload_type, 8000);
			}
			else if (local_chann.media_type == media_type_video) {

				ms->add_local_video_track(local_chann.codec_type, local_chann.payload_type, 90000);
			}

			sockaddr_storage rtp_addr = {};
			sys::socket::ep2addr(media_address.AsString().c_str(), media_port, (sockaddr*)&rtp_addr);
			ms->set_remote_rtp_endpoint((const sockaddr*)&rtp_addr, sizeof(rtp_addr), 0);

			sockaddr_storage rtcp_addr = {};
			sys::socket::ep2addr(media_ctrl_address.AsString().c_str(), media_ctrl_port, (sockaddr*)&rtcp_addr);
			ms->set_remote_rtcp_endpoint((const sockaddr*)&rtcp_addr, sizeof(rtcp_addr), 0);
			
			// Now can send meida data to remote.
			ms->run_stun_request();

			if (local_chann.type == media_channel_type_t::extend)
			{
				on_h239_presentation_role_changed(true, true);
			}

			send_miscellaneous_indication(channel_number);

			if (log_)
			{
				std::string url = url_.to_string();
				std::string addr1 = (const std::string&)media_address.AsString();
				std::string addr2 = (const std::string&)media_ctrl_address.AsString();
				log_->debug("H.323: on_open_logical_channel_ack ,url={}, chann={}, media={},code={},pt={}, rtp={}:{}, rtcp={}:{}",
					url, channel_number, (int)local_chann.media_type, (int)local_chann.codec_type, local_chann.payload_type,
					addr1, media_port, addr2, media_ctrl_port)->flush();
			}
		}

		void h323_call::on_open_logical_channel_reject(const H245_OpenLogicalChannelReject& body)
		{
			media_channel_t local_chann;
			if (get_local_channel(body.m_forwardLogicalChannelNumber, local_chann))
			{
				if (local_chann.type == media_channel_type_t::extend)
				{
					on_h239_presentation_role_changed(true, false);
				}
			}
			{
				std::lock_guard<std::recursive_mutex> lk(opened_remote_channels_mutex_);
				for (auto itr = opened_remote_channels_.begin(); itr != opened_remote_channels_.end();)
				{
					if (itr->channel_number == body.m_forwardLogicalChannelNumber)
					{
						itr=opened_remote_channels_.erase(itr);
					}
					else
					{
						itr++;
					}
				}
			}

			if (log_)
			{
				std::string url = url_.to_string();
				log_->error("H.323: on_open_logical_channel_reject ,url={}, chann={}",
					url, (int)body.m_forwardLogicalChannelNumber)->flush();
			}
		}

		void h323_call::on_close_logical_channel(const H245_CloseLogicalChannel& body)
		{
			{
				std::lock_guard<std::recursive_mutex> lk(opened_remote_channels_mutex_);
				for (auto itr = opened_remote_channels_.begin(); itr != opened_remote_channels_.end();)
				{
					if (itr->channel_number == body.m_forwardLogicalChannelNumber)
					{
						if (on_close_media)
						{
							auto self = shared_from_this();
							on_close_media(self, itr->media_type,itr->mid);
						}
						itr = opened_remote_channels_.erase(itr);

					}
					else
					{
						itr++;
					}
				}
			}
			if (log_)
			{
				std::string url = url_.to_string();
				log_->debug("H.323: on_close_logical_channel ,url={}, chann={}",
					url, (int)body.m_forwardLogicalChannelNumber)->flush();
			}
		}

		void h323_call::on_round_trip_delay_request(const H245_RoundTripDelayRequest& body)
		{
			send_round_trip_delay_response(body.m_sequenceNumber);
		}

		void h323_call::on_miscellaneous_command(const H245_MiscellaneousCommand& body)
		{
			if (body.m_type.GetTag() == H245_MiscellaneousCommand_type::e_videoFastUpdatePicture)
			{
				if (on_received_require_keyframe)
				{
					auto self = shared_from_this();
					on_received_require_keyframe(self);
				}
				if (log_)
				{
					std::string url = url_.to_string();
					log_->debug("H.323: on_miscellaneous_command videoFastUpdatePicture,url={}, chann={}",
						url, (int)body.m_logicalChannelNumber)->flush();
				}
			}
		}

		void h323_call::on_end_session_command(const H245_EndSessionCommand& body)
		{
		}

		void h323_call::on_miscellaneous_indication(const H245_MiscellaneousIndication& body)
		{

		}

		void h323_call::on_flow_control_indication(const H245_FlowControlIndication& body)
		{

		}

		void h323_call::on_conference_indication(const H245_ConferenceIndication& body)
		{
			if (body.GetTag() == H245_ConferenceIndication::e_terminalNumberAssign)
			{
				const H245_TerminalLabel& label = (const H245_TerminalLabel&)body;
				mcu_number_ = label.m_mcuNumber;
				terminal_number_ = label.m_terminalNumber;
			}
		}

		void h323_call::on_h239_message(unsigned sub_message, const H245_ArrayOf_GenericParameter& params)
		{
			switch (sub_message) {
				case 1: // flowControlReleaseRequest GenericRequest
				{
					return on_h239_flow_control_release_request(H323GetGenericParameterInteger(params, 42),
						H323GetGenericParameterInteger(params, 41));
				}
				case 2: // flowControlReleaseResponse GenericResponse
				{
					return on_h239_flow_control_release_response(H323GetGenericParameterInteger(params, 42),
						H323GetGenericParameterBoolean(params, 127));
				}
				case 3: // presentationTokenRequest GenericRequest
				{
					return on_h239_presentation_request(H323GetGenericParameterInteger(params, 42),
						H323GetGenericParameterInteger(params, 43),
						H323GetGenericParameterInteger(params, 44));
				}
				case 4: // presentationTokenResponse GenericResponse
				{
					return on_h239_presentation_response(H323GetGenericParameterInteger(params, 42),
						H323GetGenericParameterInteger(params, 44),
						H323GetGenericParameterBoolean(params, 127));
				}
				case 5: // presentationTokenRelease GenericCommand
				{
					return on_h239_presentation_release(H323GetGenericParameterInteger(params, 42),
						H323GetGenericParameterInteger(params, 44));
				}
				case 6: // presentationTokenIndicateOwner GenericIndication
				{
					return on_h239_presentation_indication(H323GetGenericParameterInteger(params, 42),
						H323GetGenericParameterInteger(params, 44));
				}
			}
		}

		void h323_call::on_h239_flow_control_release_request(unsigned logical_channel, unsigned bit_rate)
		{
			send_h239_flow_control_release_response(logical_channel, true);
		}

		void h323_call::on_h239_flow_control_release_response(unsigned logical_channel, bool rejected)
		{

		}

		void h323_call::on_h239_presentation_request(unsigned logical_channel, unsigned symmetry_breaking, unsigned terminal_label)
		{
			bool ack = false;
			if (h239_symmetry_breaking_ != 0) {
				// Our request is in progress, 11.2.4/H.239
				if (h239_symmetry_breaking_ > symmetry_breaking)
				{
					ack = false; // No, we have it
				}
				else if (h239_symmetry_breaking_ < symmetry_breaking) 
				{
					ack = true;
					h239_token_owned_ = false;
					h239_symmetry_breaking_ = 0;
				}
				else 
				{
					// Try again
					h239_symmetry_breaking_ = PRandom::Number(1, 127);
					send_h239_presentation_request(h239_token_channel_, h239_symmetry_breaking_,h239_terminal_label_);
					return;
				}
			}
			else if (!h239_token_owned_)
			{
				ack = true; // 11.2.1/H.239 not owned
				h239_token_channel_ = logical_channel;
				h239_terminal_label_ = terminal_label;
			}
			else 
			{
				ack = false;
			}

			if (ack) {
				on_h239_presentation_role_changed(h239_token_owned_, ack);
			}
			send_h239_presentation_response(logical_channel, ack, terminal_label);
		}

		void h323_call::on_h239_presentation_response(unsigned logical_channel, unsigned terminal_label, bool rejected)
		{
			// Did we request it?
			if (h239_symmetry_breaking_ == 0)
			{
				send_h239_presentation_release(logical_channel, terminal_label); // No
				return;
			}

			h239_symmetry_breaking_ = 0;
			h239_token_owned_ = !rejected;
			if (rejected)
			{
				on_h239_presentation_role_changed(h239_token_owned_, false);
				h239_symmetry_breaking_ = 0;
				h239_token_owned_ = false;
				h239_terminal_label_ = 0;
				h239_token_channel_ = 0;
			}
			else
			{
				// Trigger when logical channel is opened.
				//on_h239_presentation_role_changed(h239_token_owned_, true);

				media_channel_t remote, local;
				if (find_extend_video(remote, local))
				{
					auto codec = H245_VideoCapability::Choices::e_extendedVideoCapability;
					local.session_id = local.session_id;

					auto ms = get_media_stream(local.media_type, local.mid);
					if (ms)
					{
						{
							std::lock_guard<std::recursive_mutex> lk(opened_local_channels_mutex_);
							opened_local_channels_.push_back(local);
						}
						auto sdp = ms->get_local_sdp();
						H245_ArrayOf_GenericParameter collapsing;
						h323_util::fill_video_collapsing(collapsing, remote.profile, remote.level, remote.mbps, remote.mfs, remote.max_nal_size);

						this->send_open_video_logical_channel(local.channel_number, codec, local.session_id, local.max_bitrate, local.payload_type,
							PIPSocket::Address(sdp.rtcp_address), sdp.rtcp_port, collapsing);
						send_h239_presentation_owner(h239_token_channel_, h239_terminal_label_);
					}
					else
					{
						send_h239_presentation_release(logical_channel, terminal_label);
						on_h239_presentation_role_changed(h239_token_owned_, false);
						h239_symmetry_breaking_ = 0;
						h239_token_owned_ = false;
						h239_terminal_label_ = 0;
						h239_token_channel_ = 0;
					}
				}
				else
				{
					send_h239_presentation_release(logical_channel, terminal_label);
					on_h239_presentation_role_changed(h239_token_owned_, false);
					h239_symmetry_breaking_ = 0;
					h239_token_owned_ = false;
					h239_terminal_label_ = 0;
					h239_token_channel_ = 0;
				}
			}
		}

		void h323_call::on_h239_presentation_release(unsigned logical_channel, unsigned terminal_label)
		{
			on_h239_presentation_role_changed(h239_token_owned_, false);
			remove_media_stream("extend_video");
		}

		void h323_call::on_h239_presentation_indication(unsigned logical_channel, unsigned terminal_label)
		{

		}

		void h323_call::on_h239_presentation_role_changed(bool owned, bool active)
		{
			if (on_presentation_role_changed)
			{
				auto self = shared_from_this();
				on_presentation_role_changed(self, owned, active);
			}
		}


		void h323_call::handle_read_h225(call_ptr call, const std::error_code& ec, std::size_t bytes)
		{
			if (ec)
			{
				if (on_hangup)
				{
					auto self = shared_from_this();
					on_hangup(self, call::reason_code_t::net_error);
				}
				return;
			}
			if (!h225_buffer_.write(h225_recv_buffer_.data(),bytes))
			{
				if (log_)
				{
					std::string url = url_.to_string();
					log_->error("H.323: h225 buffer is full, url={}", url)->flush();
				}
				return;
			}

			for (;;)
			{
				size_t bufsize = h225_buffer_.available_to_read();
				if (bufsize < 4)
					break;

				PBYTEArray tpktlen(4);
				h225_buffer_.peek(tpktlen.GetPointer(), 4);
				uint16_t len = 0, datalen = 0;
				if (!tpkt::deserialize(tpktlen, 0, len))
				{
					break;
				}
				if (bufsize < len)
					break;

				datalen = len - 4;
				h225_buffer_.remove(4);
				if (datalen > 0)
				{
					PBYTEArray tpkt(datalen);
					h225_buffer_.peek(tpkt.GetPointer(), datalen);
					h225_buffer_.remove(datalen);

					on_h225_tpkt(tpkt);
				}
			}
			if (active_) {
				h225_skt_->async_read_some(asio::buffer(h225_recv_buffer_), std::bind(&h323_call::handle_read_h225, this, call, std::placeholders::_1, std::placeholders::_2));
			}
		}

		void h323_call::handle_read_h245(call_ptr call, const std::error_code& ec, std::size_t bytes)
		{
			if (ec)
			{
				if (on_hangup)
				{
					auto self = shared_from_this();
					on_hangup(self, call::reason_code_t::hangup);
				}
				return;
			}
			if (!h245_buffer_.write(h245_recv_buffer_.data(), bytes))
			{
				if (log_)
				{
					std::string url = url_.to_string();
					log_->error("H.323: h245 buffer is full, url={}", url)->flush();
				}
				if (on_hangup)
				{
					auto self = shared_from_this();
					on_hangup(self, call::reason_code_t::hangup);
				}
				return;
			}


			for (;;)
			{
				size_t bufsize = h245_buffer_.available_to_read();
				if (bufsize < 4)
					break;

				PBYTEArray tpktlen(4);
				h245_buffer_.peek(tpktlen.GetPointer(), 4);
				uint16_t len = 0, datalen = 0;;
				if (!tpkt::deserialize(tpktlen, 0, len))
				{
					break;
				}

				if (bufsize < len)
					break;

				datalen = len - 4;
				h245_buffer_.remove(4);
				if (datalen > 0)
				{
					PBYTEArray tpkt(datalen);
					h245_buffer_.peek(tpkt.GetPointer(), datalen);
					h245_buffer_.remove(datalen);

					on_h245_tpkt(tpkt);
				}
			}

			if (active_) {
				h245_skt_->async_read_some(asio::buffer(h245_recv_buffer_), std::bind(&h323_call::handle_read_h245, this, call, std::placeholders::_1, std::placeholders::_2));
			}
		}

		void h323_call::handle_accept_h245(call_ptr call, const std::error_code& ec, asio::ip::tcp::socket socket)
		{
			if (ec)
				return;

			h245_skt_ = std::make_shared<asio::ip::tcp::socket>(std::move(socket));

			std::error_code ec2;
			h245_listen_.close(ec2);

			this->send_terminal_capability_set();
			this->send_master_slave_determination();

			h245_skt_->async_read_some(asio::buffer(h245_recv_buffer_), std::bind(&h323_call::handle_read_h245, this, call, std::placeholders::_1, std::placeholders::_2));

			timer_.expires_after(std::chrono::milliseconds(1000));
			timer_.async_wait(std::bind(&h323_call::handle_timer, this, call, std::placeholders::_1));

			if (log_)
			{
				std::string url = url_.to_string();
				log_->debug("H245 accepted,url={}", url)->flush();
			}

			invoke_connected();
		}

		void h323_call::handle_connect_h245(call_ptr call, const std::error_code& ec)
		{
			if (ec)
				return;

			h245_skt_->async_read_some(asio::buffer(h245_recv_buffer_), std::bind(&h323_call::handle_read_h245, this, call, std::placeholders::_1, std::placeholders::_2));

			this->send_terminal_capability_set();
			this->send_master_slave_determination();
			this->send_empty();

			timer_.expires_after(std::chrono::milliseconds(1000));
			timer_.async_wait(std::bind(&h323_call::handle_timer, this, call, std::placeholders::_1));

			invoke_connected();
		}

		void h323_call::handle_timer(call_ptr call, const std::error_code& ec)
		{
			if (ec||!active_)
				return;

			heartbeat_++;
			if (heartbeat_ >= 15) {
				send_round_trip_delay_request();
				send_facility(H225_FacilityReason::Choices::e_undefinedReason, false, true);

				if (h239_token_owned_)
				{
					send_h239_presentation_owner(h239_token_channel_, h239_terminal_label_);
				}
				heartbeat_ = 0;
			}
			if (auto_keyframe_interval_ > 0)
			{
				keyframe_++;
				if (keyframe_ >= auto_keyframe_interval_) {
					require_keyframe();
					keyframe_ = 0;
				}
			}

			if (active_) {
				timer_.expires_after(std::chrono::milliseconds(1000));
				timer_.async_wait(std::bind(&h323_call::handle_timer, this, call, std::placeholders::_1));
			}
		}

		
	}
}
