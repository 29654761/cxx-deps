/**
 * @file socket.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */


#include "socket.h"
#include <string.h>


namespace sys{


socket::socket(int af, int type, int protocol)
{
	socket_ = ::socket(af, type,protocol);
	if (ready())
	{
		int no = 0;
		setsockopt(socket_, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&no, sizeof(no));
	}
}

socket::socket(SOCKET skt)
{
	socket_ = skt;
}

socket::~socket()
{
	close();
}

int socket::global_init() 
{
#ifdef _WIN32
	WSADATA wd;
	memset(&wd, 0, sizeof(wd));
	WSAStartup(MAKEWORD(2, 2), &wd);
#else

#endif // _WIN32

	return 0;
}


void socket::global_cleanup()
{
#ifdef _WIN32
	WSACleanup();
#else

#endif // _WIN32
}


void socket::addr2ep(const sockaddr* addr, std::string* ip, int* port) 
{
	if (addr->sa_family == AF_INET) {
		char sip[16] = { 0 };
		sockaddr_in* sin = (sockaddr_in*)addr;
		inet_ntop(AF_INET, &sin->sin_addr, sip, 16);
		ip->assign(sip);
		*port = ntohs(sin->sin_port);
	}
	else {
		char sip[64] = { 0 };
		sockaddr_in6* sin = (sockaddr_in6*)addr;
		inet_ntop(AF_INET6, &sin->sin6_addr, sip, 64);
		ip->assign(sip);
		*port = ntohs(sin->sin6_port);
	}
}


void socket::ep2addr(unsigned short family,const char* ip, int port, sockaddr* addr)
{
	if (family== AF_INET) {
		sockaddr_in* sin = (sockaddr_in*)addr;
		sin->sin_family = AF_INET;
		sin->sin_port = htons(port);
		inet_pton(AF_INET, ip?ip:"0.0.0.0", &sin->sin_addr);
	}
	else if(family == AF_INET6){
		sockaddr_in6* sin = (sockaddr_in6*)addr;
		sin->sin6_family = AF_INET6;
		sin->sin6_port = htons(port);

		if (!ip)
		{
			inet_pton(AF_INET6, "::", &sin->sin6_addr);
		}
		else 
		{
			std::string s = ip;
			if (is_ipv4(s))
			{
				s = "::ffff:" + s;
			}
			inet_pton(AF_INET6, s.c_str(), &sin->sin6_addr);
		}
	}
}

void socket::ep2addr(const char* ip, int port, sockaddr* addr)
{
	if (is_ipv4(ip))
	{
		ep2addr(AF_INET, ip, port, addr);
	}
	else if(is_ipv6(ip))
	{
		ep2addr(AF_INET6, ip, port, addr);
	}
}


bool socket::is_big_endian() 
{
	union {
		int i;
		char c[4];
	} v = { 0x00000001 };
	return v.c[3] == 1;
}

uint64_t socket::swap64(uint64_t v)
{
	uint64_t r = 0;
	char* p = (char*)&r;
	p[0] = (char)(v >> 56);
	p[1] = (char)((v & 0x00FF000000000000) >> 48);
	p[2] = (char)((v & 0x0000FF0000000000) >> 40);
	p[3] = (char)((v & 0x000000FF00000000) >> 32);
	p[4] = (char)((v & 0x00000000FF000000) >> 24);
	p[5] = (char)((v & 0x0000000000FF0000) >> 16);
	p[6] = (char)((v & 0x000000000000FF00) >> 8);
	p[7] = (char)((v & 0x00000000000000FF));
	return r;
}

uint32_t socket::swap32(uint32_t v)
{
	uint32_t r = 0;
	char* p = (char*)&r;
	p[0] = (char)(v >> 24);
	p[1] = (char)((v & 0x00FF0000) >> 16);
	p[2] = (char)((v & 0x0000FF00) >> 8);
	p[3] = (char)((v & 0x000000FF));
	return r;
}

uint16_t socket::swap16(uint16_t v)
{
	uint16_t r = 0;
	char* p = (char*)&r;
	p[0] = (char)(v >> 8);
	p[1] = (char)(v & 0x00FF);
	return r;
}

uint64_t socket::hton64(uint64_t v)
{
	if (is_big_endian()) {
		return v;
	}
	else {
		return swap64(v);
	}
}
uint32_t socket::hton32(uint32_t v)
{
	if (is_big_endian()) {
		return v;
	}
	else {
		return swap32(v);
	}
}
uint16_t socket::hton16(uint16_t v) 
{
	if (is_big_endian()) {
		return v;
	}
	else {
		return swap16(v);
	}
}
uint64_t socket::ntoh64(uint64_t v)
{
	if (is_big_endian()) {
		return v;
	}
	else {
		return swap64(v);
	}
}
uint32_t socket::ntoh32(uint32_t v)
{
	if (is_big_endian()) {
		return v;
	}
	else {
		return swap32(v);
	}
}
uint16_t socket::ntoh16(uint16_t v)
{
	if (is_big_endian()) {
		return v;
	}
	else {
		return swap16(v);
	}
}

bool socket::is_ipv4(const std::string& ip)
{
	int dotcnt = 0, colon = 0;
	for (int i = 0; i < ip.length(); i++)
	{
		if (ip[i] == '.')
		{
			dotcnt++;
		}
		else if (ip[i] == ':')
		{
			colon++;
		}
	}

	return dotcnt == 3 && colon == 0;
}

bool socket::is_ipv6(const std::string& ip)
{
	int dotcnt = 0, colon = 0;
	for (int i = 0; i < ip.length(); i++)
	{
		if (ip[i] == '.')
		{
			dotcnt++;
		}
		else if (ip[i] == ':')
		{
			colon++;
		}
	}

	return (colon == 7&& dotcnt==0)||(colon == 3 && dotcnt == 3);
}

bool socket::get_addresses_byhost(const char* host, std::vector<sockaddr_storage>& addresses)
{
	hostent* ent = gethostbyname(host);
	if (!ent) {
		return false;
	}

	int i = 0;
	while (ent->h_addr_list[i])
	{
		sockaddr_storage ss = { 0 };
		memcpy(&ss, ent->h_addr_list[i], ent->h_length);
		addresses.push_back(ss);
		i++;
	}
	return true;
}

bool socket::shutdown(shutdown_how_t how)
{
	return shutdown(socket_, how);
}

bool socket::shutdown(SOCKET skt, shutdown_how_t how)
{
	int r = ::shutdown(skt, (int)how);
	return r >= 0;
}

void socket::close()
{
	if (socket_ >= 0) {
		close(socket_);
		socket_ = -1;
	}
}

void socket::close(SOCKET skt)
{
#ifdef _WIN32
	closesocket(skt);
#else
	shutdown(skt,shutdown_how_t::sd_both);
	::close(skt);
#endif
}



bool socket::ready()
{
	return ready(socket_);
}
bool socket::ready(SOCKET skt)
{
#ifdef _WIN32
	return skt != INVALID_SOCKET;
#else
	return skt >= 0;
#endif
}

bool socket::connect(const std::string& ip, int port)
{
	sockaddr_storage addr = {};
	sys::socket::ep2addr(ip.c_str(), port, (sockaddr*)&addr);
	return connect((const sockaddr*)&addr, sizeof(addr));
}

bool socket::connect(const sockaddr* addr, socklen_t addr_size)
{
	if (!ready())
		return false;

	int r = ::connect(socket_, addr, addr_size);
	if (r != 0) {
		return false;
	}

	return true;
}

bool socket::connect(SOCKET skt, const std::string& ip, int port)
{
	sockaddr_storage addr = {};
	sys::socket::ep2addr(ip.c_str(), port, (sockaddr*)&addr);
	return connect(skt, (const sockaddr*)&addr, sizeof(addr));
}

bool socket::connect(SOCKET skt, const sockaddr* addr, socklen_t addr_size)
{
	int r = ::connect(skt,addr,addr_size);
	if (r != 0) {
		return false;
	}

	return true;
}


bool socket::bind(const sockaddr* addr, socklen_t addr_size)
{
	int r = ::bind(socket_, addr, addr_size);
	if (r < 0)
		return false;

	return true;
}
bool socket::listen(int q) 
{
	int r = ::listen(socket_, q);
	if (r < 0)
		return false;

	return true;
}

socket_ptr socket::accept()
{
	sockaddr_storage addr = { 0 };
	socklen_t addrlen = sizeof(addr);
	SOCKET skt = ::accept(socket_, (sockaddr*)&addr, &addrlen);
	auto ptr = std::make_shared<socket>(skt);
	if (!ptr->ready())
	{
		return nullptr;
	}
	return ptr;
}


int socket::send(const char* buffer, int size)
{
	return ::send(socket_, buffer, size, 0);
}
int socket::sendto(const char* buffer, int size, const sockaddr* addr, socklen_t addr_size)
{
	//std::string ip;
	//int port = 0;
	//addr2ep(addr, &ip, &port);
	return ::sendto(socket_, buffer, size, 0, addr, addr_size);
}

int socket::recv(char* buffer, int size) 
{
	return ::recv(socket_, buffer, size, 0);
}
int socket::recvfrom(char* buffer, int size, sockaddr* addr, socklen_t* addrSize) 
{
	return ::recvfrom(socket_, buffer, size, 0, addr, addrSize);
}

bool socket::remote_addr(sockaddr* addr, socklen_t* size)
{
	return remote_addr(socket_, addr, size);
}
bool socket::remote_addr(std::string& ip, int& port)
{
	return remote_addr(socket_, ip, port);
}
bool socket::remote_addr(SOCKET skt, sockaddr* addr, socklen_t* size)
{
	return getpeername(skt, addr, size) == 0;
}
bool socket::remote_addr(SOCKET skt, std::string& ip, int& port)
{
	sockaddr_storage addr = {};
	socklen_t size = sizeof(addr);

	if (getpeername(skt, (sockaddr*)&addr, &size) != 0)
	{
		return false;
	}
	addr2ep((const sockaddr*)&addr, &ip, &port);
	return true;
}


bool socket::local_addr(sockaddr* addr, socklen_t* size)
{
	return local_addr(socket_, addr, size);
}
bool socket::local_addr(std::string& ip, int& port)
{
	return local_addr(socket_, ip, port);
}
bool socket::local_addr(SOCKET skt, sockaddr* addr, socklen_t* size)
{
	return getsockname(skt, addr, size) == 0;
}
bool socket::local_addr(SOCKET skt, std::string& ip, int& port)
{
	sockaddr_storage addr = {};
	socklen_t size = sizeof(addr);

	if (getsockname(skt, (sockaddr*)&addr, &size) != 0)
	{
		return false;
	}
	addr2ep((const sockaddr*)&addr, &ip, &port);
	return true;
}

bool socket::set_reuseaddr(bool enable)
{
	return set_reuseaddr(socket_, enable);
}

bool socket::set_reuseaddr(SOCKET skt, bool enable)
{
	int v = enable ? 1 : 0;
	if (setsockopt(skt, SOL_SOCKET, SO_REUSEADDR, (const char*)&v, sizeof(int)) < 0) {
		return false;
	}
	return true;
}

bool socket::get_reuseaddr()
{
	return get_reuseaddr(socket_);
}

bool socket::get_reuseaddr(SOCKET skt)
{
	int v = 0;
	socklen_t s = sizeof(v);
	getsockopt(skt, SOL_SOCKET, SO_REUSEADDR, (char*)&v, &s);
	return !!v;
}

bool socket::set_sendbuf_size(int size)
{
	return set_sendbuf_size(socket_, size);
}

bool socket::set_sendbuf_size(SOCKET skt, int size)
{
	if (setsockopt(skt, SOL_SOCKET, SO_SNDBUF, (const char*)&size, (socklen_t)sizeof(size)) < 0) {
		return false;
	}
	return true;
}

int socket::get_sendbuf_size()
{
	return get_sendbuf_size(socket_);
}

int socket::get_sendbuf_size(SOCKET skt)
{
	int v = 0;
	socklen_t s = sizeof(v);
	getsockopt(skt, SOL_SOCKET, SO_SNDBUF, (char*)&v, &s);
	return v;
}

bool socket::set_recvbuf_size(int size)
{
	return set_recvbuf_size(socket_, size);
}

bool socket::set_recvbuf_size(SOCKET skt, int size)
{
	if (setsockopt(skt, SOL_SOCKET, SO_RCVBUF, (const char*)&size, sizeof(size)) < 0) {
		return false;
	}
	return true;
}

int socket::get_recvbuf_size()
{
	return get_recvbuf_size(socket_);
}

int socket::get_recvbuf_size(SOCKET skt)
{
	int v = 0;
	socklen_t s = sizeof(v);
	getsockopt(skt, SOL_SOCKET, SO_RCVBUF, (char*)&v, &s);
	return v;
}

bool socket::set_timeout(int ms)
{
	return set_timeout(socket_,ms);
}

bool socket::set_timeout(SOCKET skt, int ms)
{
#ifdef _WIN32
	DWORD timeout = ms;
	int r = setsockopt(skt, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof timeout);
	return r == 0 ? true : false;
#else
	struct timeval tv;
	tv.tv_sec = ms / 1000;
	tv.tv_usec = (ms % 1000) * 1000;
	int r = setsockopt(skt, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
	return r == 0 ? true : false;
#endif
}

u_long socket::available_size()
{
	return available_size(socket_);
}

u_long socket::available_size(SOCKET skt)
{
	u_long avail = 0;
#ifdef _WIN32
	ioctlsocket(skt, FIONREAD, &avail);
#else
	ioctl(skt, FIONREAD, &avail);
#endif

	return avail;
}

bool socket::set_ipv6_only(SOCKET skt,bool on)
{
	int no = on ?1:0;
	int r=setsockopt(skt, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&no, sizeof(no));
	return r == 0;
}


socket_selector::socket_selector()
{
	reset();
}

socket_selector::~socket_selector()
{
}

void socket_selector::reset()
{
	FD_ZERO(&read_set_);
	FD_ZERO(&write_set_);
	FD_ZERO(&error_set_);
	maxfd_ = 0;
}

void socket_selector::add_to_read(SOCKET skt)
{
	FD_SET(skt, &read_set_);
	if (skt > maxfd_)
		maxfd_ = skt;
}

void socket_selector::add_to_write(SOCKET skt)
{
	FD_SET(skt, &write_set_);
	if (skt > maxfd_)
		maxfd_ = skt;
}

bool socket_selector::is_readable(SOCKET skt)
{
	return FD_ISSET(skt, &read_set_);
}

bool socket_selector::is_writeable(SOCKET skt)
{
	return FD_ISSET(skt, &write_set_);
}

int socket_selector::wait(int32_t sec)
{
	timeval tv = { sec,0 };
	return select((int)(maxfd_ + 1), &read_set_, &write_set_, &error_set_, sec >= 0 ? (&tv) : nullptr);
}


}