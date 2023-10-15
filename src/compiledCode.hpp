#pragma once
#include <vector>
#include <string>
#include <variant>
#include <string_view>
#include <execution>
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

#if not defined(_MSC_VER)
#define __forceinline __attribute__((always_inline))
#include <signal.h>
#define __debugbreak() raise(SIGTRAP)
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
    array,
    nularCommand
};

struct ScriptCodePiece {
    std::vector<ScriptInstruction> code;
    union {
        struct {
            unsigned offset : 32;
            unsigned length : 31;
            unsigned isOffset : 1;
        } contentSplit;
        uint64_t contentString; //pointer to constants
    };
    ScriptCodePiece(std::vector<ScriptInstruction>&& c, uint32_t length, uint32_t offset) : code(c) {
        contentSplit.isOffset = 1;
        if (length > 0x60'00'00'00)
            __debugbreak();
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
    bool operator==(const ScriptCodePiece& other) const {

        // the content string/offset will be different for multiple empty pieces of code, but we don't care because empty code piece won't throw errors so doesn't matter that its wrong
        if (code.empty() && other.code.empty()) return true;
        //#TODO actually compare code contents?

        return false;
    }

};

struct ScriptConstantNularCommand {
    STRINGTYPE commandName;
    ScriptConstantNularCommand(STRINGTYPE command) : commandName(command) {
        std::transform(commandName.begin(), commandName.end(), commandName.begin(), ::tolower);
    }
};


struct ScriptConstantArray;

using ScriptConstant = std::variant<ScriptCodePiece, STRINGTYPE, float, bool, ScriptConstantArray, ScriptConstantNularCommand>;

struct ScriptConstantArray {
    std::vector<ScriptConstant> content;
    bool operator==(const ScriptConstantArray& other) const;
};

constexpr ConstantType getConstantType(const ScriptConstant& c) {
    switch (c.index()) {
        case 0: return ConstantType::code;
        case 1: return ConstantType::string;
        case 2: return ConstantType::scalar;
        case 3: return ConstantType::boolean;
        case 4: return ConstantType::array;
        case 5: return ConstantType::nularCommand;
    }
    __debugbreak();
}


inline bool operator==(const ScriptConstant& left, const ScriptConstant& right) {
    if (left.index() != right.index()) return false;
    switch (getConstantType(left)) {
        case ConstantType::code: return std::get<ScriptCodePiece>(left) == std::get<ScriptCodePiece>(right); break;
        case ConstantType::string: return std::get<STRINGTYPE>(left) == std::get<STRINGTYPE>(right);
        case ConstantType::scalar: return std::get<float>(left) == std::get<float>(right);
        case ConstantType::boolean: return std::get<bool>(left) == std::get<bool>(right);
        case ConstantType::array:return std::get<ScriptConstantArray>(left) == std::get<ScriptConstantArray>(right);
        case ConstantType::nularCommand:return std::get<ScriptConstantNularCommand>(left).commandName == std::get<ScriptConstantNularCommand>(right).commandName;
    }

    return false;
}

inline bool ScriptConstantArray::operator==(const ScriptConstantArray& other) const {
    if (content.size() != other.content.size()) return false;

    return std::equal(content.begin(), content.end(), other.content.begin(), other.content.end(),
        [](const ScriptConstant& left, const ScriptConstant& right)
        {
            return left == right;
        });
}

struct CompiledCodeData {
    uint32_t version{1};
    uint64_t codeIndex; //index to main code in constants
    std::vector<ScriptConstant> constants;
    std::vector<STRINGTYPE> fileNames;

#ifdef ASC_INTERCEPT
    std::vector<game_value> builtConstants;
#endif

    // temporary for serialization
    mutable std::vector<STRINGTYPE> commandNameDirectory;

    uint16_t getIndexFromCommandNameDirectory(std::string_view text) const {
 
        auto found = std::lower_bound(commandNameDirectory.begin(), commandNameDirectory.end(), text);
        if (found == commandNameDirectory.end())
        {
            __debugbreak();
        }

        return std::distance(commandNameDirectory.begin(), found);
    }


    uint64_t AddConstant(ScriptConstant&& constant) {
        auto found = std::find_if(std::execution::par_unseq, constants.begin(), constants.end(), [&constant](const ScriptConstant& cnst) {
            return cnst == constant;
        });
        if (found == constants.end()) {
            constants.emplace_back(std::move(constant));
            return constants.size() - 1;
        }

        return std::distance(constants.begin(), found);
    }

    uint64_t AddConstant(const ScriptConstant& constant) {
        auto found = std::find_if(std::execution::par_unseq, constants.begin(), constants.end(), [&constant](const ScriptConstant& cnst) {
            return cnst == constant;
            });
        if (found == constants.end()) {
            constants.emplace_back(constant);
            return constants.size() - 1;
        }

        return std::distance(constants.begin(), found);
    }

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