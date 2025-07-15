#pragma once

#ifdef _WIN32
#define SPDLOG_WCHAR_TO_UTF8_SUPPORT
#endif // _WIN32

#define SPDLOG_HEADER_ONLY
#define SPDLOG_NO_THREAD_ID
#define SPDLOG_NO_DATETIME
#define SPDLOG_NO_NAME
#define SPDLOG_NO_EXCEPTIONS

#ifndef _NO_SPDLOG_
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#ifdef __ANDROID__
#include <spdlog/sinks/android_sink.h>
#endif //__ANDROID__
#else
#include <memory>
//#include <spdlog/fmt/fmt.h>
#endif

#ifdef _NO_SPDLOG_
#define SPDLOG_LEVEL_TRACE 0
#define SPDLOG_LEVEL_DEBUG 1
#define SPDLOG_LEVEL_INFO 2
#define SPDLOG_LEVEL_WARN 3
#define SPDLOG_LEVEL_ERROR 4
#define SPDLOG_LEVEL_CRITICAL 5
#define SPDLOG_LEVEL_OFF 6
#endif // !_NO_SPDLOG_

enum class log_level_t
{
    trace = SPDLOG_LEVEL_TRACE,
    debug = SPDLOG_LEVEL_DEBUG,
    info = SPDLOG_LEVEL_INFO,
    warn = SPDLOG_LEVEL_WARN,
    err = SPDLOG_LEVEL_ERROR,
    critical = SPDLOG_LEVEL_CRITICAL,
    off = SPDLOG_LEVEL_OFF,
    n_levels
};

class spdlogger
{
public:
#ifndef _NO_SPDLOG_
    typedef std::shared_ptr<spdlog::logger> Logger;
#endif

    spdlogger()
    {
    }

    ~spdlogger()
    {
        close();
    }

    bool init(const char* name, const char* file, log_level_t level)
    {
        level_ = level;

#ifndef _NO_SPDLOG_
        if (file) {
            auto rotating = spdlog::rotating_logger_mt(name, file, 1024 * 1024 * 10, 10);
            rotating->set_level((spdlog::level::level_enum)level);
            rotating->set_pattern("[%C/%m/%d %H:%M:%S.%e][%^%l%$] %v");
            //rotating->flush_on(spdlog::level::level_enum::trace);
            loggers_.push_back(rotating);
        }
        
        auto console = spdlog::stdout_color_mt(std::string(name) + "-console");
        console->set_level((spdlog::level::level_enum)level);
        console->set_pattern("[%C/%m/%d %H:%M:%S.%e][%^%l%$] %v");
        
        loggers_.push_back(console);

#ifdef __ANDROID__
        auto android = spdlog::android_logger_mt(std::string(name) + "-android", name);
        android->set_level(spdlog::level::level_enum::trace);
        android->set_pattern("[%C/%m/%d %H:%M:%S.%e][%^%l%$] %v");
        loggers_.push_back(android);
#endif // __ANDROID__

#endif // _NO_SPDLOG_
        return true;
    }

    void close()
    {
#ifndef _NO_SPDLOG_
        loggers_.clear();
        spdlog::shutdown();
#endif // _NO_SPDLOG_
    }

    void set_level(log_level_t level)
    {
        level_ = level;
#ifndef _NO_SPDLOG_
        for (auto iter = loggers_.begin(); iter != loggers_.end(); iter++) {
            (*iter)->set_level((spdlog::level::level_enum)level);
        }
#endif // _NO_SPDLOG_
    }

    log_level_t level()const
    {
        return level_;
    }

    void begin(size_t num) 
    {
#ifndef _NO_SPDLOG_
        for (auto iter = loggers_.begin(); iter != loggers_.end(); iter++) {
            (*iter)->enable_backtrace(num);
        }
#endif // _NO_SPDLOG_
    }

    void commit()
    {
#ifndef _NO_SPDLOG_
        for (auto iter = loggers_.begin(); iter != loggers_.end(); iter++) {
            (*iter)->dump_backtrace();
        }
#endif // _NO_SPDLOG_
    }

    spdlogger* flush() {
#ifndef _NO_SPDLOG_
        for (auto iter = loggers_.begin(); iter != loggers_.end(); iter++) {
            (*iter)->flush();
        }
#endif // _NO_SPDLOG_
        return this;
    }

