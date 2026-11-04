#include "bfcp.h"
#include <sys2/string_util.h>
#include <cstring>

namespace voip
{
	namespace sip
	{
		namespace bfcp
		{
			static void write_u16(std::string& s, uint16_t v)
			{
				s.append(1, (char)((v >> 8) & 0xFF));
				s.append(1, (char)(v & 0xFF));
			}

			static void write_u32(std::string& s, uint32_t v)
			{
				s.append(1, (char)((v >> 24) & 0xFF));
				s.append(1, (char)((v >> 16) & 0xFF));
				s.append(1, (char)((v >> 8) & 0xFF));
				s.append(1, (char)(v & 0xFF));
			}

			static uint16_t read_u16(const uint8_t* p)
			{
				return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
			}

			static uint32_t read_u32(const uint8_t* p)
			{
				return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
			}

			// ---------- 属性编码 ----------

			// 属性 header：Type(7bit)|M(1bit) + Length(8bit，不含 padding)
			static void append_attribute_header(std::string& s, uint8_t type, uint8_t length)
			{
				s.append(1, (char)((uint8_t)((type << 1) & 0xFE)));
				s.append(1, (char)length);
			}

			// Unsigned16 属性（BENEFICIARY-ID / FLOOR-ID / FLOOR-REQUEST-ID），总长 4 字节
			static void append_u16_attribute(std::string& s, uint8_t type, uint16_t value)
			{
				append_attribute_header(s, type, 4);
				write_u16(s, value);
			}

			// REQUEST-STATUS 属性（OctetString16：status + queue position），总长 4 字节
			static void append_request_status_attribute(std::string& s, uint8_t status, uint8_t queue_position)
			{
				append_attribute_header(s, attribute::request_status, 4);
				s.append(1, (char)status);
				s.append(1, (char)queue_position);
			}

			// 分组属性：header(Type|M + Length + 16位ID) + body
			// Length = 4(header) + body_size（body 内部已含各自 padding）
			static void append_grouped(std::string& s, uint8_t type, uint16_t id, const std::string& body)
			{
				append_attribute_header(s, type, (uint8_t)(4 + body.size()));
				write_u16(s, id);
				s += body;
			}

			// ---------- 报文构建 ----------

			// COMMON-HEADER：Ver(3bit)=1|Reserved(5) + Primitive + PayloadLength(4字节单位，不含header)
			static std::string build_message(uint8_t primitive, uint32_t conference_id,
				uint16_t transaction_id, uint16_t user_id, const std::string& body)
			{
				std::string s;
				s.append(1, (char)0x20);                       // Ver=1 << 5
				s.append(1, (char)primitive);
				uint16_t units = (uint16_t)(body.size() / 4);
				write_u16(s, units);
				write_u32(s, conference_id);
				write_u16(s, transaction_id);
				write_u16(s, user_id);
				s += body;
				return s;
			}

			std::string build_hello(uint32_t conference_id, uint16_t transaction_id, uint16_t user_id)
			{
				return build_message(primitive::hello, conference_id, transaction_id, user_id, "");
			}

			std::string build_hello_ack(uint32_t conference_id, uint16_t transaction_id, uint16_t user_id)
			{
				// HelloAck = header + SUPPORTED-PRIMITIVES + SUPPORTED-ATTRIBUTES
				std::string body;
				{
					std::string prims;
					for (uint8_t p : { (uint8_t)primitive::floor_request, (uint8_t)primitive::floor_release,
						(uint8_t)primitive::floor_request_status, (uint8_t)primitive::floor_status,
						(uint8_t)primitive::hello, (uint8_t)primitive::hello_ack, (uint8_t)primitive::error })
					{
						prims.append(1, (char)p);
					}
					std::string padded = prims;
					while (padded.size() % 4 != 0)
					{
						padded.append(1, '\0');
					}
					// Length 不含 padding：2(header) + 原始内容长度
					append_attribute_header(body, attribute::supported_primitives, (uint8_t)(2 + prims.size()));
					body += padded;
				}
				{
					std::string attrs;
					for (uint8_t a : { (uint8_t)attribute::beneficiary_id, (uint8_t)attribute::floor_id,
						(uint8_t)attribute::floor_request_id, (uint8_t)attribute::request_status, (uint8_t)attribute::error_code })
					{
						attrs.append(1, (char)((a << 1) & 0xFE));
					}
					std::string padded = attrs;
					while (padded.size() % 4 != 0)
					{
						padded.append(1, '\0');
					}
					append_attribute_header(body, attribute::supported_attributes, (uint8_t)(2 + attrs.size()));
					body += padded;
				}
				return build_message(primitive::hello_ack, conference_id, transaction_id, user_id, body);
			}

