#include "scriptSerializer.hpp"
#include <iostream>
#define ZSTD_STATIC_LINKING_ONLY // ZSTD_findDecompressedSize
#include <functional>
#include <zstd.h>
#include <sstream>

static constexpr const int compressionLevel = 22;

void blaBla(const CompiledCodeData& code, const std::vector<ScriptInstruction>& inst, std::ostream& output);

void blaBLaConstant(const CompiledCodeData& code, const ScriptConstant& constant, std::ostream& output, bool inArray = false) {
    
    switch (getConstantType(constant)) {
    case ConstantType::code:
        output << "push CODE {\n";
        blaBla(code, std::get<0>(constant).code, output);
        output << "}\n";
        break;
    case ConstantType::string:
        if (!inArray)
            output << "push STRING " << std::get<STRINGTYPE>(constant) << "\n";
        else
            output << std::get<STRINGTYPE>(constant) << ", ";
        break;
    case ConstantType::scalar:
        if (!inArray)
            output << "push SCALAR " << std::get<float>(constant) << "\n";
        else
            output << std::get<float>(constant) << ", ";
        break;
    case ConstantType::boolean:
        if (!inArray)
            output << "push BOOL " << std::get<bool>(constant) << "\n";
        else
            output << std::get<bool>(constant) << ", ";
        break;
    case ConstantType::array:
        if (!inArray)
            output << "push ARRAY [";
        else
            output << "[\n";
        for (auto& it : std::get<4>(constant).content)
            blaBLaConstant(code, it, output, true);
        output.seekp(-2, SEEK_CUR);
        output << "]\n";
        break;
    default:;
    }

}

void blaBla(const CompiledCodeData& code, const std::vector<ScriptInstruction>& inst , std::ostream& output) {//#TODO move into proper func
    for (auto& it : inst) {
        switch (it.type) {
        case InstructionType::endStatement:
            output << "endStatement\n";
            break;
        case InstructionType::push: {
            auto index = std::get<uint64_t>(it.content);
            auto constant = code.constants[index];
            blaBLaConstant(code, constant, output);
        } break;
        case InstructionType::callUnary:
            output << "callUnary " << std::get<STRINGTYPE>(it.content) << "\n";
            break;
        case InstructionType::callBinary:
            output << "callBinary " << std::get<STRINGTYPE>(it.content) << "\n";
            break;
        case InstructionType::callNular:
            output << "callNular " << std::get<STRINGTYPE>(it.content) << "\n";
            break;
        case InstructionType::assignTo:
            output << "assignTo " << std::get<STRINGTYPE>(it.content) << "\n";
            break;
        case InstructionType::assignToLocal:
            output << "assignToLocal " << std::get<STRINGTYPE>(it.content) << "\n";
            break;
        case InstructionType::getVariable:
            output << "getVariable " << std::get<STRINGTYPE>(it.content) << "\n";
            break;
        case InstructionType::makeArray:
            output << "makeArray " << std::get<uint64_t>(it.content) << "\n";
            break;
        default:;
        }
    }
}


void ScriptSerializer::compiledToHumanReadable(const CompiledCodeData& code, std::ostream& output) {

    //#TODO fix this
    //Check output of
    // params ["_unit", "_pos", ["_target", objNull], ["_buildings", []]];
    // + [1, 2, 3];
    // _buildings pushBack[1, 2, 3];

    // array fuckup, no quotes on string constants inside the array

    blaBla(code, std::get<0>(code.constants[code.codeIndex]).code, output);
}

/*
block type
    content


 */

enum class SerializedBlockType {
    constant,
    constantCompressed,
    locationInfo,
    code,
    codeDebug,
    commandNameDirectory // lookup table for script command/variable names. Optional
};


template<typename Type>
Type readT(std::istream& stream) {
    Type ret;
    stream.read(reinterpret_cast<char*>(&ret), sizeof(Type));
    return ret;
}


