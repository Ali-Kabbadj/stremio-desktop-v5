#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include "../core/globals.h"
#include "crashlog.h"
#include <discord-rpc.hpp>

const char* DISCORD_CLIENT_ID = "1438324305581834453";

void InitializeDiscord()
{
    auto& manager = discord::RPCManager::get();

    manager.onReady([](discord::User const& user) {
        std::cout << "[DISCORD]: Connected to Discord user: " << user.username << std::endl;
    });

    manager.onDisconnected([](int errorCode, std::string_view message) {
        std::cout << "[DISCORD]: Disconnected (" << errorCode << "): " << std::string(message) << std::endl;
    });

    manager.onErrored([](int errorCode, std::string_view message) {
        std::string errorMsg = "[DISCORD]: Error (" + std::to_string(errorCode) + "): " + std::string(message);
        std::cout << errorMsg << std::endl;
        AppendToCrashLog(errorMsg);
    });

    manager.setClientID(DISCORD_CLIENT_ID);
    manager.initialize();
}

static void SetDiscordWatchingPresence(const std::vector<std::string>& args) {
    auto& presence = discord::RPCManager::get().getPresence();
    presence.clear();

    presence.setActivityType(discord::ActivityType::Watching);
    
    presence.setDetails("Watching " + args[2]);
    presence.setLargeImageKey(args[7]);
    presence.setLargeImageText(args[2]);

    bool isPaused = (args.size() > 10 && !args[10].empty() && args[10] == "yes");

    if (isPaused) {
        presence.setState("Paused");
        presence.setStartTimestamp(0);
        presence.setEndTimestamp(0);
    } else {
        std::time_t currentTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        int elapsedSeconds = std::stoi(args[8]);
        int durationSeconds = std::stoi(args[9]);

        presence.setStartTimestamp(currentTime - elapsedSeconds);
        presence.setEndTimestamp(currentTime + (durationSeconds - elapsedSeconds));
        
        if (args[1] == "series") {
            std::string state = args[5] + " (S" + args[3] + "-E" + args[4] + ")";
            presence.setState(state);
            if (args.size() > 6 && !args[6].empty()) {
                presence.setSmallImageKey(args[6]);
                presence.setSmallImageText(args[5]);
            }
        } else {
            presence.setState("Playing");
        }
    }

    if (args.size() > 11 && !args[11].empty()) {
        presence.setButton1("More Details", args[11]);
    }

    if (args.size() > 12 && !args[12].empty()) {
        presence.setButton2("Watch on Stremio", args[12]);
    }

    presence.refresh();
}

static void SetDiscordViewingPresence(const std::vector<std::string>& args) {
    auto& presence = discord::RPCManager::get().getPresence();
    presence.clear();

    presence.setActivityType(discord::ActivityType::Watching);
    
    presence.setDetails("Viewing " + args[2]);
    presence.setState("Browsing Details");
    presence.setLargeImageKey(args[3]);
    presence.setLargeImageText(args[2]);

    
    presence.refresh();
}

static void SetDiscordBrowsingPresence(const std::string& details, const std::string& state, const std::string& iconKey) {
    auto& presence = discord::RPCManager::get().getPresence();
    presence.clear();
    
    presence.setActivityType(discord::ActivityType::Watching);
    presence.setDetails(details);
    presence.setState(state);
    
    presence.setLargeImageKey(iconKey);
    presence.setLargeImageText(state);
    
    presence.refresh();
}

void SetDiscordPresenceFromArgs(const std::vector<std::string>& args) {
    if (!g_isRpcOn || args.empty()) {
        return;
    }

    const std::string& activityType = args[0];
    
    if (activityType == "watching" && args.size() >= 11) {
        SetDiscordWatchingPresence(args);
    } else if (activityType == "meta-detail" && args.size() >= 4) {
        SetDiscordViewingPresence(args);
    } else if (activityType == "board") {
        SetDiscordBrowsingPresence("Resuming Favorites", "On Board", "icon-board");
    } else if (activityType == "discover") {
        SetDiscordBrowsingPresence("Finding New Gems", "In Discover", "icon-discover");
    } else if (activityType == "library") {
        SetDiscordBrowsingPresence("Revisiting Old Favorites", "In Library", "icon-library");
    } else if (activityType == "calendar") {
        SetDiscordBrowsingPresence("Planning My Next Binge", "On Calendar", "icon-calendar");
    } else if (activityType == "addons") {
        SetDiscordBrowsingPresence("Exploring Add-ons", "In Add-ons", "icon-addons");
    } else if (activityType == "settings") {
        SetDiscordBrowsingPresence("Tuning Preferences", "In Settings", "icon-settings");
    } else if (activityType == "search") {
        SetDiscordBrowsingPresence("Searching for Shows & Movies", "In Search", "icon-search");
    } else if (activityType == "clear") {
        discord::RPCManager::get().clearPresence();
    }
}