#include "sip_udp_connection.h"

sip_udp_connection::sip_udp_connection(asio::io_context& ioc, socket_ptr socket, const asio::ip::udp::endpoint& ep)
	:sip_connection(ioc)
{
	socket_ = socket;
	ep_ = ep;

	std::string ip;
	int port = 0;
	this->remote_address(ip, port);
	// id 必须保持纯远端地址（不要学 sip_tcp_connection 呼出方向加唯一后缀）：
	// handle_read 按远端 id 查表，出向 con 挂在纯 id 下才兼作该远端的收包通道，靠响应刷新 180s 超时；
	// 加后缀会留在表里收不到 on_message，被服务器定时器回收后误挂在途通话。
	// 同目标并发呼出的撞 key 由 endpoint::handle_close 的对象级匹配兜底（不会误停呼叫）
	id_ = make_id("udp", ip, port);
}

sip_udp_connection::~sip_udp_connection()
{
}

void sip_udp_connection::stop()
{
	sip_connection::stop();
}

bool sip_udp_connection::send_message(const sip_message& message)
{
	if (!socket_)
	{
		return false;
	}
	std::string s = message.to_string();
	if (log_) {
		log_->trace("Send sip message:\n{}", s)->flush();
	}
	asio::error_code ec;
	size_t r = socket_->send_to(asio::buffer(s),ep_,0,ec);

	return r > 0;
}

bool sip_udp_connection::remote_address(std::string& ip, int& port)const
{
	ip = ep_.address().to_string();
	port = ep_.port();
	return true;
}

bool sip_udp_connection::local_address(std::string& ip, int& port)const
{
	if (!socket_)
		return false;
	// 带错误码调用：socket 未打开/已关闭时无参重载会抛 asio::system_error，
	// io 线程上无人捕获会直接 std::terminate
	std::error_code ec;
	auto ep = socket_->local_endpoint(ec);
	if (ec)
		return false;
	ip = ep.address().to_string();
	port = ep.port();
	return true;
}


