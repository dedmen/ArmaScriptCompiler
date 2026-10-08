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
    makeArray,
    makeHashMap
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
    nularCommand,
    hashMap
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


template <class Type, typename... Args>
__forceinline auto ConstructAtArgs(void* dst, Args&&... args ) { return ::new(dst) Type(::std::forward<Args>(args)...); }

class ScriptConstant
{
    ConstantType storedType = ConstantType::boolean;
    // size of biggest possible member
    union
    {
        char buffer[std::max(sizeof(ScriptCodePiece), sizeof(std::string))]; // This needs to be the biggest type
        //ScriptCodePiece _code;
        //RString _string;
        float _float;
        bool _bool;
    };


    void Destruct();
public:

    ConstantType getType() const {
        return storedType;
    }

    void forceSetType(ConstantType newType) {
        storedType = newType;
    }

    ScriptConstant() {}
    ~ScriptConstant() { Destruct(); }



    ScriptConstant(ScriptConstant&& other)
    {
        //memset(buffer, 0, sizeof(buffer));
        *this = std::move(other);
    }

    ScriptConstant(const ScriptConstant& other)
    {
        //memset(buffer, 0, sizeof(buffer));
        *this = other;
    }

    ScriptConstant(const ScriptCodePiece& code)
    {
        //memset(buffer, 0, sizeof(buffer));
        *this = code;
    }

    ScriptConstant(ScriptCodePiece&& code)
    {
        //memset(buffer, 0, sizeof(buffer));
        *this = std::move(code);
    }

    ScriptConstant(const STRINGTYPE& string)
    {
        //memset(buffer, 0, sizeof(buffer));
        *this = string;
    }

    ScriptConstant(STRINGTYPE&& string)
    {
        //memset(buffer, 0, sizeof(buffer));
        *this = std::move(string);
    }

    ScriptConstant(float scalar)
    {
        //memset(buffer, 0, sizeof(buffer));
        *this = scalar;
    }

    ScriptConstant(bool boolean)
    {
        //memset(buffer, 0, sizeof(buffer));
        *this = boolean;
    }

    ScriptConstant(const ScriptConstantArray& arr)
    {
        //memset(buffer, 0, sizeof(buffer));
        *this = arr;
    }

    ScriptConstant(ScriptConstantArray&& arr)
    {
        //memset(buffer, 0, sizeof(buffer));
        *this = std::move(arr);
    }

    ScriptConstant(const ScriptConstantNularCommand& arr)
    {
        //memset(buffer, 0, sizeof(buffer));
        *this = arr;
    }

    ScriptConstant(ScriptConstantNularCommand&& arr)
    {
        //memset(buffer, 0, sizeof(buffer));
        *this = std::move(arr);
    }

    // I could've done this with templates, but now I don't feel like changing it again

    inline ScriptConstant& operator=(const ScriptConstant& other)
    {
        switch (other.storedType)
        {
        case ConstantType::code: *this = other.GetCode(); break;
        case ConstantType::string: *this = other.GetString(); break;
        case ConstantType::scalar: *this = other.GetScalar(); break;
        case ConstantType::boolean: *this = other.GetBool(); break;
        case ConstantType::array: *this = other.GetArray(); break;
        case ConstantType::hashMap: *this = other.GetArray(); storedType = other.storedType; break;
        case ConstantType::nularCommand: *this = other.GetNularCommand(); break;
        }
        return *this;
    }

    inline ScriptConstant& operator=(ScriptConstant&& other)
    {
        Destruct();
        storedType = other.storedType;
        memmove(buffer, other.buffer, sizeof(buffer));
        other.storedType = ConstantType::boolean; // This basically clears other, no need to empty out its buffer
        return *this;
    }

    inline ScriptConstant& operator=(const ScriptCodePiece& code)
    {
        Destruct();
        storedType = ConstantType::code;
        ConstructAtArgs<ScriptCodePiece>(buffer, code);
        return *this;
    }

    inline ScriptConstant& operator=(ScriptCodePiece&& code)
    {
        Destruct();
        storedType = ConstantType::code;
        ConstructAtArgs<ScriptCodePiece>(buffer, std::move(code));
        return *this;
    }

    inline ScriptConstant& operator=(const STRINGTYPE& string)
    {
        Destruct();
        storedType = ConstantType::string;
        ConstructAtArgs<STRINGTYPE>(buffer, string);
        return *this;
    }

    inline ScriptConstant& operator=(STRINGTYPE&& string)
    {
        Destruct();
        storedType = ConstantType::string;
        ConstructAtArgs<STRINGTYPE>(buffer, std::move(string));
        return *this;
    }

    inline ScriptConstant& operator=(float scalar)
    {
        Destruct();
        storedType = ConstantType::scalar;
        ConstructAtArgs<float>(buffer, scalar);
        return *this;
    }

    inline ScriptConstant& operator=(bool boolean)
    {
        Destruct();
        storedType = ConstantType::boolean;
        ConstructAtArgs<bool>(buffer, boolean);
        return *this;
    }

