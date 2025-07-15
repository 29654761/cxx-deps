#pragma once
#include "../call.h"
#include "gk_client.h"
#include <sys2/ring_buffer.hpp>
#include <sys2/asio_queue.hpp>
#include <ptlib.h>
#include <opal.h>
#include <ptlib/ipsock.h>
#include <h323/h323pdu.h>
#include <h323/h235dh.h>
#include <atomic>
#include <asio.hpp>


namespace voip
{
	namespace h323
	{
		typedef std::shared_ptr<endpoint> endpoint_ptr;

		class h323_call:public call
		{
		public:
			friend endpoint;
			enum master_slave_status_t
			{
				indeterminate,
				master,
				slave,
			};
			enum media_channel_type_t
			{
				notmal,
				extend,
				h239_control,
			};
			struct media_channel_t
			{
				int channel_number = -1;
				int session_id = -1;
				int payload_type = 0;
				media_channel_type_t type = notmal;
				
				int max_bitrate = 19200;
				std::string mid;
				media_type_t media_type = media_type_t::media_type_unknown;
				codec_type_t codec_type = codec_type_t::codec_type_unknown;
				int frequency = 0;
				uint8_t profile = 0x40;
				uint16_t level = 0x005c;
				uint32_t mbps = 540;
				uint32_t mfs = 34;
				uint32_t max_nal_size = 1400;
			};

			typedef std::shared_ptr<asio::ip::tcp::socket> tcp_socket_ptr;
			typedef std::function<void(call_ptr, const PGloballyUniqueID&, const PGloballyUniqueID&)> on_facility_t;


			h323_call(endpoint_ptr ep, tcp_socket_ptr h225_skt, const std::string& local_alias, const std::string& remote_alias, direction_t direction,
				const std::string& nat_address, int port,
				litertp::port_range_ptr h245_ports, litertp::port_range_ptr rtp_ports);

			h323_call(endpoint_ptr ep,const asio::ip::tcp::endpoint& h225_addr, const std::string& local_alias, const std::string& remote_alias, direction_t direction,
				const std::string& nat_address, int port,
				litertp::port_range_ptr h245_ports, litertp::port_range_ptr rtp_ports);

			~h323_call();

			void set_gk_client(std::shared_ptr<gk_client> gkclient) { gkclient_ = gkclient; }

			virtual std::string id()const;
			virtual bool start(bool audio, bool video);
			virtual void stop(voip::call::reason_code_t reason);
			virtual bool require_keyframe();
			virtual bool answer();
			virtual bool refuse();
			virtual bool request_presentation_role();
			virtual void release_presentation_role();
			virtual bool has_presentation_role();

			void setup(const voip_uri& url,const std::string& call_id="", const std::string& conf_id="",int call_ref=-1);
			void facility(const voip_uri& url, const PGloballyUniqueID& call_id, const PGloballyUniqueID& conf_id);


			void set_on_facility_event(on_facility_t handler) { on_facility_event = handler; }
		private:
			bool start_h245_listen();
			bool start_h245_connect(const std::string& h245_addr,int h245_port);

			bool find_compatible_channel(const media_channel_t& chann, media_channel_t& cmp);

			void start_open_local_chnnnels();
			void close_remote_channels();
			void close_local_channels();


			void set_media_crypto_suites();

			void add_audio_channel_g711();
			void add_video_channel_h264();
			void add_extend_video_channel_h264();

			bool get_local_channel(int chann_num, media_channel_t& chann);
			bool get_remote_channel(int chann_num, media_channel_t& chann);

			litertp::media_stream_ptr get_media_stream(media_type_t mt,const std::string& mid);
			bool remove_media_stream(const std::string& mid);

			bool has_h239_control();
			bool find_extend_video(media_channel_t& remote,media_channel_t& local);
			unsigned get_local_terminal_label()const { return mcu_number_ * 256 + terminal_number_; }
		private:
			//h225
			bool send_setup(const voip_uri& url);
			bool send_facility(H225_FacilityReason::Choices reason,bool from_dest,bool heartbeat=false);
			bool send_call_proceeding();
			bool send_alerting();
			bool send_connect();
			bool send_release_complete(H225_ReleaseCompleteReason::Choices reason,bool from_dest);
			bool send_empty();