    template<typename FormatString, typename... Args>
    spdlogger* trace(const FormatString& fmt, Args&&...args)
    {
#ifndef _NO_SPDLOG_
        for (auto iter = loggers_.begin(); iter != loggers_.end(); iter++) {
            (*iter)->log(spdlog::level::trace, fmt, std::forward<Args>(args)...);
        }
#else
   //     if (level_ <= log_level_t::trace)
   //     {
			//std::string s=fmt::format(fmt, std::forward<Args>(args)...);
			//printf("[TRACE] %s\n", s.c_str());
   //     }
#endif // _NO_SPDLOG_
        return this;
    }

    template<typename FormatString, typename... Args>
    spdlogger* debug(const FormatString& fmt, Args&&...args)
    {
#ifndef _NO_SPDLOG_
        for (auto iter = loggers_.begin(); iter != loggers_.end(); iter++) {
            (*iter)->log(spdlog::level::debug, fmt, std::forward<Args>(args)...);
        }
#endif // _NO_SPDLOG_
        return this;
    }

    template<typename FormatString, typename... Args>
    spdlogger* info(const FormatString& fmt, Args&&...args)
    {
#ifndef _NO_SPDLOG_
        for (auto iter = loggers_.begin(); iter != loggers_.end(); iter++) {
            (*iter)->log(spdlog::level::info, fmt, std::forward<Args>(args)...);
        }
#endif // _NO_SPDLOG_
        return this;
    }

    template<typename FormatString, typename... Args>
    spdlogger* warn(const FormatString& fmt, Args&&...args)
    {
#ifndef _NO_SPDLOG_
        for (auto iter = loggers_.begin(); iter != loggers_.end(); iter++) {
            (*iter)->log(spdlog::level::warn, fmt, std::forward<Args>(args)...);
        }
#endif // _NO_SPDLOG_
        return this;
    }

    template<typename FormatString, typename... Args>
    spdlogger* error(const FormatString& fmt, Args&&...args)
    {
#ifndef _NO_SPDLOG_
        for (auto iter = loggers_.begin(); iter != loggers_.end(); iter++) {
            (*iter)->log(spdlog::level::err, fmt, std::forward<Args>(args)...);
        }
#endif // _NO_SPDLOG_
        return this;
    }

    template<typename FormatString, typename... Args>
    spdlogger* critical(const FormatString& fmt, Args&&...args)
    {

#ifndef _NO_SPDLOG_
        for (auto iter = loggers_.begin(); iter != loggers_.end(); iter++) {
            (*iter)->log(spdlog::level::critical, fmt, std::forward<Args>(args)...);
        }
#endif // _NO_SPDLOG_
        return this;
    }



    template<typename T>
    spdlogger* trace(const T& msg)
    {
#ifndef _NO_SPDLOG_
        for (auto iter = loggers_.begin(); iter != loggers_.end(); iter++) {
            (*iter)->log(spdlog::level::trace, msg);
        }
#endif // _NO_SPDLOG_
        return this;
    }

    template<typename T>
    spdlogger* debug(const T& msg)
    {
#ifndef _NO_SPDLOG_
        for (auto iter = loggers_.begin(); iter != loggers_.end(); iter++) {
            (*iter)->log(spdlog::level::debug, msg);
        }
#endif // _NO_SPDLOG_
        return this;
    }

    template<typename T>
    spdlogger* info(const T& msg)
    {
#ifndef _NO_SPDLOG_
        for (auto iter = loggers_.begin(); iter != loggers_.end(); iter++) {
            (*iter)->log(spdlog::level::info, msg);
        }
#endif // _NO_SPDLOG_
        return this;
    }

    template<typename T>
    spdlogger* warn(const T& msg)
    {
#ifndef _NO_SPDLOG_
        for (auto iter = loggers_.begin(); iter != loggers_.end(); iter++) {
            (*iter)->log(spdlog::level::warn, msg);
        }
#endif // _NO_SPDLOG_
        return this;
    }

    template<typename T>
    spdlogger* error(const T& msg)
    {
#ifndef _NO_SPDLOG_
        for (auto iter = loggers_.begin(); iter != loggers_.end(); iter++) {
            (*iter)->log(spdlog::level::err, msg);
        }
#endif // _NO_SPDLOG_
        return this;
    }

    template<typename T>
    spdlogger* critical(const T& msg)
    {
#ifndef _NO_SPDLOG_
        for (auto iter = loggers_.begin(); iter != loggers_.end(); iter++) {
            (*iter)->log(spdlog::level::critical, msg);
        }
#endif // _NO_SPDLOG_
        return this;
    }

private:
#ifndef _NO_SPDLOG_
	std::vector<Logger> loggers_;
#endif // _NO_SPDLOG_

    log_level_t level_= log_level_t::debug;
};


typedef std::shared_ptr<spdlogger> spdlogger_ptr;