template<typename Type>
void writeT(Type data, std::ostream& stream) {
    stream.write(reinterpret_cast<const char*>(&data), sizeof(Type));
}


#include <lzokay.hpp>

void ScriptSerializer::compiledToBinary(const CompiledCodeData& code, std::ostream& output) {
    writeT<uint32_t>(code.version, output); //version
    output.flush();

    // lookup table for script command/variable names. Optional, if code.commandNameDirectory is empty, script commands will be serialized as string names
    {

        std::set<STRINGTYPE> commandNameDirectory;
        collectCommandNames(code, commandNameDirectory);

        code.commandNameDirectory.clear();
        std::copy(commandNameDirectory.begin(), commandNameDirectory.end(), std::back_inserter(code.commandNameDirectory));
        std::sort(code.commandNameDirectory.begin(), code.commandNameDirectory.end());

        if (code.commandNameDirectory.size() != static_cast<uint16_t>(code.commandNameDirectory.size())) {
            __debugbreak();
            // too big.
            code.commandNameDirectory.clear();
        }
            


        if (!code.commandNameDirectory.empty()) {
            writeT(static_cast<uint8_t>(SerializedBlockType::commandNameDirectory), output);
            
            std::ostringstream buffer(std::ostringstream::binary);
            writeT(static_cast<uint16_t>(code.commandNameDirectory.size()), buffer);
            for (auto& it : code.commandNameDirectory)
                writeString(buffer, it);




            auto bufferContent = buffer.str();

            lzokay::Dict<> dict;
            std::size_t estimated_size = lzokay::compress_worst_size(bufferContent.size());
            std::unique_ptr<uint8_t[]> compressed(new uint8_t[estimated_size]);
            std::size_t compressed_size;
            auto error = lzokay::compress((const uint8_t*)bufferContent.data(), bufferContent.size(), compressed.get(), estimated_size,
                compressed_size, dict);
            if (error < lzokay::EResult::Success)
                __debugbreak();

            writeT(static_cast<uint32_t>(bufferContent.size()), output); // uncompressed size
            writeT(static_cast<uint8_t>(2), output); // compression method, always 2
            output.write((const char*)compressed.get(), compressed_size);

            output.flush();
        }


    }
    
    if (true) {
        writeT(static_cast<uint8_t>(SerializedBlockType::constantCompressed), output);

        std::ostringstream buffer(std::ostringstream::binary);
        writeConstants(code, buffer);
        auto bufferContent = buffer.str();

        lzokay::Dict<> dict;
        std::size_t estimated_size = lzokay::compress_worst_size(bufferContent.size());
        std::unique_ptr<uint8_t[]> compressed(new uint8_t[estimated_size]);
        std::size_t compressed_size;
        auto error = lzokay::compress((const uint8_t*)bufferContent.data(), bufferContent.size(), compressed.get(), estimated_size,
            compressed_size, dict);
        if (error < lzokay::EResult::Success)
            __debugbreak();

        writeT(static_cast<uint32_t>(bufferContent.size()), output); // uncompressed size
        writeT(static_cast<uint8_t>(2), output); // compression method, always 2
        output.write((const char*)compressed.get(), compressed_size);
    } else {
        writeT(static_cast<uint8_t>(SerializedBlockType::constant), output);
        writeConstants(code, output);
    }

    output.flush();

    auto pos = output.tellp();

    writeT(static_cast<uint8_t>(SerializedBlockType::locationInfo), output);
    output.flush();
    writeT(static_cast<uint16_t>(code.fileNames.size()), output);
    for (auto& it : code.fileNames)
        writeString(output, it);
    output.flush();
    writeT(static_cast<uint8_t>(SerializedBlockType::code), output);
    writeT(code.codeIndex, output);
    output.flush();
}

