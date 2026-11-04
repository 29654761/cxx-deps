#pragma once
#include <asio.hpp>
#include <spdlog/spdlogger.hpp>
#include <string>
#include <vector>
#include <array>
#include <functional>
#include <memory>
#include <atomic>

namespace voip
{
	namespace sip
	{
		// BFCP（RFC 4582）二进制楼层控制协议的最小实现：
		// 报文编解码 + UDP 传输（对端 SDP 使用 UDP/BFCP）。
		// 策略状态机（token 语义）由 sip_call 承担，本层只负责协议收发与编解码。
		namespace bfcp
		{
			// RFC 4582 Table 1: Primitive
			typedef enum primitive
			{
				floor_request = 1,
				floor_release = 2,
				floor_request_query = 3,
				floor_request_status = 4,
				user_query = 5,
				user_status = 6,
				floor_query = 7,
				floor_status = 8,
				chair_action = 9,
				chair_action_ack = 10,
				hello = 11,
				hello_ack = 12,
				error = 13,
			}primitive_t;

			// RFC 4582 Table 2: Attribute
			typedef enum attribute
			{
				beneficiary_id = 1,             // Unsigned16
				floor_id = 2,                   // Unsigned16
				floor_request_id = 3,           // Unsigned16
				priority = 4,                   // OctetString16
				request_status = 5,             // OctetString16: status + queue position
				error_code = 6,
				error_info = 7,
				participant_provided_info = 8,
				status_info = 9,
				supported_attributes = 10,
				supported_primitives = 11,
				user_display_name = 12,
				user_uri = 13,
				beneficiary_information = 14,   // Grouped
				floor_request_information = 15, // Grouped
				requested_by_information = 16,  // Grouped
				floor_request_status_attr = 17, // Grouped
				overall_request_status = 18,    // Grouped
			}attribute_t;

			// RFC 4582 Table 4: Request Status
			typedef enum status
			{
				st_pending = 1,
				st_accepted = 2,
				st_granted = 3,
				st_denied = 4,
				st_cancelled = 5,
				st_released = 6,
				st_revoked = 7,
			}status_t;

			// 解析后的扁平化消息（分组属性提取关键字段）
			struct message
			{
				uint8_t primitive = 0;
				uint32_t conference_id = 0;
				uint16_t transaction_id = 0;
				uint16_t user_id = 0;

				std::vector<uint16_t> floor_ids;      // FLOOR-ID（FloorRequest 顶层）
				uint16_t floor_request_id = 0;        // FLOOR-REQUEST-ID / 分组 header 内
				uint16_t beneficiary_id = 0;          // BENEFICIARY-ID
				uint8_t overall_status = 0;           // OVERALL-REQUEST-STATUS 内的 REQUEST-STATUS
				uint8_t status = 0;                   // FLOOR-REQUEST-STATUS 内的 REQUEST-STATUS
				uint8_t error_code = 0;               // ERROR-CODE
			};

			// ---------- 编解码 ----------
			// 解析 BFCP 报文（含分组属性的关键字段扁平化）
			bool parse(const uint8_t* data, size_t size, message& msg);

			// 构建最小消息集
			std::string build_hello(uint32_t conference_id, uint16_t transaction_id, uint16_t user_id);
			std::string build_hello_ack(uint32_t conference_id, uint16_t transaction_id, uint16_t user_id);
			// FloorRequest = header + 1*FLOOR-ID + [BENEFICIARY-ID]
			std::string build_floor_request(uint32_t conference_id, uint16_t transaction_id, uint16_t user_id,
				uint16_t floor_id, uint16_t beneficiary_id);
			// FloorRelease = header + FLOOR-REQUEST-ID
			std::string build_floor_release(uint32_t conference_id, uint16_t transaction_id, uint16_t user_id,
				uint16_t floor_request_id);
			// FloorRequestStatus = header + FLOOR-REQUEST-INFORMATION(header + OVERALL-REQUEST-STATUS + FLOOR-REQUEST-STATUS)
			// 作为 chair 响应对端请求或响应我方 FloorRelease
			std::string build_floor_request_status(uint32_t conference_id, uint16_t transaction_id, uint16_t user_id,
				uint16_t floor_request_id, uint16_t floor_id, uint8_t status);
		}

		// BFCP UDP 客户端：收发与保活应答；策略回调交给上层
		class bfcp_client : public std::enable_shared_from_this<bfcp_client>
		{
		public:
			typedef std::function<void(const bfcp::message& msg, const asio::ip::udp::endpoint& ep)> message_handler;
			typedef std::function<void()> close_handler;

			bfcp_client(asio::io_context& ioc);
			~bfcp_client();

			// 绑定 0.0.0.0:local_port（与媒体 transport 一致）。不可绑定指定 IP——Windows 下
			// "指定 IP:port" 会与已存在的 "0.0.0.0:port" 共存，且单播投递优先给指定 IP 的
			// socket，会把发往媒体端口的报文"偷"走
			bool start(spdlogger_ptr log, uint16_t local_port);
			void stop();
			uint16_t local_port()const { return local_port_; }
			bool is_active()const { return active_; }

			void set_remote(const std::string& ip, uint16_t port);

			// 线程安全（内部投递到 io_context）
			void send(const std::string& data);

			void set_message_handler(message_handler handler) { message_handler_ = std::move(handler); }
			void set_close_handler(close_handler handler) { close_handler_ = std::move(handler); }
		private:
			void do_receive();
			void handle_receive(const std::error_code& ec, size_t bytes);
			void handle_send(const std::error_code& ec, size_t bytes);

		private:
			asio::io_context& ioc_;
			std::shared_ptr<asio::ip::udp::socket> skt_;
			asio::ip::udp::endpoint remote_;
			asio::ip::udp::endpoint recv_ep_;
			std::array<uint8_t, 4096> recv_buffer_ = { 0 };
			uint16_t local_port_ = 0;
			std::atomic<bool> active_ = false;
			spdlogger_ptr log_;

			message_handler message_handler_;
			close_handler close_handler_;
		};
	}
}
