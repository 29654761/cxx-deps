/**
 * @file sctp_chunk.h
 * @brief
 * @author Shijie Zhou
 * @copyright 2025 Shijie Zhou
 */

#pragma once
#include <vector>
#include <string>



namespace rtpx
{
#define SCTP_MAX_CHUNK_SIZE 4096

	enum class sctp_chunk_type :uint8_t
	{
		data = 0x00,
		init = 0x01,
		init_ack = 0x02,
		sack = 0x03,
		heartbeat = 0x04,
		heartbeat_ack = 0x05,
		abort = 0x06,
		shutdown = 0x07,
		shutdown_ack = 0x08,
		operational_error = 0x09,
		cookie_echo = 0x0a,
		cookie_ack = 0x0b,
	};

	enum class sctp_chunk_flags :uint8_t
	{
		none = 0x00,
		end = 0x01,
		begin = 0x02,
		unordered = 0x04,
	};
	inline sctp_chunk_flags operator| (sctp_chunk_flags lhs, sctp_chunk_flags rhs) {
		return static_cast<sctp_chunk_flags>(
			static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs)
			);
	}

	enum class sctp_init_parameter_type : uint16_t
	{
		ipv4_address = 0x0005,  // IPv4 Address
		ipv6_address = 0x0006,  // IPv6 Address
		state_cookie = 0x0007,  // Cookie for INIT ACK
		supported_extensions = 0x0009,  // Supported Extensions
		heartbeat_info = 0x0008,  // Heartbeat Info for INIT TLV
		supported_address_types = 0x000C,  // e.g., IPv4/IPv6
		forwarded_tsn_supported = 0x8008,  // Forward TSN Supported（PR-SCTP）

		// RFC 4895 (SCTP Authentication)
		random = 0x8002,  // Random (auth key generation)
		hmac_algo = 0x8003,  // HMAC Algorithm
		chunk_list = 0x8004,  // Authenticated Chunk Types

		// RFC 5061 (Add-IP extension)
		add_ip_address = 0xC001,  // Add IP Address
		delete_ip_address = 0xC002,  // Delete IP Address
		set_primary_address = 0xC003,  // Set Primary Address
		asconf_ack = 0x8001,  // ASCONF ACK (dynamic addr reconfig)

		// RFC 7053 (IPv4/IPv6 address types extended)
		correlated_address = 0xC004,  // Used for multihoming/correlation
	};

	enum class sctp_error_cause : uint16_t
	{
		invalid_stream_identifier = 0x0001,
		missing_mandatory_parameter = 0x0002,
		stale_cookie_error = 0x0003,
		out_of_resource = 0x0004,
		unresolvable_address = 0x0005,
		unrecognized_chunk_type = 0x0006,
		invalid_mandatory_parameter = 0x0007,
		unrecognized_parameters = 0x0008,
		no_user_data = 0x0009,
		cookie_received_while_shutting_down = 0x000A,
		restart_with_new_address = 0x000B,
		user_initiated_abort = 0x000C,
		protocol_violation = 0x000D,

		// RFC 4895 (SCTP Authentication)
		authentication_required = 0x0101,
		unsupported_hmac_id = 0x0102,
		invalid_hmac_id = 0x0103,
		bad_message_authentication_code = 0x0104,

		// RFC 5061 (Dynamic Address Reconfiguration)
		unsupported_asconf_parameter = 0x00C1,
		invalid_asconf_parameter = 0x00C2,
		addip_unresolvable_address = 0x00C3,
		no_addip_support = 0x0100,
		delete_last_remaining_ip_address = 0x0105,
		operation_refused_resource_shortage = 0x0106
	};

	enum class sctp_payload_protocol_id:uint32_t
	{
		dcep=50,
		string=51,
		binary=53,
		empty_string=56,
		empty_binary=57,
	};

	class sctp_chunk_tlv
	{
	public:
		sctp_chunk_tlv();
		~sctp_chunk_tlv();

		size_t deserialize(const uint8_t* data, size_t size);
		std::vector<uint8_t> serialize() const;
	public:
		uint16_t type = 0;
		mutable uint16_t len = 0;
		std::vector<uint8_t> value;
	};



	class sctp_chunk
	{
	public:
		sctp_chunk();
		sctp_chunk(sctp_chunk_type type, sctp_chunk_flags flags, const std::vector<uint8_t>& payload);
		~sctp_chunk();

		size_t deserialize(const uint8_t* data, size_t size);
		std::vector<uint8_t> serialize() const;

		uint16_t length()const { return len; }
		uint16_t payload_length()const { return len - 4; }
	public:
		sctp_chunk_type type = sctp_chunk_type::data;
		sctp_chunk_flags flags = sctp_chunk_flags::none;

		mutable uint16_t len = 0;
		std::vector<uint8_t> payload;

		
	};


	/*
	0                   1                   2                   3
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |   Type = 0     |  Flags        |      Length                 |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |           TSN (Transmission Sequence Number)                 |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |     Stream ID                |    Stream Sequence Number     |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                 Payload Protocol Identifier (PPID)           |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                       User Data (variable)                   |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	*/
	class sctp_data_chunk
	{
	public:
		sctp_data_chunk();
		~sctp_data_chunk();
		bool deserialize(const uint8_t* data, size_t size);
		std::vector<uint8_t> serialize() const;

	public:
		uint32_t tsn=0; // Transmission Sequence Number
		uint16_t stream_id = 0; // Stream Identifier
		uint16_t stream_sequence = 0; // Stream Sequence Number
		sctp_payload_protocol_id payload_protocol_id = sctp_payload_protocol_id::dcep; // Payload Protocol Identifier

		std::vector<uint8_t> payload; // Payload Data
	};
	


	/*
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|  Type = 3      |  Flags = 0    |      Length                 |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|                  Cumulative TSN Ack                          |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|                      Advertised Receiver Window Credit       |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	| Number of Gap Ack Blocks      | Number of Duplicate TSNs     |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	| ... Gap Ack Block (begin) ... |... Gap Ack Block (end) ...   |
	|                              ...                             |
	| ... Duplicate TSNs ...                                       |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	*/
	class sctp_sack_chunk
	{
	public:

		struct gap_item_t
		{
			uint32_t begin = 0;
			uint32_t end = 0;
		};

		sctp_sack_chunk();
		~sctp_sack_chunk();

		bool deserialize(const uint8_t* data, size_t size);
		std::vector<uint8_t> serialize() const;
	public:
		uint32_t tsn = 0;
		uint32_t arwnd = 0; // Advertised Receiver Window Credit
		std::vector<gap_item_t> gaps;
		std::vector<uint32_t> dups;

	};

	/*
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|  Type = 4      |  Flags = 0    |      Length                 |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|            Heartbeat Info (如时间戳或 path ID)               |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	*/
	class sctp_heartbeat_chunk
	{
	public:
		sctp_heartbeat_chunk();
		~sctp_heartbeat_chunk();

		bool deserialize(const uint8_t* data, size_t size);
		std::vector<uint8_t> serialize() const;
		bool find_paramter(sctp_init_parameter_type type, std::vector<uint8_t>& value);
	public:
		std::vector<sctp_chunk_tlv> parameters;
	};

	typedef sctp_heartbeat_chunk sctp_heartbeat_ack_chunk;





	/*
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|  Type = 1      | Flags = 0     |      Length                 |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|                     Initiate Tag                             |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|                Advertised Receiver Window Credit             |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|  Number of Outbound Streams   |  Number of Inbound Streams   |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|                  Initial TSN                                |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|    Optional Parameters (Supported Extensions, Cookie, etc.)  |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	*/
	class sctp_init_chunk
	{
	public:
		sctp_init_chunk();
		~sctp_init_chunk();

		bool deserialize(const uint8_t* data, size_t size);
		std::vector<uint8_t> serialize() const;

		bool find_paramter(sctp_init_parameter_type type, std::vector<uint8_t>& value);
	public:
		uint32_t init_tag = 0;
		uint32_t arwnd = 0;
		uint16_t num_out_stream = 0;
		uint16_t num_in_stream = 0;
		uint32_t init_tsn = 0;

		std::vector<sctp_chunk_tlv> parameters;
	};

	typedef sctp_init_chunk sctp_init_ack_chunk;







	/*
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|  Type = 7      | Flags = 0     |      Length = 8             |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|        Cumulative TSN Ack                                     |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	*/
	class sctp_shutdown_chunk
	{
	public:
		sctp_shutdown_chunk();
		~sctp_shutdown_chunk();

		bool deserialize(const uint8_t* data, size_t size);
		std::vector<uint8_t> serialize() const;
	public:
		uint32_t tsn = 0;
	};


	// No body for shundown ack
	/*
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|  Type = 7      | Flags = 0     |      Length = 8             |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	*/



	/*
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|  Type = 6      | Flags (bit 0 = T) | Length                   |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	| Optional Cause Information (variable)                         |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	*/
	class sctp_abort_chunk
	{
	public:
		sctp_abort_chunk();
		~sctp_abort_chunk();

		bool deserialize(const uint8_t* data, size_t size);
		std::vector<uint8_t> serialize() const;
	private:
		std::vector<sctp_chunk_tlv> parameters;
	};


	/*
	
	*/
	class sctp_error_chunk
	{
	public:
		sctp_error_chunk();
		~sctp_error_chunk();

		bool deserialize(const uint8_t* data, size_t size);
		std::vector<uint8_t> serialize() const;
	public:
		std::vector<sctp_chunk_tlv> parameters;
	};


	class sctp_cookie_echo_chunk
	{
	public:
		sctp_cookie_echo_chunk();
		~sctp_cookie_echo_chunk();

		bool deserialize(const uint8_t* data, size_t size,uint16_t len);
		std::vector<uint8_t> serialize() const;
	public:
		std::vector<uint8_t> cookie;
	};

	// No body for cookie ack

	// No body for shundown commplete
}

