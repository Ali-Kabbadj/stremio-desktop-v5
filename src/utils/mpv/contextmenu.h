#pragma once
#include "configparser.h"
#include <windows.h>
#include <functional>
#include <string>
#include "../../core/globals.h"
#include <map>

class ContextMenuManager
{
public:
    using CommandHandler = std::function<void(const std::string &)>;

    static ContextMenuManager &Get();
    void RefreshConfig();
    bool HandleCommand(int commandId); // Moved to public
    std::unordered_map<int, std::string> commandMap;
    void SetCommandHandler(CommandHandler handler) { commandHandler = std::move(handler); }
    int GetNextCommandId() { return currentId++; }
    void RegisterCommand(int id, const std::string &command) { commandMap[id] = command; }

private:
    HMENU currentMenu = nullptr;
    int currentId = 1;
    CommandHandler commandHandler;
};