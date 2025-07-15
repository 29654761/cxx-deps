/**
 * @file util.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */

#include "util.h"
#include <iomanip>
#include <random>
#include <sstream>
#include <string.h>
#include <chrono>

#if defined(__linux__) || defined(__APPLE__)
#include <unistd.h>
#include <sys/prctl.h>
#include <pthread.h>
#include <sched.h>
//#include "sys/sysinfo.h"
#elif _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

typedef struct tagTHREADNAME_INFO
{
    DWORD dwType; // must be 0x1000
    LPCSTR szName; // pointer to name (in user addr space)
    DWORD dwThreadID; // thread ID (-1=caller thread)
    DWORD dwFlags; // reserved for future use, must be zero
} THREADNAME_INFO;

#endif

namespace sys {


    std::string util::uuid(uuid_fmt_t fmt)
    {
        static std::random_device rd;
        static std::uniform_int_distribution<uint64_t> dist(0ULL, 0xFFFFFFFFFFFFFFFFULL);
        uint64_t ab = dist(rd);
        uint64_t cd = dist(rd);
        uint32_t a, b, c, d;
        std::stringstream ss;
        ab = (ab & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
        cd = (cd & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;
        a = (ab >> 32U);
        b = (ab & 0xFFFFFFFFU);
        c = (cd >> 32U);
        d = (cd & 0xFFFFFFFFU);
        ss << std::hex << std::nouppercase << std::setfill('0');

        if (fmt == uuid_fmt_t::brace)
            ss << "{";

        ss << std::setw(8) << (a);
        if(fmt== uuid_fmt_t::bar||fmt== uuid_fmt_t::brace)
            ss<< '-';
        
        ss << std::setw(4) << (b >> 16U);
        if (fmt == uuid_fmt_t::bar || fmt == uuid_fmt_t::brace)
            ss << '-';

        ss << std::setw(4) << (b & 0xFFFFU);
        if (fmt == uuid_fmt_t::bar || fmt == uuid_fmt_t::brace)
            ss << '-';

        ss << std::setw(4) << (c >> 16U);
        if (fmt == uuid_fmt_t::bar || fmt == uuid_fmt_t::brace)
            ss << '-';

        ss << std::setw(4) << (c & 0xFFFFU);
        ss << std::setw(8) << d;

        if (fmt == uuid_fmt_t::brace)
            ss << "}";

        return ss.str();
    }

    std::string util::random_string(int len)
    {
        const char* c = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
        size_t cs = strlen(c);
        std::string s;
        s.reserve(len);
        for (size_t i = 0; i < len; i++)
        {
            size_t r=random_number<size_t>(0, cs-1);
            s.append(1,c[r]);
        }
        return s;
    }

    int util::get_cpu_count()
    {
#ifdef __linux__
        return sysconf(_SC_NPROCESSORS_ONLN);
        //GNU way
        //printf("system cpu num is %d\n", get_nprocs_conf());
        //printf("system enable num is %d\n", get_nprocs());
#elif _WIN32
        
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        return sysInfo.dwNumberOfProcessors;
        
#endif
    }

    bool util::set_thread_name(const std::string& name, std::thread* thread)
    {
#ifdef __linux__
        if (thread) 
        {
            int r = pthread_setname_np((pthread_t)thread->native_handle(), name.c_str());
            return r == 0;
        }
        else
        {
            int r = prctl(PR_SET_NAME, name.c_str());
            return r == 0;
        }
#elif _WIN32
        
        THREADNAME_INFO info;
        info.dwType = 0x1000;
        info.szName = name.c_str();
        if(thread){
            info.dwThreadID = GetThreadId(thread->native_handle());
            if (info.dwThreadID == 0)
            {
                return false;
            }
        }
        else {
            info.dwThreadID = -1;
        }

        info.dwFlags = 0;

        __try
        {
            RaiseException(0x406D1388, 0, sizeof(info) / sizeof(DWORD), (const ULONG_PTR*)(DWORD*)&info);
        }
        __except (EXCEPTION_CONTINUE_EXECUTION)
        {
        }
        return true;
#endif
    }

    bool util::set_thread_priority(std::thread* thread, thread_priority_t priority)
    {
        if (!thread||!thread->joinable()) {
            return false; 
        }

#ifdef _WIN32
        int nativePriority = 0;
        switch (priority) {
        case thread_priority_t::low:    nativePriority = THREAD_PRIORITY_BELOW_NORMAL; break;
        case thread_priority_t::normal: nativePriority = THREAD_PRIORITY_NORMAL; break;
        case thread_priority_t::high:   nativePriority = THREAD_PRIORITY_ABOVE_NORMAL; break;
        default:return false;
        }
        return SetThreadPriority(thread->native_handle(), nativePriority) != 0;
#elif defined(__linux__) || defined(__APPLE__)
        sched_param sch;
        int policy;
        if (pthread_getschedparam(thread->native_handle(), &policy, &sch) != 0)
        {
            return false;
        }

        switch (priority) {
        case thread_priority_t::low:    sch.sched_priority = 10; break;
        case thread_priority_t::normal: sch.sched_priority = 50; break;
        case thread_priority_t::high:   sch.sched_priority = 90; break;
        default:return false;
        }
        return pthread_setschedparam(thread->native_handle(), SCHED_FIFO, &sch)==0;
#endif
    }

    double util::cur_time_db()
    {
        auto tp = std::chrono::system_clock::now();
        auto dur = tp.time_since_epoch();
        typedef std::chrono::duration<double, std::ratio<1, 1>> duration_type;
        auto ts = std::chrono::duration_cast<duration_type>(dur);
        return ts.count();
    }

}
