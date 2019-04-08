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


static std::string_view instructionTypeToString(InstructionType type) {
    switch (type) {
        case InstructionType::endStatement: return "endStatement"sv;
        case InstructionType::push: return "push"sv;
        case InstructionType::callUnary: return "callUnary"sv;
        case InstructionType::callBinary: return "callBinary"sv;
        case InstructionType::callNular: return "callNular"sv;
        case InstructionType::assignTo: return "assignTo"sv;
        case InstructionType::assignToLocal: return "assignToLocal"sv;
        case InstructionType::getVariable: return "getVariable"sv;
        case InstructionType::makeArray: return "makeArray"sv;
        default: __debugbreak();
    }
}


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
    boolean,
    array
};

struct ScriptCodePiece {
    std::vector<ScriptInstruction> code;
    union {
        struct {
            unsigned isOffset : 1;
            unsigned length : 31;
            unsigned offset : 32;
        } contentSplit;
        uint64_t contentString; //pointer to constants
    };
    ScriptCodePiece(std::vector<ScriptInstruction>&& c, uint32_t length, uint32_t offset) : code(c) {
        contentSplit.isOffset = 1;
        contentSplit.length = length;
        contentSplit.offset = offset;
    }
    ScriptCodePiece(std::vector<ScriptInstruction>&& c, uint64_t content) : code(c), contentString(content) {}
    //ScriptCodePiece(ScriptCodePiece&& o) noexcept : code(std::move(o.code)), contentString(o.contentString) {}
    //ScriptCodePiece(const ScriptCodePiece& o) noexcept : code(o.code), contentString(o.contentString) {}
    //
    //ScriptCodePiece& operator=(ScriptCodePiece&& o) noexcept {
    //    code = std::move(o.code);
    //    contentString = o.contentString;
    //    return *this;
    //}
    //ScriptCodePiece& operator=(const ScriptCodePiece& o) noexcept {
    //    code = o.code;
    //    contentString = o.contentString;
    //    return *this;
    //}

    ScriptCodePiece(): contentString(0) {}
};

struct ScriptConstantArray;

using ScriptConstant = std::variant<ScriptCodePiece, STRINGTYPE, float, bool, ScriptConstantArray>;

struct ScriptConstantArray {
    std::vector<ScriptConstant> content;
};


constexpr ConstantType getConstantType(const ScriptConstant& c) {
    switch (c.index()) {
        case 0: return ConstantType::code;
        case 1: return ConstantType::string;
        case 2: return ConstantType::scalar;
        case 3: return ConstantType::boolean;
        case 4: return ConstantType::array;
    }
    __debugbreak();
}

struct CompiledCodeData {
    uint32_t version{1};
    uint64_t codeIndex; //index to main code in constants
    std::vector<ScriptConstant> constants;
    std::vector<STRINGTYPE> fileNames;

#ifdef ASC_INTERCEPT
    std::vector<game_value> builtConstants;
#endif


    //#TODO compress constants, don't have duplicates for a number or string
};



template<typename T>
class Singleton {
    Singleton(const Singleton&) = delete;
    Singleton(Singleton&&) = delete;
    Singleton& operator=(const Singleton&) = delete;
    Singleton& operator=(Singleton&&) = delete;
public:
    static __forceinline T& get() noexcept {
        return _singletonInstance;
    }
    static void release() {
    }
protected:
    Singleton() noexcept {}
    static T _singletonInstance;
    static bool _initialized;
};
template<typename T>
T Singleton<T>::_singletonInstance;
template<typename T>
bool Singleton<T>::_initialized = false;