#pragma once
#include "compiledCode.hpp"
#include <filesystem>
#include <memory>
#include <unordered_map>
#include "optimizer/optimizerModuleBase.hpp"


#include <runtime/runtime.h>
#include <runtime/parser/preprocessor.h>
#include <runtime/parser/sqf.h>
#include <parser/sqf/sqf_parser.hpp>
#include <src\optimizer\optimizerModuleLua.hpp>
using astnode = sqf::parser::sqf::bison::astnode;

class ScriptCompiler {
    void init();
public:
    ScriptCompiler(const std::vector<std::filesystem::path>& includePaths);
    ScriptCompiler();


    CompiledCodeData compileScript(std::filesystem::path physicalPath, std::filesystem::path virtualPath);

    CompiledCodeData compileScriptLua(std::filesystem::path physicalPath, std::filesystem::path virtualPath, OptimizerModuleLua& optimizer, const std::filesystem::path& outputFile);
    void initIncludePaths(const std::vector<std::filesystem::path>&);
    void addMacro(sqf::runtime::parser::macro macro);
    void addPragma(sqf::runtime::parser::pragma pragma);
    std::string preprocessScript(std::filesystem::path physicalPath, std::filesystem::path virtualPath);

private:
    struct CompileTempData {
        std::unordered_map<std::string, uint8_t> fileLoc;
    };

    void ASTToInstructions(CompiledCodeData& output, CompileTempData& temp, std::vector<ScriptInstruction>& instructions, const astnode& node) const;


    ScriptConstantArray ASTParseArray(CompiledCodeData& output, CompileTempData& temp, const OptimizerModuleBase::Node& node) const;
    void ASTToInstructions(CompiledCodeData& output, CompileTempData& temp, std::vector<ScriptInstruction>& instructions, const OptimizerModuleBase::Node& node) const;
    std::unique_ptr<sqf::runtime::runtime> vm;
    bool ignoreCurrentFile = false;
};