CompiledCodeData ScriptSerializer::binaryToCompiled(std::istream& input) {
    CompiledCodeData output;
    output.version = readT<uint32_t>(input);
    while (!input.eof()) {
        auto elementType = readT<uint8_t>(input);
        auto type = static_cast<SerializedBlockType>(elementType);

        uint32_t pos = input.tellg();

        switch (type) {

            case SerializedBlockType::constant: {
                readConstants(output, input);
            } break;
            case SerializedBlockType::locationInfo: {
                auto locCount = readT<uint16_t>(input);

                for (uint16_t i = 0; i < locCount; ++i) {
                    output.fileNames.emplace_back(readString(input));
                }
            } break;
            case SerializedBlockType::code: {
                output.codeIndex = readT<uint64_t>(input);
            } break;
        }
    }
    return output;
}

void ScriptSerializer::compiledToBinaryCompressed(const CompiledCodeData& code, std::ostream& output) {
    std::stringstream uncompressedData(std::stringstream::binary | std::stringstream::out | std::stringstream::in);
    compiledToBinary(code, uncompressedData);

    std::vector<char> uncompressedVec;

    std::streampos end = uncompressedData.tellg();
    uncompressedData.seekg(0, std::ios_base::beg);
    std::streampos beg = uncompressedData.tellg();

    uncompressedVec.reserve(end - beg);

    uncompressedVec.assign(std::istreambuf_iterator<char>(uncompressedData), std::istreambuf_iterator<char>());

    auto compressedData = compressData(uncompressedVec);
    uncompressedVec.clear();
    output.write(compressedData.data(), compressedData.size());
}


template<typename CharT, typename TraitsT = std::char_traits<CharT> >
class vectorwrapbuf : public std::basic_streambuf<CharT, TraitsT> {
public:
    vectorwrapbuf(std::vector<CharT>& vec) {
        setg(vec.data(), vec.data(), vec.data() + vec.size());
    }
};


CompiledCodeData ScriptSerializer::binaryToCompiledCompressed(std::istream& input) {
    std::vector<char> compressedVec;

    compressedVec.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());

    auto decompressedData = decompressData({ compressedVec.data(), compressedVec.size() });
    compressedVec.clear();

    vectorwrapbuf<char> databuf(decompressedData);
    std::istream is(&databuf);

    return binaryToCompiled(is);
}

CompiledCodeData ScriptSerializer::binaryToCompiledCompressed(std::string_view input) {
    auto decompressedData = decompressData(input);

    vectorwrapbuf<char> databuf(decompressedData);
    std::istream is(&databuf);

    return binaryToCompiled(is);
}

void ScriptSerializer::instructionToBinary(const CompiledCodeData& code, const ScriptInstruction& instruction, std::ostream& output) {

    writeT(static_cast<uint8_t>(instruction.type), output);

    if (instruction.type != InstructionType::endStatement && instruction.type != InstructionType::push) {
        //these can't fail, so we don't need source info
        writeT(static_cast<uint32_t>(instruction.offset), output);
        writeT(static_cast<uint8_t>(instruction.fileIndex), output);
        writeT(static_cast<uint16_t>(instruction.line), output);
    }

    output.flush();
    switch (instruction.type) {

        case InstructionType::endStatement: break;
        case InstructionType::push: {
            auto constantIndex = std::get<uint64_t>(instruction.content);
           if (constantIndex != static_cast<uint16_t>(constantIndex))
               __debugbreak();
            writeT(static_cast<uint16_t>(constantIndex), output);
        } break;
        case InstructionType::callUnary: 
        case InstructionType::callBinary: 
        case InstructionType::callNular:
        case InstructionType::assignTo:
        case InstructionType::assignToLocal:
        case InstructionType::getVariable:
            if (code.commandNameDirectory.empty())
                writeString(output, std::get<STRINGTYPE>(instruction.content));
            else {
                auto index = code.getIndexFromCommandNameDirectory(std::get<STRINGTYPE>(instruction.content));
                writeT<uint16_t>(index, output);
            }
            break;
        case InstructionType::makeArray: {
            auto constantIndex = std::get<uint64_t>(instruction.content);
            if (constantIndex != static_cast<uint16_t>(constantIndex))
                __debugbreak();
            writeT(static_cast<uint16_t>(constantIndex), output);
        } break;
            
        default: ;
    }
}

