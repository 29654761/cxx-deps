/**
 * @file util.h
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */

#pragma once

#include <string>
#include <random>
#include <thread>

namespace sys{

    enum class uuid_fmt_t
    {
        empty,
        bar,
        brace,
    };

    enum class thread_priority_t {
        low,
        normal,
        high
    };

    class util
    {
    public:

        /// <summary>
    /// Make a uuid string
    /// </summary>
    /// <param name="fmt">
    /// empty: xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    /// bar: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
    /// brace: {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}
    /// </param>
    /// <returns></returns>
        static std::string uuid(uuid_fmt_t fmt= uuid_fmt_t::empty);

        template<class TNumber>
        static TNumber random_number(TNumber min,TNumber max)
        {
            std::random_device rd;
            std::uniform_int_distribution<TNumber> dist(min, max);
            return dist(rd);
        }

    
        static std::string random_string(int len);

        static int get_cpu_count();

        static bool set_thread_name(const std::string& name, std::thread* thread);

        static bool set_thread_priority(std::thread* thread, thread_priority_t priority);

        static double cur_time_db();

        static int64_t cur_time_ms()
        {
            return (int64_t)(cur_time_db() * 1000);
        }
    };


}


