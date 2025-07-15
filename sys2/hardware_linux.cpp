/**
 * @file hardware.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */
#include "hardware.h"
#include "string_util.h"

#ifdef _LINUX_

#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <scsi/sg.h>
#include <linux/hdreg.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>

namespace sys
{
    hardware::hardware()
    {

    }

    hardware::~hardware()
    {
        cleanup();
    }


	bool hardware::init()
	{
        return true;
	}

	void hardware::cleanup()
	{
	}

    static void parse_board_serial(const char* file_name, const char* match_words, std::string& board_serial)
    {
        board_serial.c_str();

        std::ifstream ifs(file_name, std::ios::binary);
        if (!ifs.is_open())
        {
            return;
        }

        char line[4096] = { 0 };
        while (!ifs.eof())
        {
            ifs.getline(line, sizeof(line));
            if (!ifs.good())
            {
                break;
            }

            const char* board = strstr(line, match_words);
            if (NULL == board)
            {
                continue;
            }
            board += strlen(match_words);

            while ('\0' != board[0])
            {
                if (' ' != board[0])
                {
                    board_serial.push_back(board[0]);
                }
                ++board;
            }

            if ("None" == board_serial)
            {
                board_serial.clear();
                continue;
            }

            if (!board_serial.empty())
            {
                break;
            }
    }

        ifs.close();
}

    static bool get_board_serial_by_system(std::string& board_serial)
    {
        board_serial.c_str();

        const char* dmidecode_result = ".dmidecode_result.txt";
        char command[512] = { 0 };
        snprintf(command, sizeof(command), "dmidecode -t baseboard | grep Serial > %s", dmidecode_result);

        if (0 == system(command))
        {
            parse_board_serial(dmidecode_result, "Serial Number:", board_serial);
        }

        unlink(dmidecode_result);

        return(!board_serial.empty());
    }

    
	std::string hardware::get_board_serial_number()
	{
        std::string board_serial;
        if (0 == getuid())
        {
            if (get_board_serial_by_system(board_serial))
            {
                return board_serial;
            }
        }
        return board_serial;
	}





    static bool get_cpu_id_by_asm(std::string& cpu_id)
    {
        cpu_id.clear();

        unsigned int s1 = 0;
        unsigned int s2 = 0;
        asm volatile(
            "movl $0x01, %%eax; \n\t"
            "xorl %%edx, %%edx; \n\t"
            "cpuid; \n\t"
            "movl %%edx, %0; \n\t"
            "movl %%eax, %1; \n\t"
            : "=m"(s1), "=m"(s2));
        if (0 == s1 && 0 == s2)
        {
            return (false);
        }
        char cpu[128];
        memset(cpu, 0, sizeof(cpu));
        snprintf(cpu, sizeof(cpu), "%08X%08X", htonl(s2), htonl(s1));
        cpu_id.assign(cpu);
        return (true);
}

    static void parse_cpu_id(const char* file_name, const char* match_words, std::string& cpu_id)
    {
        cpu_id.c_str();

        std::ifstream ifs(file_name, std::ios::binary);
        if (!ifs.is_open())
        {
            return;
        }

        char line[4096] = { 0 };
        while (!ifs.eof())
        {
            ifs.getline(line, sizeof(line));
            if (!ifs.good())
            {
                break;
            }

            const char* cpu = strstr(line, match_words);
            if (NULL == cpu)
            {
                continue;
            }
            cpu += strlen(match_words);

            while ('\0' != cpu[0])
            {
                if (' ' != cpu[0])
                {
                    cpu_id.push_back(cpu[0]);
                }
                ++cpu;
            }

            if (!cpu_id.empty())
            {
                break;
            }
        }

        ifs.close();
    }

    static bool get_cpu_id_by_system(std::string& cpu_id)
    {
        cpu_id.c_str();

        const char* dmidecode_result = ".dmidecode_result.txt";
        char command[512] = { 0 };
        snprintf(command, sizeof(command), "dmidecode -t processor | grep ID > %s", dmidecode_result);

        if (0 == system(command))
        {
            parse_cpu_id(dmidecode_result, "ID:", cpu_id);
        }

        unlink(dmidecode_result);

        return (!cpu_id.empty());
    }


	std::string hardware::get_cpu_id()
	{
        std::string cpu_id;
        if (get_cpu_id_by_asm(cpu_id))
        {
            return cpu_id;
        }
        if (0 == getuid())
        {
            if (get_cpu_id_by_system(cpu_id))
            {
                return cpu_id;
            }
        }
        return cpu_id;
	}








    static void parse_disk_name(const char* file_name, const char* match_words, std::string& disk_name)
    {
        disk_name.c_str();

        std::ifstream ifs(file_name, std::ios::binary);
        if (!ifs.is_open())
        {
            return;
        }

        char line[4096] = { 0 };
        while (!ifs.eof())
        {
            ifs.getline(line, sizeof(line));
            if (!ifs.good())
            {
                break;
            }
            const char* board = strstr(line, match_words);
            if (NULL == board)
            {
                continue;
            }
            if (strlen(board) != 1)
            {
                continue;
            }
            break;
        }

        while (!ifs.eof())
        {
            ifs.getline(line, sizeof(line));
            if (!ifs.good())
            {
                break;
            }
            const char* board = strstr(line, "disk");
            if (NULL == board)
            {
                continue;
            }

            int i = 0;
            while (' ' != line[i])
            {
                disk_name.push_back(line[i]);
                ++i;
            }
            if (!disk_name.empty())
            {
                break;
            }
        }
        ifs.close();
    }

