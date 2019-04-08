#pragma once
#include "compiledCode.hpp"
#include <filesystem>
#include <memory>
#include <unordered_map>
#include "virtualmachine.h"
#include "optimizer/optimizerModuleBase.hpp"

struct astnode;

class ScriptCompiler {
public:
    ScriptCompiler(const std::vector<std::filesystem::path>& includePaths);


    CompiledCodeData compileScript(std::filesystem::path file);
private:
    struct CompileTempData {
        std::unordered_map<std::string, uint8_t> fileLoc;
    };

    void ASTToInstructions(CompiledCodeData& output, CompileTempData& temp, std::vector<ScriptInstruction>& instructions, const astnode& node) const;


    ScriptConstantArray ASTParseArray(CompiledCodeData& output, CompileTempData& temp, const OptimizerModuleBase::Node& node) const;
    void ASTToInstructions(CompiledCodeData& output, CompileTempData& temp, std::vector<ScriptInstruction>& instructions, const OptimizerModuleBase::Node& node) const;
    void initIncludePaths(const std::vector<std::filesystem::path>&) const;




    std::unique_ptr<sqf::virtualmachine> vm;
};
