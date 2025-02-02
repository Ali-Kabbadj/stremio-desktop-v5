#include "configparser.h"
#include "../helpers.h"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <mpv/client.h>
#include <vector>
#include "../../core/globals.h"

using json = nlohmann::json;
std::map<std::string, MenuConfig> ConfigParser::menuHierarchy;

void ConfigParser::LoadFullConfig()
{
    menuHierarchy.clear();

    std::wstring exeDir = GetExeDirectory();
    std::wstring jsonPath = exeDir + L"\\portable_config\\script-opts\\menu.json";
    std::wstring oldConfPath = exeDir + L"\\portable_config\\script-opts\\menu.conf";

    try
    {
        if (std::filesystem::exists(jsonPath))
        {
            // New JSON format
            std::ifstream file(jsonPath);
            std::string jsonContent((std::istreambuf_iterator<char>(file)),
                                    std::istreambuf_iterator<char>());
            ParseJsonConfig(jsonContent);
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "[Config Parser] Error loading config: " << e.what() << std::endl;
    }
}

void ConfigParser::ParseJsonConfig(const std::string &jsonContent)
{
    try
    {
        json j = json::parse(jsonContent);

        // First parse menu bindings
        if (j.contains("menu_bindings"))
        {
            for (const auto &item : j["menu_bindings"])
            {
                MenuConfig config;
                config.id = item.value("id", "");
                config.label = item["label"].get<std::string>();
                config.command = item["command"].get<std::string>();

                // Store in hierarchy for reference
                menuHierarchy[config.id] = config;
            }
        }

        if (j.contains("menus"))
        {
            for (auto &[menuId, menuObj] : j["menus"].items())
            {
                MenuConfig menu;
                menu.id = menuId;
                menu.label = menuObj.value("label", menuId);

                if (menuObj.contains("submenus"))
                {
                    for (const auto &subId : menuObj["submenus"])
                    {
                        // Check if submenu exists in "menus"
                        if (j["menus"].contains(subId))
                        {
                            // Recursively parse submenu
                            menu.submenus.push_back(ParseMenuObject(j["menus"][subId]));
                        }
                        // Else, treat as command (existing logic)
                        else if (menuHierarchy.count(subId))
                        {
                            menu.submenus.push_back(menuHierarchy[subId]);
                        }
                    }
                }
                menuHierarchy[menuId] = menu;
            }
        }

        // Debug output
        std::cout << "Root menu subitems: ";
        if (menuHierarchy.count("root"))
        {
            for (const auto &item : menuHierarchy["root"].submenus)
            {
                std::cout << item.id << " ";
            }
        }
        std::cout << std::endl;
    }
    catch (const json::exception &e)
    {
        std::cerr << "[Config Parser] JSON parse error: " << e.what() << std::endl;
    }
}

MenuConfig ConfigParser::ParseMenuObject(const json &jsonObj)
{
    MenuConfig config;

    try
    {
        config.id = jsonObj.value("id", "");
        config.label = jsonObj.value("label", "");
        config.command = jsonObj.value("command", "");

        if (jsonObj.contains("submenus"))
        {
            for (const auto &submenu : jsonObj["submenus"])
            {
                if (submenu.is_string())
                {
                    std::string refId = submenu.get<std::string>();
                    if (menuHierarchy.count(refId))
                    {
                        config.submenus.push_back(menuHierarchy[refId]);
                    }
                }
                else
                {
                    config.submenus.push_back(ParseMenuObject(submenu));
                }
            }
        }
    }
    catch (const json::exception &e)
    {
        std::cerr << "Error parsing menu object: " << e.what() << std::endl;
    }

    return config;
}

std::vector<MenuConfig> ConfigParser::GetMediaTracks(const char *trackType)
{
    std::vector<MenuConfig> tracks;
    mpv_handle *ctx = g_mpv;

    // Add "Disable" option first
    char disableCommand[256];
    snprintf(disableCommand, sizeof(disableCommand), "no-osd set %s no", trackType);
    tracks.push_back(MenuConfig{
        std::string(trackType) + "_disabled",
        "Disable",
        disableCommand});

    // Get track list property
    mpv_node node;
    if (mpv_get_property(ctx, "track-list", MPV_FORMAT_NODE, &node) < 0)
    {
        std::cerr << "Failed to get track list" << std::endl;
        return tracks;
    }

    if (node.format != MPV_FORMAT_NODE_ARRAY)
    {
        std::cerr << "Track list has unexpected format" << std::endl;
        return tracks;
    }

    // Iterate through tracks
    for (int i = 0; i < node.u.list->num; i++)
    {
        mpv_node *item = &node.u.list->values[i];
        if (item->format != MPV_FORMAT_NODE_MAP)
            continue;

        std::string type;
        int id = -1;
        std::string title;
        std::string lang;
        bool selected = false;

        // Parse track properties
        for (int n = 0; n < item->u.list->num; n++)
        {
            std::string key = item->u.list->keys[n];
            mpv_node *val = &item->u.list->values[n];

            if (key == "type")
            {
                if (val->format == MPV_FORMAT_STRING)
                    type = val->u.string;
            }
            else if (key == "id")
            {
                if (val->format == MPV_FORMAT_INT64)
                    id = val->u.int64;
            }
            else if (key == "title")
            {
                if (val->format == MPV_FORMAT_STRING)
                    title = val->u.string;
            }
            else if (key == "lang")
            {
                if (val->format == MPV_FORMAT_STRING)
                    lang = val->u.string;
            }
            else if (key == "selected")
            {
                if (val->format == MPV_FORMAT_FLAG)
                    selected = val->u.flag;
            }
        }

        // Filter by track type and create menu items
        if (type == trackType)
        {
            MenuConfig track;
            track.id = std::string(trackType) + "_" + std::to_string(id);
            track.label = title.empty() ? (lang.empty() ? track.id : lang) : title;

            // Create command based on track type
            if (strcmp(trackType, "audio") == 0)
            {
                track.command = "set audio-track " + std::to_string(id);
            }
            else if (strcmp(trackType, "sub") == 0)
            {
                track.command = "set sub-track " + std::to_string(id);
            }
            else if (strcmp(trackType, "video") == 0)
            {
                track.command = "set vid-track " + std::to_string(id);
            }

            // Add selection indicator
            if (selected)
                track.label += " ✓";

            tracks.push_back(track);
        }
    }

    // Cleanup mpv node
    mpv_free_node_contents(&node);

    return tracks;
}

const MenuConfig *ConfigParser::GetMenuConfig(const std::string &menuId)
{
    // First check regular config
    auto it = menuHierarchy.find(menuId);
    if (it != menuHierarchy.end())
    {
        // Handle dynamic menus
        if (menuId == "audio_tracks" || menuId == "subtitle_tracks")
        {
            // Clone the config to avoid modifying cached version
            static thread_local MenuConfig dynamicConfig;
            dynamicConfig = it->second; // Copy base config from JSON
            dynamicConfig.submenus = GetMediaTracks(menuId == "audio_tracks" ? "audio" : "sub");
            return &dynamicConfig;
        }
        return &it->second;
    }

    // Legacy fallback (should eventually be removed)
    if (menuId == "audio_tracks" || menuId == "subtitle_tracks")
    {
        static thread_local MenuConfig fallbackConfig;
        fallbackConfig.id = menuId;
        fallbackConfig.label = menuId == "audio_tracks" ? "Audio Tracks" : "Subtitles";
        fallbackConfig.submenus = GetMediaTracks(menuId == "audio_tracks" ? "audio" : "sub");
        return &fallbackConfig;
    }

    std::wcout << L"[Config Parser] Menu ID not found: " << Utf8ToWstring(menuId) << std::endl;
    return nullptr;
}
