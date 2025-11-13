#include "server.h"
#include <windows.h>
#include <shlobj.h>
#include <string>
#include <thread>
#include <atomic>
#include "../core/globals.h"
#include "../ui/mainwindow.h"
#include "../utils/crashlog.h"
#include "../utils/helpers.h"
#include "../logger/logger.h"


bool StartNodeServer()
{
    std::wstring exeDir = GetExeDirectory();
    std::wstring exePath = exeDir + L"\\stremio-runtime.exe";
    std::wstring scriptPath = exeDir + L"\\server.js";

    if (!FileExists(exePath) || !FileExists(scriptPath)) {
        wchar_t localAppData[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, localAppData))) {
            std::wstring altDir = std::wstring(localAppData) + L"\\Programs\\StremioService";
            std::wstring altExePath = altDir + L"\\stremio-runtime.exe";
            std::wstring altScriptPath = altDir + L"\\server.js";

            if (FileExists(altExePath) && FileExists(altScriptPath)) {
                exeDir = altDir;
                exePath = altExePath;
                scriptPath = altScriptPath;
                LOG_INFO("StartNodeServer", "Found node runtime in %localappdata%");
            } else {
                AppendToCrashLog(L"[NODE]: Missing stremio-runtime.exe and server.js in both exeDir and localappdata.");
                return false;
            }
        } else {
            AppendToCrashLog(L"[NODE]: Failed to retrieve local app data path.");
            return false;
        }
    }

    if (!g_serverJob) {
        g_serverJob = CreateJobObject(nullptr, nullptr);
        if(!g_serverJob){
            AppendToCrashLog(L"[NODE]: Failed to create Job Object.");
            return false;
        }
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobInfo = {0};
        jobInfo.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(g_serverJob, JobObjectExtendedLimitInformation, &jobInfo, sizeof(jobInfo));
    }

    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));
    
    std::wstring cmdLine = L"\"" + exePath + L"\" \"" + scriptPath + L"\"";

    SetEnvironmentVariableW(L"NO_CORS", L"1");

    // Launch the process without I/O redirection.
    // Set the working directory  to the runtime's location.
    BOOL success = CreateProcessW(
        nullptr,         // Application Name
        &cmdLine[0],     // Command line (mutable)
        nullptr,         // Process handle not inheritable
        nullptr,         // Thread handle not inheritable
        FALSE,           // Set handle inheritance to FALSE
        CREATE_NO_WINDOW,// Creation flags: run headlessly
        nullptr,         // Use parent's environment block
        exeDir.c_str(),  // Set the working directory
        &si,             // Pointer to STARTUPINFO structure
        &pi              // Pointer to PROCESS_INFORMATION structure
    );

    if (!success) {
        std::wstring err = L"Failed to launch stremio-runtime.exe. GetLastError=" + std::to_wstring(GetLastError());
        AppendToCrashLog(err);
        return false;
    }

    AssignProcessToJobObject(g_serverJob, pi.hProcess);

    g_nodeProcess = pi.hProcess;
    CloseHandle(pi.hThread);

    g_nodeRunning = true;
    
    LOG_INFO("StartNodeServer", "Node server process launched successfully.");

    nlohmann::json j;
    j["type"] ="ServerStarted";
    g_outboundMessages.push_back(j);
    PostMessage(g_hWnd, WM_NOTIFY_FLUSH, 0, 0);

    return true;
}

void StopNodeServer()
{
    if (g_nodeRunning) {
        g_nodeRunning = false;
        if (g_nodeProcess) {
            CloseHandle(g_nodeProcess);
            g_nodeProcess = nullptr;
        }        
        LOG_INFO("StopNodeServer", "Node server stopped.");
    }
}