			std::string build_floor_request(uint32_t conference_id, uint16_t transaction_id, uint16_t user_id,
				uint16_t floor_id, uint16_t beneficiary_id)
			{
				// FloorRequest = header + 1*FLOOR-ID + [BENEFICIARY-ID]
				std::string body;
				append_u16_attribute(body, attribute::floor_id, floor_id);
				if (beneficiary_id > 0 && beneficiary_id != user_id)
				{
					append_u16_attribute(body, attribute::beneficiary_id, beneficiary_id);
				}
				return build_message(primitive::floor_request, conference_id, transaction_id, user_id, body);
			}

			std::string build_floor_release(uint32_t conference_id, uint16_t transaction_id, uint16_t user_id,
				uint16_t floor_request_id)
			{
				// FloorRelease = header + FLOOR-REQUEST-ID
				std::string body;
				append_u16_attribute(body, attribute::floor_request_id, floor_request_id);
				return build_message(primitive::floor_release, conference_id, transaction_id, user_id, body);
			}

			std::string build_floor_request_status(uint32_t conference_id, uint16_t transaction_id, uint16_t user_id,
				uint16_t floor_request_id, uint16_t floor_id, uint8_t status)
			{
				// FloorRequestStatus = header + FLOOR-REQUEST-INFORMATION
				//   FRI header(Floor Request ID) + FLOOR-REQUEST-STATUS(Floor ID + REQUEST-STATUS) + OVERALL-REQUEST-STATUS(Floor Request ID + REQUEST-STATUS)
				std::string fr_status_body;
				append_request_status_attribute(fr_status_body, status, 0);
				std::string fr_status;
				append_grouped(fr_status, attribute::floor_request_status_attr, floor_id, fr_status_body);

				std::string overall_body;
				append_request_status_attribute(overall_body, status, 0);
				std::string overall;
				append_grouped(overall, attribute::overall_request_status, floor_request_id, overall_body);

				std::string fri_body = fr_status + overall;
				std::string fri;
				append_grouped(fri, attribute::floor_request_information, floor_request_id, fri_body);

				return build_message(primitive::floor_request_status, conference_id, transaction_id, user_id, fri);
			}

			// ---------- 解析 ----------

			// 遍历属性（32 位对齐）。Length 不含 padding。
			// 返回 false 表示格式非法
			template<typename Func>
			static bool for_each_attribute(const uint8_t* data, size_t size, Func&& func)
			{
				size_t offset = 0;
				while (offset + 2 <= size)
				{
					uint8_t type = (uint8_t)(data[offset] >> 1);
					uint8_t length = data[offset + 1];
					if (length < 2 || offset + length > size)
					{
						return false;
					}

					func(type, data + offset, length);

					size_t padded = (length + 3) & ~(size_t)3;
					offset += padded;
				}
				return offset == size;
			}