			//h245
			bool send_terminal_capability_set();
			bool send_terminal_capability_set_ack(uint32_t seq);
			bool send_terminal_capability_set_reject(uint32_t seq, H245_TerminalCapabilitySetReject_cause::Choices cause);
			bool send_master_slave_determination();
			bool send_master_slave_determination_ack(bool is_master);
			bool send_master_slave_determination_reject();

			bool send_generic_indication();
			bool send_miscellaneous_indication(int logical_channel);
			bool send_miscellaneous_command(int logical_channel); // this for require key frame
			bool send_flow_control_indication();

			bool send_request_channel_close(int logical_channel, H245_RequestChannelClose_reason::Choices reason);
			bool send_request_channel_close_ack(int logical_channel);
			bool send_request_channel_close_reject(int logical_channel, H245_RequestChannelCloseReject_cause::Choices cause);

			bool send_open_audio_logical_channel(int logical_channel, H245_AudioCapability::Choices codec,int session_id,
				int payload_type,const PIPSocket::Address& local_media_ctrl_address, WORD local_media_ctrl_port);
			bool send_open_video_logical_channel(int logical_channel, H245_VideoCapability::Choices codec, int session_id, int max_bitrate,
				int payload_type,const PIPSocket::Address& local_media_ctrl_address, WORD local_media_ctrl_port, const H245_ArrayOf_GenericParameter& collapsing);

			
			bool send_open_logical_channel_ack(int logical_channel, int session_id,
				const PString& media_addr,WORD media_port,
				const PString& media_ctrl_addr, WORD media_ctrl_port);
			bool send_open_logical_channel_reject(int logical_channel, H245_OpenLogicalChannelReject_cause::Choices cause);

			bool send_close_logical_channel(int logical_channel, H245_CloseLogicalChannel_reason::Choices reason);
			bool send_close_logical_channel_ack(int logical_channel);

			bool send_end_session_command();

			bool send_round_trip_delay_request();
			bool send_round_trip_delay_response(uint32_t seq);

			bool send_h239_flow_control_release_request(unsigned logical_channel, unsigned bit_rate);
			bool send_h239_flow_control_release_response(unsigned logical_channel, bool acknowledge);
			bool send_h239_presentation_request(unsigned logical_channel,unsigned symmetry_breaking,unsigned terminal_label);
			bool send_h239_presentation_response(unsigned logical_channel,bool acknowledge, unsigned terminal_label);
			bool send_h239_presentation_release(unsigned logical_channel,unsigned terminal_label);
			bool send_h239_presentation_owner(unsigned logical_channel, unsigned terminal_label);
		private:
			void fill_capability(H245_ArrayOf_CapabilityTableEntry& entries, H245_ArrayOf_CapabilityDescriptor& descs);

			static void s_litertp_on_require_keyframe(void* ctx, uint32_t ssrc, uint32_t ssrc_media);


			void on_h225_tpkt(const PBYTEArray& tpkt);
			void on_h245_tpkt(const PBYTEArray& tpkt);

			void on_setup(const H225_Setup_UUIE& uuie,const Q931& q931);
			void on_call_proceeding(const H225_CallProceeding_UUIE& uuie);
			void on_alerting(const H225_Alerting_UUIE& uuie);
			void on_facility(const H225_H323_UU_PDU& pdu, const Q931& q931);
			void on_connect(const H225_Connect_UUIE& uuie, const Q931& q931);
			void on_release_complete(const H225_ReleaseComplete_UUIE& uuie);

			void on_terminal_capability_set(const H245_TerminalCapabilitySet& body);
			void on_terminal_capability_set_ack(const H245_TerminalCapabilitySetAck& body);
			void on_terminal_capability_set_reject(const H245_TerminalCapabilitySetReject& body);

			void on_master_slave_determination(const H245_MasterSlaveDetermination& body);
			void on_master_slave_determination_ack(const H245_MasterSlaveDeterminationAck& body);
			void on_master_slave_determination_reject(const H245_MasterSlaveDeterminationReject& body);

