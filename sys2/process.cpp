#include "process.h"
#include <iostream>
#include <fstream>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#elif __linux__
#endif

namespace sys
{
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

#endif
}