/**
 * @file socket.h
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */


#pragma once

#include <string>
#include <vector>
#include <memory>


#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
//#include <Windows.h>
#include<WinSock2.h>
#include<WS2tcpip.h> 
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
typedef int SOCKET;
#endif

namespace sys {

#ifdef WIN32
enum class shutdown_how_t
{
	sd_rd= SD_RECEIVE,
	sd_wr= SD_SEND,
	sd_both= SD_BOTH,
};
#else
enum class shutdown_how_t
{
	sd_rd = SHUT_RD,
	sd_wr = SHUT_WR,
	sd_both = SHUT_RDWR,
};
#endif

class socket;

typedef std::shared_ptr<socket> socket_ptr;

class socket
{
public:
	/// <summary>
	/// Create new socket handle
	/// </summary>
	/// <param name="af">AF_INET,AF_INET6</param>
	/// <param name="type">SOCK_DGRAM,SOCK_STREAM</param>
	/// <param name="protocol">IPPROTO_UDP,IPPROTO_TCP</param>
	socket(int af,int type,int protocol);

	/// <summary>
	/// Create with exist handle
	/// </summary>
	/// <param name="skt"></param>
	socket(SOCKET skt);

	~socket();



	static int global_init();
	static void global_cleanup();

	static void addr2ep(const sockaddr* addr, std::string* ip, int* port);
	static void ep2addr(unsigned short family, const char* ip, int port, sockaddr* addr);
	static void ep2addr(const char* ip, int port, sockaddr* addr);

	static bool is_big_endian();

	static uint64_t swap64(uint64_t v);
	static uint32_t swap32(uint32_t v);
	static uint16_t swap16(uint16_t v);

	static uint64_t hton64(uint64_t v);
	static uint32_t hton32(uint32_t v);
	static uint16_t hton16(uint16_t v);
	static uint64_t ntoh64(uint64_t v);
	static uint32_t ntoh32(uint32_t v);
	static uint16_t ntoh16(uint16_t v);

	static bool is_ipv4(const std::string& ip);
	static bool is_ipv6(const std::string& ip);

	static bool get_addresses_byhost(const char* host, std::vector<sockaddr_storage>& addresses);
	
	bool ready();
	static bool ready(SOCKET skt);

	bool shutdown(shutdown_how_t how);
	static bool shutdown(SOCKET skt,shutdown_how_t how);

	void close();
	static void close(SOCKET skt);

	SOCKET handle()const { return socket_; }

	bool connect(const std::string& ip,int port);
	bool connect(const sockaddr* addr, socklen_t addr_size);
	static bool connect(SOCKET skt,const std::string& ip, int port);
	static bool connect(SOCKET skt, const sockaddr* addr, socklen_t addr_size);

	bool bind(const sockaddr* addr, socklen_t addr_size);
	bool listen(int q);

	socket_ptr accept();

	int send(const char* buffer, int size);
	int sendto(const char* buffer, int size, const sockaddr* addr, socklen_t addr_size);

	int recv(char* buffer, int size);
	int recvfrom(char* buffer, int size, sockaddr* addr, socklen_t* addrSize);

	bool remote_addr(sockaddr* addr, socklen_t* size);
	bool remote_addr(std::string& ip, int& port);
	static bool remote_addr(SOCKET skt, sockaddr* addr, socklen_t* size);
	static bool remote_addr(SOCKET skt, std::string& ip, int& port);

	bool local_addr(sockaddr* addr, socklen_t* size);
	bool local_addr(std::string& ip, int& port);
	static bool local_addr(SOCKET skt, sockaddr* addr, socklen_t* size);
	static bool local_addr(SOCKET skt, std::string& ip, int& port);

	bool set_reuseaddr(bool enable);
	static bool set_reuseaddr(SOCKET skt,bool enable);
	bool get_reuseaddr();
	static bool get_reuseaddr(SOCKET skt);

	bool set_sendbuf_size(int size);
	static bool set_sendbuf_size(SOCKET skt,int size);
	int get_sendbuf_size();
	static int get_sendbuf_size(SOCKET skt);

	bool set_recvbuf_size(int size);
	static bool set_recvbuf_size(SOCKET skt, int size);
	int get_recvbuf_size();
	static int get_recvbuf_size(SOCKET skt);

	bool set_timeout(int ms);
	static bool set_timeout(SOCKET skt,int ms);

	u_long available_size();
	static u_long available_size(SOCKET skt);

	static bool set_ipv6_only(SOCKET skt, bool on);
private:
	SOCKET socket_ = -1;
};



class socket_selector
{
private:
	fd_set read_set_;
	fd_set write_set_;
	fd_set error_set_;
	SOCKET maxfd_;
public:
	socket_selector();
	~socket_selector();

	void reset();
	void add_to_read(SOCKET skt);
	void add_to_write(SOCKET skt);

	bool is_readable(SOCKET skt);
	bool is_writeable(SOCKET skt);

	int wait(int32_t sec = -1);
};

}