			void on_open_logical_channel(const H245_OpenLogicalChannel& body);
			void on_open_logical_channel_ack(const H245_OpenLogicalChannelAck& body);
			void on_open_logical_channel_reject(const H245_OpenLogicalChannelReject& body);
			void on_close_logical_channel(const H245_CloseLogicalChannel& body);
			void on_round_trip_delay_request(const H245_RoundTripDelayRequest& body);

			void on_miscellaneous_command(const H245_MiscellaneousCommand& body);
			void on_end_session_command(const H245_EndSessionCommand& body);
			void on_miscellaneous_indication(const H245_MiscellaneousIndication& body);
			void on_flow_control_indication(const H245_FlowControlIndication& body);
			void on_conference_indication(const H245_ConferenceIndication& body);

			void on_h239_message(unsigned sub_message, const H245_ArrayOf_GenericParameter& params);
			void on_h239_flow_control_release_request(unsigned logical_channel,unsigned bit_rate);
			void on_h239_flow_control_release_response(unsigned logical_channel,bool rejected);
			void on_h239_presentation_request(unsigned logical_channel,unsigned symmetry_breaking,unsigned terminal_label);
			void on_h239_presentation_response(unsigned logical_channel,unsigned terminal_label,bool rejected);
			void on_h239_presentation_release(unsigned logical_channel,unsigned terminal_label);
			void on_h239_presentation_indication(unsigned logical_channel, unsigned terminal_label);
			void on_h239_presentation_role_changed(bool owned, bool active);
		private:
			void handle_read_h225(call_ptr call, const std::error_code& ec, std::size_t bytes);
			void handle_read_h245(call_ptr call, const std::error_code& ec, std::size_t bytes);
			void handle_accept_h245(call_ptr call, const std::error_code& ec, asio::ip::tcp::socket socket);
			void handle_timer(call_ptr call, const std::error_code& ec);
			void handle_connect_h245(call_ptr call, const std::error_code& ec);
		private:
			endpoint_ptr ep_;
			asio::ip::tcp::endpoint h225_addr_;
			std::shared_ptr<asio::ip::tcp::socket> h225_skt_;
			std::shared_ptr<asio::ip::tcp::socket> h245_skt_;
			asio::ip::tcp::acceptor h245_listen_;
			sys::ring_buffer<uint8_t> h225_buffer_;
			sys::ring_buffer<uint8_t> h245_buffer_;
			std::array<uint8_t, 10240> h225_recv_buffer_;
			std::array<uint8_t, 10240> h245_recv_buffer_;
			asio::sending_queue_ptr h225_sending_queue_;
			asio::sending_queue_ptr h245_sending_queue_;

			std::atomic<bool> active_;

			asio::steady_timer timer_;

			bool init_setup_ = false;
			bool init_facility_ = false;

			PString h245_address_;
			WORD h245_port_ = 0;
			asio::ip::tcp::endpoint h245_connect_ep_;
			bool h245_tunneling = false;
			uint32_t call_ref_ = 0;
			litertp::port_range_ptr h245_ports_;
			

			PGloballyUniqueID call_id_;
			PGloballyUniqueID conf_id_;
			unsigned int terminal_type_ = 50;
			DWORD determination_number_ = 0;
			int determination_tries = 0;
			master_slave_status_t master_slave_status_ = master_slave_status_t::indeterminate;

			std::vector<media_channel_t> local_channels_;
			std::vector<media_channel_t> remote_channels_;

			std::recursive_mutex opened_local_channels_mutex_;
			std::vector<media_channel_t> opened_local_channels_;
			std::recursive_mutex opened_remote_channels_mutex_;
			std::vector<media_channel_t> opened_remote_channels_;

			H235DiffieHellman dh_;
			std::atomic<uint32_t> request_seq_;

			std::shared_ptr<gk_client> gkclient_;

			int heartbeat_ = 0;
			int keyframe_ = 3;

			on_facility_t on_facility_event;

			bool     h239_control_ = false;
			unsigned h239_symmetry_breaking_ = 0;
			unsigned h239_token_channel_ = 0;
			unsigned h239_terminal_label_ = 0;
			unsigned mcu_number_ = 0;
			unsigned terminal_number_ = 0;
			bool h239_token_owned_ = false;
		};

		
	}
}