void ScriptSerializer::instructionsToBinary(const CompiledCodeData& code, const std::vector<ScriptInstruction>& instructions, std::ostream& output) {
    writeT(static_cast<uint32_t>(instructions.size()), output);
    for (auto& it : instructions)
        instructionToBinary(code, it, output);
}

ScriptInstruction ScriptSerializer::binaryToInstruction(const CompiledCodeData& code, std::istream& input) {

    uint32_t position = input.tellg();

    auto instructionType = readT<uint8_t>(input);
    auto type = static_cast<InstructionType>(instructionType);

    uint32_t offset = 0;
    uint8_t fileIndex = 0;
    uint16_t fileLine = 0;

    if (type != InstructionType::endStatement && type != InstructionType::push) {
        //these can't fail, so we don't need source info
        offset = readT<uint32_t>(input);
        fileIndex = readT<uint8_t>(input);
        fileLine = readT<uint16_t>(input);
    }

    switch (type) {
        case InstructionType::endStatement: 
            return ScriptInstruction{ type, offset, fileIndex, fileLine, {} };
        case InstructionType::push: {
            uint64_t constIndex = readT<uint16_t>(input);

            return ScriptInstruction{ type, offset, fileIndex, fileLine, constIndex };
        }
        case InstructionType::callUnary:
        case InstructionType::callBinary:
        case InstructionType::callNular:
        case InstructionType::assignTo:
        case InstructionType::assignToLocal:
        case InstructionType::getVariable: {
            STRINGTYPE commandName;
            if (code.commandNameDirectory.empty())
                commandName = readString(input);
            else
                commandName = code.commandNameDirectory[readT<uint16_t>(input)];

            return ScriptInstruction{ type, offset, fileIndex, fileLine, commandName };
        }
        case InstructionType::makeArray: {
            auto arraySize = readT<uint16_t>(input);
            return ScriptInstruction{ type, offset, fileIndex, fileLine, static_cast<uint64_t>(arraySize) };
        }
    }
    __debugbreak();
}

std::vector<ScriptInstruction> ScriptSerializer::binaryToInstructions(const CompiledCodeData& code, std::istream& input) {
    auto pos = input.tellg();
    auto count = readT<uint32_t>(input);
    std::vector<ScriptInstruction> result;
    result.reserve(count);
    for (uint32_t i = 0; i < count; ++i)
        result.emplace_back(binaryToInstruction(code, input));
    return result;
}

void ScriptSerializer::writeConstant(const CompiledCodeData& code, const ScriptConstant& constant, std::ostream& output) {
    auto type = getConstantType(constant);
    writeT(static_cast<uint8_t>(type), output);

    switch (type) {
    case ConstantType::code: {
        auto& instructions = std::get<ScriptCodePiece>(constant);
        writeT<uint64_t>(instructions.contentString, output);
        instructionsToBinary(code, instructions.code, output);
    } break;
    case ConstantType::string:
        writeString(output, std::get<STRINGTYPE>(constant));
        break;
    case ConstantType::scalar:
        writeT(std::get<float>(constant), output);
        break;
    case ConstantType::boolean:
        writeT(std::get<bool>(constant), output);
        break;
    case ConstantType::array: {
        auto& array = std::get<ScriptConstantArray>(constant);
        if (static_cast<uint32_t>(array.content.size()) != array.content.size()) // truncation
            __debugbreak();
        writeT<uint32_t>(static_cast<uint32_t>(array.content.size()), output);

        for (auto& cnst : array.content)
            writeConstant(code, cnst, output);
    } break;
    case ConstantType::nularCommand:
        writeString(output, std::get<ScriptConstantNularCommand>(constant).commandName);
        break;
    default: __debugbreak();
    }
}