			// 分组属性内容：2 字节 header ID + 子属性（递归解析）
			static uint16_t parse_grouped(const uint8_t* attr, uint8_t length, message& msg)
			{
				uint16_t id = length >= 4 ? read_u16(attr + 2) : 0;
				if (length > 4)
				{
					for_each_attribute(attr + 4, length - 4, [&](uint8_t type, const uint8_t* sub, uint8_t sub_length)
					{
						switch (type)
						{
						case attribute::request_status:
							if (sub_length >= 4)
							{
								msg.status = sub[2];
							}
							break;
						case attribute::beneficiary_information:
							// BENEFICIARY-INFORMATION: header(Beneficiary ID) + [USER-DISPLAY-NAME/USER-URI]
							if (sub_length >= 4)
							{
								msg.beneficiary_id = read_u16(sub + 2);
							}
							break;
						case attribute::floor_request_status_attr:
							// FLOOR-REQUEST-STATUS: header(Floor ID) + REQUEST-STATUS
							if (sub_length >= 4)
							{
								for_each_attribute(sub + 4, sub_length - 4, [&](uint8_t t2, const uint8_t* a2, uint8_t l2)
								{
									if (t2 == attribute::request_status && l2 >= 4)
									{
										msg.status = a2[2];
									}
								});
							}
							break;
						case attribute::overall_request_status:
							// OVERALL-REQUEST-STATUS: header(Floor Request ID) + REQUEST-STATUS
							if (sub_length >= 4)
							{
								for_each_attribute(sub + 4, sub_length - 4, [&](uint8_t t2, const uint8_t* a2, uint8_t l2)
								{
									if (t2 == attribute::request_status && l2 >= 4)
									{
										msg.overall_status = a2[2];
									}
								});
							}
							break;
						default:
							break;
						}
					});
				}
				return id;
			}

			bool parse(const uint8_t* data, size_t size, message& msg)
			{
				if (size < 12)
				{
					return false;
				}
				uint8_t ver = (uint8_t)(data[0] >> 5);
				if (ver != 1)
				{
					return false;
				}
				msg.primitive = data[1];
				uint16_t payload_units = read_u16(data + 2);
				size_t total = 12 + (size_t)payload_units * 4;
				if (total > size)
				{
					return false;
				}

				msg.conference_id = read_u32(data + 4);
				msg.transaction_id = read_u16(data + 8);
				msg.user_id = read_u16(data + 10);

				const uint8_t* attrs = data + 12;
				size_t attr_size = total - 12;

				for_each_attribute(attrs, attr_size, [&](uint8_t type, const uint8_t* attr, uint8_t length)
				{
					switch (type)
					{
					case attribute::beneficiary_id:
						if (length >= 4)
						{
							msg.beneficiary_id = read_u16(attr + 2);
						}
						break;
					case attribute::floor_id:
						if (length >= 4)
						{
							msg.floor_ids.push_back(read_u16(attr + 2));
						}
						break;
					case attribute::floor_request_id:
						if (length >= 4)
						{
							msg.floor_request_id = read_u16(attr + 2);
						}
						break;
					case attribute::request_status:
						if (length >= 4)
						{
							msg.status = attr[2];
						}
						break;
					case attribute::error_code:
						if (length >= 3)
						{
							msg.error_code = attr[2];
						}
						break;
					case attribute::floor_request_information:
						// 分组：header(Floor Request ID) + FLOOR-REQUEST-STATUS + OVERALL-REQUEST-STATUS
						if (length >= 4)
						{
							msg.floor_request_id = parse_grouped(attr, length, msg);
						}
						break;
					default:
						break;
					}
				});

				return true;
			}
		}

		// ---------- bfcp_client ----------

		bfcp_client::bfcp_client(asio::io_context& ioc)
			:ioc_(ioc)
		{
		}

		bfcp_client::~bfcp_client()
		{
			stop();
		}

