/**
 * @file sdp_media.h
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */


#pragma once

#include "sdp_format.h"
#include "candidate.h"

namespace litertp {

	struct ssrc_t
	{
		uint32_t ssrc=0;
		std::string cname;
		std::string msid;
	};

	class sdp_media
	{
	public:
		sdp_media();
		~sdp_media();

	public:
		void to_string(std::stringstream& ss)const;
		bool parse_header(const std::string& m);
		bool parse_c(const std::string& c);
		bool parse_a(const std::string& a);
		void parse_a(sdp_pair& attr);

		bool negotiate(const sdp_format* fmt_in,sdp_format* fmt_out);

		uint32_t get_default_ssrc()const;

		bool has_ssrc(uint32_t ssrc)const;

		void set_msid(const std::string& msid);

		bool has_attribute(const std::string& key)const;
		std::string get_attribute(const std::string& key, const std::string& def)const;
		void remove_attrs(const std::string& key);

		bool is_security()const;
	private:
		void to_protocols_string(std::stringstream& ss)const;
		
	public:
		media_type_t media_type = media_type_audio;  //audio|video|text|application|message

		std::set<std::string> protos; //UDP/TLS/RTP/SAVPF;

		std::string app_data_type;

		std::vector<std::string> b;

		bool has_rtp_address = false;
		std::string rtp_network_type = "IN";
		std::string rtp_address_type = "IP4";
		std::string rtp_address = "0.0.0.0";
		int rtp_port = 0;

		bool has_rtcp = false;
		bool rtcp_mux = false;
		bool rtcp_rsize = false;
		std::string rtcp_network_type = "IN";
		std::string rtcp_address_type = "IP4";
		std::string rtcp_address = "0.0.0.0";
		int rtcp_port = 0;



		std::string ice_ufrag;
		std::string ice_pwd;
		std::string ice_options;

		std::string fingerprint_sign = "sha-256";
		std::string fingerprint;

		std::string mid;
		std::string msid;
		std::map<int, std::string> extmap;

		rtp_trans_mode_t trans_mode = rtp_trans_mode_sendrecv;
		sdp_setup_t setup = stp_setup_none;

		std::map<int, sdp_format> rtpmap;

		std::string ssrc_group;// = "FID";
		std::vector<ssrc_t> ssrcs;

		std::string control;
		std::vector<sdp_pair> attrs;
		std::vector<candidate> candidates;

		uint32_t y = 0; //for gb28181
	};



}
