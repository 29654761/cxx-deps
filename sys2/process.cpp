#include "process.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

#ifdef __linux__
extern char** environ;
#endif

namespace sys
{
    process::process()
    {

#ifdef _WIN32
        memset(&pi_, 0, sizeof(pi_));
#elif __linux__
#endif
    }

    process::~process()
    {

    }

    bool process::start(const std::string& cmd, const std::string& args)
    {
        std::vector<std::string> args_vec;
        args_vec.push_back(cmd); // argv[0]

        std::istringstream iss(args);
        std::string token;
        while (iss >> token) {
            args_vec.push_back(token);
        }


        std::vector<char*> argv;
        argv.reserve(args_vec.size() + 1);

        for (auto& s : args_vec) {
            argv.push_back(const_cast<char*>(s.c_str()));
        }
        argv.push_back(nullptr);

#ifdef _WIN32
        return start_win(cmd,args);
#elif __linux__
        return start_linux(cmd,args);
#else
        return false;
#endif
    }

    void process::stop()
    {
#ifdef _WIN32
        stop_win();
#elif __linux__
        stop_linux();
#endif
    }

    int64_t process::run(const std::string& cmd, std::string* output)
    {
#ifdef _WIN32
        return run_win(cmd, output);
#elif __linux__
        return run_linux(cmd, output);
#else
        return -1;
#endif
    }

#ifdef _WIN32
    int64_t process::run_win(const std::string& cmd, std::string* output)
    {
        HANDLE hStdOutRead, hStdOutWrite;
        SECURITY_ATTRIBUTES saAttr;
        saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
        saAttr.bInheritHandle = TRUE;
        saAttr.lpSecurityDescriptor = NULL;

        if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &saAttr, 0)) {
            return -1;
        }

        STARTUPINFO si = { 0 };
        PROCESS_INFORMATION pi = { 0 };

        si.cb = sizeof(STARTUPINFO);
        si.hStdOutput = hStdOutWrite;
        si.hStdError = hStdOutWrite;
        si.dwFlags |= STARTF_USESTDHANDLES;

        if (!CreateProcess(
            NULL,
            (LPSTR)cmd.c_str(),
            NULL,
            NULL,
            TRUE,
            0,
            NULL,
            NULL,
            &si,
            &pi
        )) {
            DWORD err = GetLastError();
            CloseHandle(hStdOutWrite);
            CloseHandle(hStdOutRead);
            return -2;
        }

        // 关闭写管道，因为不需要再写入数据
        CloseHandle(hStdOutWrite);

        // 读取输出
        if (output)
        {
            char buffer[4096];
            DWORD bytesRead;
            while (ReadFile(hStdOutRead, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
                output->append(buffer, bytesRead);
            }
        }

        // 等待进程结束
        WaitForSingleObject(pi.hProcess, INFINITE);

        DWORD dwExitCode = 0;
        // 获取退出码
        if (!GetExitCodeProcess(pi.hProcess, &dwExitCode)) {
            dwExitCode = -3;
        }

        // 关闭进程和线程句柄
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hStdOutRead);

        return dwExitCode;
    }

    bool process::start_win(const std::string& cmd, const std::string& args)
    {
        STARTUPINFO si = { sizeof(si) };
        std::string c = cmd + " " + args;
        return CreateProcessA(
            NULL,
            (char*)c.c_str(),
            NULL, NULL, FALSE,
            0, NULL, NULL,
            &si, &pi_
        );
    }

    void process::stop_win()
    {
        if (pi_.hProcess) 
        {
            TerminateProcess(pi_.hProcess, 0);
            CloseHandle(pi_.hProcess);
        }
        if (pi_.hThread)
        {
            CloseHandle(pi_.hThread);
        }
		memset(&pi_, 0, sizeof(pi_));

    }
#elif __linux__

    int64_t process::run_linux(const std::string& cmd, std::string* output)
    {
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            return -1;
        }

        if (output) {
            char buffer[4096] = {};
            while (fgets(buffer, sizeof(buffer), pipe)) {
                (*output) += buffer;
            }
        }

        int status = pclose(pipe);

        if (status == -1) {
            return -2;
        }

        int exitCode = WEXITSTATUS(status);

        return exitCode;
    }

    bool process::start_linux(const std::string& cmd, const std::string& args)
    {
        std::vector<std::string> args_vec;
        args_vec.push_back(cmd); // argv[0]

        std::istringstream iss(args);
        std::string token;
        while (iss >> token) {
            args_vec.push_back(token);
        }


        std::vector<char*> argv;
        argv.reserve(args_vec.size() + 1);

        for (auto& s : args_vec) {
            argv.push_back(const_cast<char*>(s.c_str()));
        }
        argv.push_back(nullptr);


        int ret = posix_spawnp(
            &pid_,
            cmd.c_str(),
            nullptr,   // file_actions
            nullptr,   // attr
            argv.data(),
            environ    // 继承环境变量
        );

        return ret==0;
    }

    void process::stop_linux()
    {
        if (pid_ >= 0) 
        {
            kill(pid_, SIGTERM);
        }
    }

#endif
}