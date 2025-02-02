#include "../utils/filewatcher.h"
#include <windows.h>
#include <thread>

void FileWatcher::WatchConfigDirectory(const std::wstring &path, std::function<void()> callback)
{
    changeCallback = callback;
    hDirectory = CreateFileW(
        path.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        nullptr);

    if (hDirectory != INVALID_HANDLE_VALUE)
    {
        std::thread([this]
                    {
            BYTE buffer[1024];
            DWORD bytesReturned;
            
            while(ReadDirectoryChangesW(
                hDirectory,
                buffer,
                sizeof(buffer),
                TRUE,
                FILE_NOTIFY_CHANGE_LAST_WRITE,
                &bytesReturned,
                nullptr,
                nullptr
            )) {
                if(changeCallback) changeCallback();
            } })
            .detach();
    }
}

void FileWatcher::StopWatching()
{
    if (hDirectory != INVALID_HANDLE_VALUE)
    {
        CloseHandle(hDirectory);
        hDirectory = INVALID_HANDLE_VALUE;
    }
}