ScriptConstant ScriptSerializer::readConstant(CompiledCodeData& code, std::istream& input) {
    
    auto typeRaw = readT<uint8_t>(input);

    auto type = static_cast<ConstantType>(typeRaw);

    switch (type) {
        case ConstantType::code: {
            ScriptCodePiece piece;
            piece.contentString = readT<uint64_t>(input);
            piece.code = binaryToInstructions(code, input);

           return piece;
        } break;
        case ConstantType::string:
            return readString(input);
        case ConstantType::scalar: 
            return readT<float>(input);
        case ConstantType::boolean: 
            return readT<bool>(input);
        case ConstantType::array: {
            auto size = readT<uint32_t>(input);
            ScriptConstantArray arr;
            arr.content.reserve(size);
            for (int i = 0; i < size; ++i) {
                arr.content.emplace_back(readConstant(code, input));
            }
            return arr;
        } break;
        case ConstantType::nularCommand: 
            return ScriptConstantNularCommand(readString(input));
        default: __debugbreak();
    }

}

void ScriptSerializer::writeConstants(const CompiledCodeData& code, std::ostream& output) {
    if (static_cast<uint16_t>(code.constants.size()) != code.constants.size()) {
        throw std::runtime_error("too many constants");
    }
    writeT(static_cast<uint16_t>(code.constants.size()), output);
    int index = 0;
    for (auto& constant : code.constants) {
        writeConstant(code, constant, output);
    }
}

void ScriptSerializer::readConstants(CompiledCodeData& code, std::istream& input) {
    auto constantCount = readT<uint16_t>(input);

    code.constants.reserve(constantCount);
    for (int i = 0; i < constantCount; ++i) {
        code.constants.emplace_back(readConstant(code, input));
    }

}

void ScriptSerializer::collectCommandNames(const CompiledCodeData& code, std::set<STRINGTYPE>& directory) {

    std::function<void(const std::vector<ScriptConstant>&)> collectArray = [&directory, &collectArray](const std::vector<ScriptConstant>& content)
    {
        for (const auto& it : content) {
            auto type = getConstantType(it);
            if (type == ConstantType::code) {
                const auto& instructions = std::get<ScriptCodePiece>(it);
                collectCommandNames(instructions.code, directory);
            }
            if (type == ConstantType::array) {
                auto& array = std::get<ScriptConstantArray>(it);
                collectArray(array.content);
            }
        }
    };

    for (const auto& it : code.constants) {
        auto type = getConstantType(it);
        if (type == ConstantType::code) {
            const auto& instructions = std::get<ScriptCodePiece>(it);
            collectCommandNames(instructions.code, directory);
        }
        if (type == ConstantType::array) {
            auto& array = std::get<ScriptConstantArray>(it);
            collectArray(array.content);
        }
    }
}

void ScriptSerializer::collectCommandNames(const std::vector<ScriptInstruction>& instructions, std::set<std::string>& directory) {
    for (const auto& it : instructions)
        switch (it.type) {
          case InstructionType::endStatement: break;
          case InstructionType::push: break;
          case InstructionType::makeArray: break;  
          case InstructionType::callUnary:
          case InstructionType::callBinary:
          case InstructionType::callNular:
          case InstructionType::assignTo:
          case InstructionType::assignToLocal:
          case InstructionType::getVariable:
              directory.emplace(std::get<STRINGTYPE>(it.content));
              break;
        }
}


std::vector<char> ScriptSerializer::compressData(const std::vector<char>& data) {
    const size_t cBuffSize = ZSTD_compressBound(data.size());
    std::vector<char> outputBuffer;
    outputBuffer.resize(cBuffSize);

    size_t const cSize = ZSTD_compress(outputBuffer.data(), outputBuffer.size(), data.data(), data.size(), compressionLevel);
    if (ZSTD_isError(cSize)) {
        __debugbreak();
    }
    outputBuffer.resize(cSize);
    return outputBuffer;
}

