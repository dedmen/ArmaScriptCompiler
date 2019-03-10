#pragma once
#include <vector>
#include <string>
#include <variant>
#include <string_view>
using namespace std::string_view_literals;

/*
endStatement
push <type> <value>
callUnary <command>
callBinary <command>
assignTo <name>
assignToLocal <name>
callNular <name>
getVariable <name>
makeArray <size>
*/

enum class InstructionType {
    endStatement,
    push,
    callUnary,
    callBinary,
    callNular,
    assignTo,
    assignToLocal,
    getVariable,
    makeArray
};

struct ScriptInstruction {
    InstructionType type;
    size_t offset;
    uint8_t fileIndex;
    size_t line;
    //content string, or constant index
    std::variant<std::string,uint64_t> content;
};

enum class ConstantType {
    code,
    string,
    scalar,
    boolean
};

using ScriptConstant = std::variant<std::vector<ScriptInstruction>, std::string, float, bool>;

constexpr ConstantType getConstantType(const ScriptConstant& c) {
    switch (c.index()) {
        case 0: return ConstantType::code;
        case 1: return ConstantType::string;
        case 2: return ConstantType::scalar;
        case 3: return ConstantType::boolean;
    }
    __debugbreak();
}


struct CompiledCodeData {
    std::vector<ScriptInstruction> instructions;
    std::vector<ScriptConstant> constants;
    std::vector<std::string> fileNames;
    //#TODO compress constants, don't have duplicates for a number or string
};