		bool bfcp_client::start(spdlogger_ptr log, uint16_t local_port)
		{
			if (active_)
			{
				return true;
			}
			log_ = log;

			std::error_code ec;
			skt_ = std::make_shared<asio::ip::udp::socket>(ioc_);
			skt_->open(asio::ip::udp::v4(), ec);
			if (ec)
			{
				if (log_)
				{
					log_->error("BFCP: open socket failed, err={}", ec.message())->flush();
				}
				skt_.reset();
				return false;
			}

			// 通配绑定，与媒体 transport 一致；端口被占时 bind 失败，由上层换端口重试
			asio::ip::udp::endpoint ep(asio::ip::udp::v4(), local_port);
			skt_->bind(ep, ec);
			if (ec)
			{
				if (log_)
				{
					log_->error("BFCP: bind 0.0.0.0:{} failed, err={}", local_port, ec.message())->flush();
				}
				skt_->close(ec);
				skt_.reset();
				return false;
			}
			// local_port 为 0 时由操作系统分配空闲端口，回读 socket 实际端口
			local_port_ = skt_->local_endpoint(ec).port();
			if (ec)
			{
				if (log_)
				{
					log_->error("BFCP: query local endpoint failed, err={}", ec.message())->flush();
				}
				skt_->close(ec);
				skt_.reset();
				return false;
			}
			active_ = true;

			do_receive();
			return true;
		}

		void bfcp_client::stop()
		{
			bool expected = true;
			if (!active_.compare_exchange_strong(expected, false))
			{
				return;
			}
			std::error_code ec;
			if (skt_)
			{
				skt_->cancel(ec);
				skt_->close(ec);
				skt_.reset();
			}
		}

		void bfcp_client::set_remote(const std::string& ip, uint16_t port)
		{
			std::error_code ec;
			auto addr = asio::ip::address::from_string(ip, ec);
			if (!ec)
			{
				remote_ = asio::ip::udp::endpoint(addr, port);
			}
		}

		void bfcp_client::send(const std::string& data)
		{
			// socket 操作需在 io_context 线程执行，投递保证串行安全
			if (!active_ || !skt_ || remote_.port() == 0)
			{
				return;
			}
			auto self = shared_from_this();
			auto data_ptr = std::make_shared<std::string>(data);
			asio::post(ioc_, [self, data_ptr]()
			{
				if (!self->active_ || !self->skt_)
				{
					return;
				}
				self->skt_->async_send_to(asio::buffer(*data_ptr), self->remote_,
					[self, data_ptr](const std::error_code& ec, size_t bytes)
					{
						self->handle_send(ec, bytes);
					});
			});
		}

		void bfcp_client::handle_send(const std::error_code& ec, size_t bytes)
		{
			if (ec)
			{
				if (log_)
				{
					log_->warn("BFCP: send failed, err={}, bytes={}", ec.message(), bytes)->flush();
				}
			}
		}

		void bfcp_client::do_receive()
		{
			auto self = shared_from_this();
			skt_->async_receive_from(asio::buffer(recv_buffer_), recv_ep_,
				[self](const std::error_code& ec, size_t bytes)
				{
					self->handle_receive(ec, bytes);
				});
		}

		void bfcp_client::handle_receive(const std::error_code& ec, size_t bytes)
		{
			if (!active_)
			{
				return;
			}
			if (ec)
			{
				if (log_)
				{
					log_->warn("BFCP: receive failed, err={}", ec.message())->flush();
				}
				if (close_handler_)
				{
					close_handler_();
				}
				return;
			}

			bfcp::message msg;
			if (bfcp::parse(recv_buffer_.data(), bytes, msg))
			{
				if (msg.primitive == bfcp::primitive::hello)
				{
					// 保活探活自动应答（RFC 4582 §13.7）
					std::string ack = bfcp::build_hello_ack(msg.conference_id, msg.transaction_id, msg.user_id);
					asio::error_code err;
					skt_->send_to(asio::buffer(ack), recv_ep_, asio::socket_base::message_flags(0), err);
				}

				if (message_handler_)
				{
					message_handler_(msg, recv_ep_);
				}
			}

			do_receive();
		}
	}
}
