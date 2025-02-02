// configparser.h
#pragma once
#include <string>
#include <map>
#include <nlohmann/json.hpp>

struct MenuConfig
{
    std::string id;
    std::string label;
    std::string command;
    std::vector<MenuConfig> submenus;
};

class ConfigParser
{
public:
    static std::map<std::string, MenuConfig> menuHierarchy;

    static void LoadFullConfig();
    static const MenuConfig *GetMenuConfig(const std::string &menuId);
    static std::vector<MenuConfig> GetMediaTracks(const char *trackType);

private:
    static void ParseJsonConfig(const std::string &jsonContent);
    static MenuConfig ParseMenuObject(const nlohmann::json &jsonObj);
};