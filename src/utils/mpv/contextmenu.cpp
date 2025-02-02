#include "contextmenu.h"
#include "../../utils/helpers.h"
#include <string>

ContextMenuManager &ContextMenuManager::Get()
{
    static ContextMenuManager instance;
    return instance;
}

bool ContextMenuManager::HandleCommand(int commandId)
{

    if (commandHandler)
    {
        commandHandler(commandMap[commandId]);
        return true;
    }

    return false;
}

void ContextMenuManager::RefreshConfig()
{
    ConfigParser::LoadFullConfig();
}