    inline ScriptConstant &operator=(const ScriptConstantArray &arr);

    inline ScriptConstant &operator=(ScriptConstantArray &&arr);


    inline ScriptConstant& operator=(const ScriptConstantNularCommand& cmd)
    {
        Destruct();
        storedType = ConstantType::nularCommand;
        ConstructAtArgs<ScriptConstantNularCommand>(buffer, cmd);
        return *this;
    }

    inline ScriptConstant& operator=(ScriptConstantNularCommand&& cmd)
    {
        Destruct();
        storedType = ConstantType::nularCommand;
        ConstructAtArgs<ScriptConstantNularCommand>(buffer, std::move(cmd));
        return *this;
    }

    inline ScriptCodePiece& GetCode()
    {
        //Assert(storedType == ConstantType::code);
        return *reinterpret_cast<ScriptCodePiece*>(buffer);
    }

    inline STRINGTYPE& GetString()
    {
        //Assert(storedType == ConstantType::string);
        return *reinterpret_cast<STRINGTYPE*>(buffer);
    }

    inline float& GetScalar()
    {
        //Assert(storedType == ConstantType::scalar);
        return *reinterpret_cast<float*>(buffer);
    }

    inline bool& GetBool()
    {
        //Assert(storedType == ConstantType::boolean);
        return *reinterpret_cast<bool*>(buffer);
    }

    inline ScriptConstantArray& GetArray()
    {
        //Assert(storedType == ConstantType::array);
        return *reinterpret_cast<ScriptConstantArray*>(buffer);
    }

    inline const ScriptCodePiece& GetCode() const
    {
        //Assert(storedType == ConstantType::code);
        return *reinterpret_cast<const ScriptCodePiece*>(buffer);
    }

    inline const STRINGTYPE& GetString() const
    {
        //Assert(storedType == ConstantType::string);
        return *reinterpret_cast<const STRINGTYPE*>(buffer);
    }

    inline const float& GetScalar() const
    {
        //Assert(storedType == ConstantType::scalar);
        return *reinterpret_cast<const float*>(buffer);
    }

    inline const bool& GetBool() const
    {
        //Assert(storedType == ConstantType::boolean);
        return *reinterpret_cast<const bool*>(buffer);
    }

    inline const ScriptConstantArray& GetArray() const
    {
        //Assert(storedType == ConstantType::array);
        return *reinterpret_cast<const ScriptConstantArray*>(buffer);
    }

    inline const ScriptConstantNularCommand& GetNularCommand() const
    {
        //Assert(storedType == ConstantType::nularCommand);
        return *reinterpret_cast<const ScriptConstantNularCommand*>(buffer);
    }



    bool operator==(const ScriptConstant& right) const;

    // void GetHash(FNV1A_Hash& hash) const;
};

struct ScriptConstantArray {
    std::vector<ScriptConstant> content;
    bool operator==(const ScriptConstantArray& other) const;
};

inline void ScriptConstant::Destruct()
{
    switch (storedType)
    {
    case ConstantType::code:
        reinterpret_cast<ScriptCodePiece*>(buffer)->~ScriptCodePiece();
        break;
    case ConstantType::string:
        reinterpret_cast<STRINGTYPE*>(buffer)->clear();
        break;
    case ConstantType::scalar:
        *reinterpret_cast<float*>(buffer) = 0.f;
        break;
    case ConstantType::boolean:
        *reinterpret_cast<float*>(buffer) = false;
        break;
    case ConstantType::array:
    case ConstantType::hashMap:
        reinterpret_cast<ScriptConstantArray*>(buffer)->~ScriptConstantArray();
        break;
    case ConstantType::nularCommand:
        reinterpret_cast<ScriptConstantNularCommand*>(buffer)->~ScriptConstantNularCommand();
        break;
    }

    storedType = ConstantType::boolean; // We are gone now
}


ScriptConstant& ScriptConstant::operator=(const ScriptConstantArray& arr)
{
    Destruct();
    storedType = ConstantType::array;
    ConstructAtArgs<ScriptConstantArray>(buffer, arr);
    return *this;
}

ScriptConstant& ScriptConstant::operator=(ScriptConstantArray&& arr)
{
    Destruct();
    storedType = ConstantType::array;
    ConstructAtArgs<ScriptConstantArray>(buffer, std::move(arr));
    return *this;
}

inline bool ScriptConstant::operator==(const ScriptConstant &right) const
{
    if (storedType != right.storedType) return false;
    switch (storedType) {
        case ConstantType::code: return GetCode() == right.GetCode(); break;
        case ConstantType::string: return GetString() == right.GetString();
        case ConstantType::scalar: return GetScalar() == right.GetScalar();
        case ConstantType::boolean: return GetBool() == right.GetBool();
        case ConstantType::array: return GetArray() == right.GetArray();
        case ConstantType::hashMap: return GetArray() == right.GetArray();
        case ConstantType::nularCommand: return GetNularCommand().commandName == right.GetNularCommand().commandName;
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
    uint32_t version{2};
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