    static void parse_disk_serial(const char* file_name, const char* match_words, std::string& serial_no)
    {
        serial_no.c_str();

        std::ifstream ifs(file_name, std::ios::binary);
        if (!ifs.is_open())
        {
            return;
        }

        char line[4096] = { 0 };
        while (!ifs.eof())
        {
            ifs.getline(line, sizeof(line));
            if (!ifs.good())
            {
                break;
            }

            const char* board = strstr(line, match_words);
            if (NULL == board)
            {
                continue;
            }
            board += strlen(match_words);

            while ('\0' != board[0])
            {
                if (' ' != board[0])
                {
                    serial_no.push_back(board[0]);
                }
                ++board;
            }

            if ("None" == serial_no)
            {
                serial_no.clear();
                continue;
            }

            if (!serial_no.empty())
            {
                break;
            }
        }

        ifs.close();
    }

    static bool get_disk_name_system(std::string& disk_name)
    {
        disk_name.c_str();

        const char* dmidecode_result = ".dmidecode_result.txt";
        char command[512] = { 0 };
        snprintf(command, sizeof(command), "lsblk -nls > %s", dmidecode_result);

        if (0 == system(command))
        {
            parse_disk_name(dmidecode_result, "/", disk_name);
        }

        unlink(dmidecode_result);

        return (!disk_name.empty());
    }

    static bool get_disk_by_system(std::string disk_name, std::string& serial_no)
    {
        serial_no.c_str();

        const char* dmidecode_result = ".dmidecode_result.txt";
        char command[512] = { 0 };
        snprintf(command, sizeof(command), "udevadm info --query=all --name=%s | grep ID_SERIAL= > %s", disk_name.c_str(), dmidecode_result);

        if (0 == system(command))
        {
            parse_disk_serial(dmidecode_result, "ID_SERIAL=", serial_no);
        }

        unlink(dmidecode_result);

        return (!serial_no.empty());
    }

	std::string hardware::get_disk_id()
	{
        std::string disk_id;
        if (0 == getuid())
        {
            std::string disk_name;
            if (get_disk_name_system(disk_name))
            {
                if (get_disk_by_system(disk_name, disk_id))
                {
                    return disk_id;
                }
            }
        }
        return disk_id;
	}


    static char* getmac(char* mac, char* dv) 
    {
        struct   ifreq   ifreq;
        int   sock;
        if (!mac || !dv)
        {
            return mac;
        }
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            return mac;
        }
        strcpy(ifreq.ifr_name, dv);
        if (ioctl(sock, SIOCGIFHWADDR, &ifreq) < 0)
        {
            return mac;
        }
        // pHx((unsigned char*)ifreq.ifr_hwaddr.sa_data, sizeof(ifreq.ifr_hwaddr.sa_data));
        sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X", //以太网MAC地址的长度是48位
            (unsigned char)ifreq.ifr_hwaddr.sa_data[0],
            (unsigned char)ifreq.ifr_hwaddr.sa_data[1],
            (unsigned char)ifreq.ifr_hwaddr.sa_data[2],
            (unsigned char)ifreq.ifr_hwaddr.sa_data[3],
            (unsigned char)ifreq.ifr_hwaddr.sa_data[4],
            (unsigned char)ifreq.ifr_hwaddr.sa_data[5]);
        return mac;
    }

	std::set<std::string> hardware::get_mac_addresses()
	{
        std::set<std::string> macs;

        char mac[30];
        struct ifaddrs* ifap0 = NULL, *ifap = NULL;
        void* tmp = NULL;
        getifaddrs(&ifap0);
        ifap = ifap0;
        while (ifap != NULL) 
        {
            if (ifap->ifa_addr->sa_family == AF_INET)
            {
                // is a valid IP4 Address
                tmp = &((struct sockaddr_in*)ifap->ifa_addr)->sin_addr;
                char buffer[INET_ADDRSTRLEN] = {};
                inet_ntop(AF_INET, tmp, buffer, INET_ADDRSTRLEN);
                if (strcmp(buffer, "127.0.0.1") != 0)
                {
                    //printf("%s IPv4: %s\n", ifap->ifa_name, buffer);
                    //printf("MAC: %s\n\n", getmac(mac, ifap->ifa_name));
                    macs.insert(getmac(mac, ifap->ifa_name));
                }
            }
            else if (ifap->ifa_addr->sa_family == AF_INET6) 
            {
                // is a valid IP6 Address
                tmp = &((struct sockaddr_in*)ifap->ifa_addr)->sin_addr;
                char buffer[INET6_ADDRSTRLEN] = {};
                inet_ntop(AF_INET6, tmp, buffer, INET6_ADDRSTRLEN);
                if (strcmp(buffer, "::") != 0) 
                {
                    //printf("%s IPv6: %s\n", ifap->ifa_name, buffer);
                    //printf("MAC: %s\n\n", getmac(mac, ifap->ifa_name));
                    macs.insert(getmac(mac, ifap->ifa_name));
                }
            }
            ifap = ifap->ifa_next;
        }
        if (ifap0) 
        { 
            freeifaddrs(ifap0); 
            ifap0 = NULL; 
        }
	}

    std::string hardware::get_machine_id()
    {
        auto macs = get_mac_addresses();
        std::string raw = sys::string_util::join(":", macs);
        hmac_sha1 hmac;
        return hmac.make_string(raw, "B87E9890-5A3B-4B77-B8FA-B1737524D32F");
    }
}

#endif
