#pragma once
#include <filesystem>
#include <mutex>

#include "sol/sol.hpp"

class ScriptCompiler;

class LuaHandler {
    sol::state lua;
    // Was a lua script loaded from file?
    bool isActive = false;
    std::mutex luaAccess;
public:

    LuaHandler();
    void LoadFromFile(std::filesystem::path filePath);

    void SetupCompiler(ScriptCompiler& compiler);

};

inline LuaHandler GLuaHandler;
