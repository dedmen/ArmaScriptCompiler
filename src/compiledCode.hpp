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

#ifndef ASC_INTERCEPT
#define STRINGTYPE std::string
#else
#define STRINGTYPE intercept::types::r_string
#include <intercept.hpp>
#endif



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
    std::variant<STRINGTYPE,uint64_t> content;
};

enum class ConstantType {
    code,
    string,
    scalar,
    boolean
};

struct ScriptCodePiece {
    std::vector<ScriptInstruction> code;
    uint64_t contentString; //pointer to constants
};


using ScriptConstant = std::variant<ScriptCodePiece, STRINGTYPE, float, bool>;

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
    uint32_t version{1};
    std::vector<ScriptInstruction> instructions;
    std::vector<ScriptConstant> constants;
    std::vector<STRINGTYPE> fileNames;

#ifdef ASC_INTERCEPT
    std::vector<game_value> builtConstants;
#endif


    //#TODO compress constants, don't have duplicates for a number or string
};