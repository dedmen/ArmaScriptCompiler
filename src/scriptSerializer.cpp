#include "scriptSerializer.hpp"
#include <iostream>
#define ZSTD_STATIC_LINKING_ONLY // ZSTD_findDecompressedSize
#include <zstd.h>
#include <sstream>

static constexpr const int compressionLevel = 22;

void blaBla(const CompiledCodeData& code, const std::vector<ScriptInstruction>& inst , std::ostream& output) {//#TODO move into proper func
    for (auto& it : inst) {
        switch (it.type) {
        case InstructionType::endStatement:
            output << "endStatement\n";
            break;
        case InstructionType::push: {
            auto index = std::get<uint64_t>(it.content);
            auto constant = code.constants[index];

            switch (getConstantType(constant)) {
            case ConstantType::code:
                output << "push CODE {\n";
                blaBla(code, std::get<0>(constant), output);
                output << "}\n";
                break;
            case ConstantType::string:
                output << "push STRING " << std::get<STRINGTYPE>(constant) << "\n";
                break;
            case ConstantType::scalar:
                output << "push SCALAR " << std::get<float>(constant) << "\n";
                break;
            case ConstantType::boolean:
                output << "push BOOL " << std::get<bool>(constant) << "\n";
                break;
            default:;
            }
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
    blaBla(code, code.instructions, output);
}

/*
block type
    content


 */

enum class SerializedBlockType {
    constant,
    locationInfo,
    code
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




void ScriptSerializer::compiledToBinary(const CompiledCodeData& code, std::ostream& output) {
    writeT<uint32_t>(code.version, output); //version
    output.flush();
    writeConstants(code, output);
    output.flush();

    writeT(static_cast<uint8_t>(SerializedBlockType::locationInfo), output);
    output.flush();
    writeT(static_cast<uint16_t>(code.fileNames.size()), output);
    for (auto& it : code.fileNames)
        output.write(it.c_str(), it.size() + 1);
    output.flush();
    writeT(static_cast<uint8_t>(SerializedBlockType::code), output);
    instructionsToBinary(code, code.instructions, output);
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
                    std::string content;
                    std::getline(input, content, '\0');
                    output.fileNames.emplace_back(std::move(content));
                }
            } break;
            case SerializedBlockType::code: {
                auto instructions = binaryToInstructions(output, input);
                output.instructions = std::move(instructions);
            } break;
        }
    }
    return output;
}

void ScriptSerializer::compiledToBinaryCompressed(const CompiledCodeData& code, std::ostream& output) {
    std::stringstream uncompressedData;
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

    auto decompressedData = decompressData(compressedVec);
    compressedVec.clear();

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
            writeT(std::get<uint64_t>(instruction.content), output);
        } break;
        case InstructionType::callUnary: 
        case InstructionType::callBinary: 
        case InstructionType::callNular:
        case InstructionType::assignTo:
        case InstructionType::assignToLocal:
        case InstructionType::getVariable:
            output.write(std::get<STRINGTYPE>(instruction.content).c_str(), std::get<STRINGTYPE>(instruction.content).size() + 1);
            break;
        case InstructionType::makeArray:
            writeT(static_cast<uint32_t>(std::get<uint64_t>(instruction.content)), output);
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
            auto constIndex = readT<uint64_t>(input);

            return ScriptInstruction{ type, offset, fileIndex, fileLine, constIndex };
        }
        case InstructionType::callUnary:
        case InstructionType::callBinary:
        case InstructionType::callNular:
        case InstructionType::assignTo:
        case InstructionType::assignToLocal:
        case InstructionType::getVariable: {
            std::string content;
            std::getline(input, content, '\0');

            return ScriptInstruction{ type, offset, fileIndex, fileLine, static_cast<STRINGTYPE>(content) };
        }
        case InstructionType::makeArray: {
            auto arraySize = readT<uint32_t>(input);
            return ScriptInstruction{ type, offset, fileIndex, fileLine, static_cast<uint64_t>(arraySize) };
        }
    }
    __debugbreak();
}

std::vector<ScriptInstruction> ScriptSerializer::binaryToInstructions(const CompiledCodeData& code, std::istream& input) {
    auto count = readT<uint32_t>(input);
    std::vector<ScriptInstruction> result;
    result.reserve(count);
    for (uint32_t i = 0; i < count; ++i)
        result.emplace_back(binaryToInstruction(code, input));
    return result;
}

void ScriptSerializer::writeConstants(const CompiledCodeData& code, std::ostream& output) {
    writeT(static_cast<uint8_t>(SerializedBlockType::constant), output);
    writeT(static_cast<uint16_t>(code.constants.size()), output);

    for (auto& constant : code.constants) {
        auto type = getConstantType(constant);
        writeT(static_cast<uint8_t>(type), output);

        switch (type) {
        case ConstantType::code: {
            auto& instructions = std::get<std::vector<ScriptInstruction>>(constant);
            instructionsToBinary(code, instructions, output);
        } break;
        case ConstantType::string:
            output.write(std::get<STRINGTYPE>(constant).c_str(), std::get<STRINGTYPE>(constant).size() + 1);
            break;
        case ConstantType::scalar:
            writeT(std::get<float>(constant), output);
            break;
        case ConstantType::boolean:
            writeT(std::get<bool>(constant), output);
            break;
        }
    }
}

void ScriptSerializer::readConstants(CompiledCodeData& code, std::istream& input) {
    auto constantCount = readT<uint16_t>(input);

    code.constants.reserve(constantCount);
    for (int i = 0; i < constantCount; ++i) {
        auto typeRaw = readT<uint8_t>(input);

        auto type = static_cast<ConstantType>(typeRaw);

        switch (type) {
            case ConstantType::code: {
                auto instructions = binaryToInstructions(code, input);
                code.constants.emplace_back(std::move(instructions));
            } break;
            case ConstantType::string: {
                std::string content;
                std::getline(input, content, '\0');
#ifndef ASC_INTERCEPT
                code.constants.emplace_back(std::move(content));
#else
                code.constants.emplace_back(static_cast<STRINGTYPE>(content));
#endif
            } break;
            case ConstantType::scalar: {
                auto data = readT<float>(input);
                code.constants.emplace_back(data);
            } break;
            case ConstantType::boolean: {
                auto data = readT<bool>(input);
                code.constants.emplace_back(data);
            } break;
        }
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

std::vector<char> ScriptSerializer::decompressData(const std::vector<char>& data) {

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
