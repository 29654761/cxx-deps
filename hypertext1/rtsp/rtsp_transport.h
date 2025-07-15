#pragma once

#include <string>
#include <sstream>

class rtsp_transport
{
public:
	rtsp_transport();
	~rtsp_transport();

	bool parse(const std::string& s);
	std::string to_string()const;

private:
	void parse_protos(const std::string protos);
	void make_protos(std::stringstream& ss)const;
public:
	bool is_rtp = true;
	bool is_avp = true;
	bool is_udp = true;
	bool is_tcp = false;
	bool unicast = true;

	int rtp_channel = 0;
	int rtcp_channel = 1;
	uint32_t ssrc = 0;
	std::string mode = "";  //play record

	std::string destination = "";
	std::string source = "";

	int rtp_client_port = 0;
	int rtcp_client_port = 0;
	int rtp_server_port = 0;
	int rtcp_server_port = 0;
};

