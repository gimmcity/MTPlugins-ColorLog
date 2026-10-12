#define MODTOOLS_PLUGIN_IMPL
#include "modtools_plugin.hpp"
#include "logcolors.h"

#include <string>

namespace
{
    std::wstring g_ini;

    int OnLoad()
    {
        if (rp::App() != MODTOOLS_APP_LAUNCHER) return 0;
        g_ini = rp::PluginDir() + L"\\ColorLog.ini";
        logcolors::Setup(g_ini);
        rp::Log("Color Log standalone plugin loaded; LauncherPlus is not modified.");
        return 1;
    }

    void OnTick()
    {
        logcolors::Tick();
    }

    void OnUnload()
    {
        logcolors::Shutdown();
    }

    const char* OnCommand(const char* text)
    {
        static std::string reply;
        reply.clear();
        const std::string command = text ? text : "";
        return logcolors::Command(command, reply) ? reply.c_str() : nullptr;
    }
}

extern "C" MODTOOLS_PLUGIN_EXPORT const ModToolsPluginApi* ModToolsPluginEntry(ModToolsPlugin* self, const ModToolsHostApi* host)
{
    rp::Bind(self, host);
    static ModToolsPluginApi api = {};
    api.size = sizeof api;
    api.apiVersion = MODTOOLS_PLUGIN_API_VERSION;
    api.name = "ColorLog";
    api.displayName = "Color Log";
    api.version = "1.0.1";
    api.author = "GimmCity";
    api.description = "Colors warnings and errors, counts them, and navigates diagnostics in the stock BO3 Mod Tools Launcher console.";
    api.url = "https://github.com/gimmcity/MTPlugins-ColorLog";
    api.OnLoad = OnLoad;
    api.OnUnload = OnUnload;
    api.OnTick = OnTick;
    api.OnCommand = OnCommand;
    api.apps = MODTOOLS_APP_LAUNCHER;
    return &api;
}
