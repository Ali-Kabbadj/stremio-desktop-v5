#pragma once
#include <functional>
#include <windows.h>
#include <string>

class FileWatcher
{
public:
    void WatchConfigDirectory(const std::wstring &path, std::function<void()> callback);
    void StopWatching();

private:
    HANDLE hDirectory = INVALID_HANDLE_VALUE;
    std::function<void()> changeCallback;

    static DWORD WINAPI WatchThread(LPVOID lpParam);
};