std::vector<char> ScriptSerializer::compressDataDictionary(const std::vector<char>& data, const std::vector<char>& dictionary) {
    ZSTD_CDict* const cdict = ZSTD_createCDict(dictionary.data(), dictionary.size(), compressionLevel);

    const size_t cBuffSize = ZSTD_compressBound(data.size());
    std::vector<char> outputBuffer;
    outputBuffer.resize(cBuffSize);


    ZSTD_CCtx* const cctx = ZSTD_createCCtx();
    if (cctx == NULL) { fprintf(stderr, "ZSTD_createCCtx() error \n"); exit(10); }
    size_t const cSize = ZSTD_compress_usingCDict(cctx, outputBuffer.data(), outputBuffer.size(), data.data(), data.size(), cdict);
    if (ZSTD_isError(cSize)) {
        __debugbreak();
    }
    outputBuffer.resize(cSize);
    ZSTD_freeCCtx(cctx);
    ZSTD_freeCDict(cdict); //#TODO keep dict
    return outputBuffer;
}

std::vector<char> ScriptSerializer::decompressData(std::string_view data) {

    const size_t rSize = ZSTD_findDecompressedSize(data.data(), data.size());

    if (rSize == ZSTD_CONTENTSIZE_ERROR) {
        //fprintf(stderr, "%s : it was not compressed by zstd.\n", fname);
        __debugbreak();
    }
    else if (rSize == ZSTD_CONTENTSIZE_UNKNOWN) {
       // fprintf(stderr, "%s : original size unknown. Use streaming decompression instead.\n", fname);
        __debugbreak();
    }


    std::vector<char> outputBuffer;
    outputBuffer.resize(rSize);

    size_t const dSize = ZSTD_decompress(outputBuffer.data(), outputBuffer.size(), data.data(), data.size());
    if (dSize != rSize) {
        //fprintf(stderr, "error decoding %s : %s \n", fname, ZSTD_getErrorName(dSize));
        __debugbreak();
    }

    return outputBuffer;
}

std::vector<char> ScriptSerializer::decompressDataDictionary(const std::vector<char>& data, const std::vector<char>& dictionary) {
    ZSTD_DDict* const ddict = ZSTD_createDDict(dictionary.data(), dictionary.size());

    const size_t cBuffSize = ZSTD_compressBound(data.size());
    std::vector<char> outputBuffer;
    outputBuffer.resize(cBuffSize);


    ZSTD_DCtx* const dctx = ZSTD_createDCtx();
    if (dctx == NULL) { fprintf(stderr, "ZSTD_createDCtx() error \n"); exit(10); }
    size_t const cSize = ZSTD_decompress_usingDDict(dctx, outputBuffer.data(), outputBuffer.size(), data.data(), data.size(), ddict);
    if (ZSTD_isError(cSize)) {
        __debugbreak();
    }
    outputBuffer.resize(cSize);
    ZSTD_freeDCtx(dctx);
    ZSTD_freeDDict(ddict); //#TODO keep dict

    return outputBuffer;
}

STRINGTYPE ScriptSerializer::readString(std::istream& input) {
    uint32_t length;
    input.read(reinterpret_cast<char*>(&length) + 1, 3);


    if constexpr (std::is_same_v<STRINGTYPE, std::string>) {
        STRINGTYPE result;
        result.resize(length);

        input.read(result.data(), length);

        return result;
    } else { // RString
        STRINGTYPE result;
        result.resize(length);

        input.read(result.data(), length);

        return result;
    }
    

}

void ScriptSerializer::writeString(std::ostream& output, std::string_view string) {
    uint32_t length = string.length();
    // special stuff to write a 24bit number
    //Assert(length & 0x00FFFFFF == length); // check that 24bit is not truncating

    if ((length & 0x00FFFFFF) != length)
        __debugbreak();

    output.write(reinterpret_cast<char*>(&length), 3);
    output.write(string.data(), length);
}
