#include "scriptCompiler.hpp"
#include "commandList.hpp"
#include <charconv>
#include <fstream>

#include <iostream>
#include <mutex>
#include "optimizer/optimizerModuleBase.hpp"
#include "optimizer/optimizer.h"
#include "scriptSerializer.hpp"

#include "fileio/default.h"
#include "operators/ops.h"
#include "parser/config/config_parser.hpp"
#include "parser/preprocessor/default.h"
#include "parser/sqf/parser.tab.hh"
#include "runtime/d_string.h"

std::once_flag commandMapInitFlag;

class MyLogger : public StdOutLogger {
    std::ofstream logOut;
    std::mutex lock;
public:
    MyLogger() : logOut("P:\\log.txt") {}
    void log(const LogMessageBase& message) override {
        std::lock_guard l(lock);
        StdOutLogger::log(message);

        logOut << Logger::loglevelstring(message.getLevel()) << ' ' << message.formatMessage() << std::endl;
    }
};


static MyLogger vmlogger;

void ScriptCompiler::init()
{
    vmlogger.setEnabled(loglevel::trace, false);
    vm = std::make_unique<sqf::runtime::runtime>(vmlogger, sqf::runtime::runtime::runtime_conf{});

    vm->fileio(std::make_unique<sqf::fileio::impl_default>(vmlogger));
    vm->parser_config(std::make_unique<sqf::parser::config::parser>(vmlogger));
    vm->parser_preprocessor(std::make_unique<sqf::parser::preprocessor::impl_default>(vmlogger));
    vm->parser_sqf(std::make_unique<sqf::parser::sqf::parser>(vmlogger));

    //sqf::operators::ops_dummy_binary(*vm);
    //sqf::operators::ops_dummy_unary(*vm);
    //sqf::operators::ops_dummy_nular(*vm);

    //sqf::operators::ops(*vm);
    CommandList::init(*vm);

    //std::call_once(commandMapInitFlag, []() {
    //    CommandList::init(*vm);
    //    //sqf::commandmap::get().init();
    //});

    addPragma({ "ASC_ignoreFile", [this](const sqf::runtime::parser::pragma& m,
                    ::sqf::runtime::runtime& runtime,
                    const ::sqf::runtime::diagnostics::diag_info dinf,
                    const ::sqf::runtime::fileio::pathinfo location,
                    const std::string& data) -> std::string
    {
        ignoreCurrentFile = true;
        return {}; // string return type is wrong, isn't used for anything
    } });
}

ScriptCompiler::ScriptCompiler(const std::vector<std::filesystem::path>& includePaths) {
    init();
    initIncludePaths(includePaths);
}

ScriptCompiler::ScriptCompiler() {
    init();
}

CompiledCodeData ScriptCompiler::compileScript(std::filesystem::path physicalPath, std::filesystem::path virtualPath) {
    std::ifstream inputFile(physicalPath);
    
    auto filesize = std::filesystem::file_size(physicalPath);
    if (filesize == 0) // uh. oki
        return CompiledCodeData();

    std::string scriptCode;
    scriptCode.resize(filesize);
    inputFile.read(scriptCode.data(), filesize);

    if (
        static_cast<unsigned char>(scriptCode[0]) == 0xef &&
        static_cast<unsigned char>(scriptCode[1]) == 0xbb &&
        static_cast<unsigned char>(scriptCode[2]) == 0xbf
        ) {
        scriptCode.erase(0, 3);
    }

    // #OPTION force all script files to contain a include
    //if (scriptCode.find("script_component.hpp\"") == std::string::npos) {
    //    throw std::domain_error("no include");
    //}

    
    auto preprocessedScript = vm->parser_preprocessor().preprocess(*vm, scriptCode, sqf::runtime::fileio::pathinfo(physicalPath.string(), virtualPath.string()) );
    if (!preprocessedScript) {
        //__debugbreak();
        return CompiledCodeData();
    }

    if (ignoreCurrentFile)
    {
        std::cout << "File " << physicalPath.generic_string() << " skipped due to ASC_ignoreFile pragma\n";
        ignoreCurrentFile = false;
        return CompiledCodeData();
    }

    bool errorflag = false;


    sqf::parser::sqf::parser sqfParser(vmlogger);
    sqf::parser::sqf::bison::astnode ast;
    sqf::parser::sqf::tokenizer tokenizer(preprocessedScript->begin(), preprocessedScript->end(), virtualPath.string());
    auto errflag = !sqfParser.get_tree(*vm, tokenizer, &ast);
    if (errflag || ast.children.empty())  {
        //__debugbreak();
        return CompiledCodeData();
    }

    //print_navigate_ast(&std::cout, ast, sqf::parse::sqf::astkindname);

    CompiledCodeData stuff;
    CompileTempData temp;
    ScriptCodePiece mainCode;


    if (true) {
        auto& statementsNode = ast.children[0];
        if (statementsNode.kind != sqf::parser::sqf::bison::astkind::STATEMENTS)
            __debugbreak();

        ///statementsNode.kind = sqf::parser::sqf::bison::astkind::CODE;

        auto node = OptimizerModuleBase::nodeFromAST(statementsNode);
    
        //std::ofstream nodeo("P:\\node.txt");
        //node.dumpTree(nodeo, 0);
        //nodeo.close();
    
        //auto res = node.bottomUpFlatten();
    
        Optimizer opt;
    
        opt.optimize(node);
    
        //std::ofstream nodeop("P:\\nodeOpt.txt");
        //node.dumpTree(nodeop, 0);
        //nodeop.close();
    
        ASTToInstructions(stuff, temp, mainCode.code, node);
        mainCode.contentString = stuff.AddConstant(std::move(*preprocessedScript));
        stuff.codeIndex = stuff.AddConstant(std::move(mainCode));
    
        //std::ofstream output2("P:\\outOpt.sqfa", std::ofstream::binary);
        //ScriptSerializer::compiledToHumanReadable(stuff, output2);
        //output2.flush();
    } else {
        ASTToInstructions(stuff, temp, mainCode.code, ast);
        mainCode.contentString = stuff.AddConstant(std::move(*preprocessedScript));
        stuff.codeIndex = stuff.AddConstant(std::move(mainCode));
    }

   



   


    //auto outputPath2 = file.parent_path() / (file.stem().string() + ".sqfa");
    //std::ofstream output2(outputPath2, std::ofstream::binary);
    //std::ofstream output2("P:\\outOrig.sqfa", std::ofstream::binary);
    //ScriptSerializer::compiledToHumanReadable(stuff, output2);
    //output2.flush();
    return stuff;
}

void ScriptCompiler::ASTToInstructions(CompiledCodeData& output, CompileTempData& temp, std::vector<ScriptInstruction>& instructions, const astnode& node) const {
    auto getFileIndex = [&](const std::string& filename) -> uint8_t
    {
        auto found = temp.fileLoc.find(filename);
        if (found != temp.fileLoc.end())
            return found->second;
        auto index = static_cast<uint8_t>(output.fileNames.size());
        output.fileNames.emplace_back(filename);
        temp.fileLoc.insert({ filename, index });
        return index;
    };
    //#TODO get constant index and keep sets of bool/float/string constants

    auto nodeType = node.kind;
    switch (nodeType) {

    case sqf::parser::sqf::bison::astkind::ASSIGNMENT: {
        auto varname = std::string(node.children[0].token.contents);
        //need value on stack first
        ASTToInstructions(output, temp, instructions, node.children[1]);
        std::transform(varname.begin(), varname.end(), varname.begin(), ::tolower);
        instructions.emplace_back(ScriptInstruction{
            nodeType == sqf::parser::sqf::bison::astkind::ASSIGNMENT ?
            InstructionType::assignTo
            :
            InstructionType::assignToLocal
            , node.token.offset, getFileIndex(*node.token.path), node.token.line, varname });
    } break;
    case sqf::parser::sqf::bison::astkind::ASSIGNMENT_LOCAL: {
        auto varname = std::string(node.token.contents);
        //need value on stack first
        ASTToInstructions(output, temp, instructions, node.children[0]);
        std::transform(varname.begin(), varname.end(), varname.begin(), ::tolower);
        instructions.emplace_back(ScriptInstruction{
            nodeType == sqf::parser::sqf::bison::astkind::ASSIGNMENT ?
            InstructionType::assignTo
            :
            InstructionType::assignToLocal
            , node.token.offset, getFileIndex(*node.token.path), node.token.line, varname });
    } break;
    case sqf::parser::sqf::bison::astkind::EXP0:
    case sqf::parser::sqf::bison::astkind::EXP1:
    case sqf::parser::sqf::bison::astkind::EXP2:
    case sqf::parser::sqf::bison::astkind::EXP3:
        //number
        //binaryop
        //number
    case sqf::parser::sqf::bison::astkind::EXP4:
        //unary left arg
        //binary command
        //code on right
    case sqf::parser::sqf::bison::astkind::EXP5:
    case sqf::parser::sqf::bison::astkind::EXP6:
        //constant
        //binaryop
        //unaryop
    case sqf::parser::sqf::bison::astkind::EXP7:
    case sqf::parser::sqf::bison::astkind::EXP8:
    case sqf::parser::sqf::bison::astkind::EXP9: {

        //get left arg on stack
        ASTToInstructions(output, temp, instructions, node.children[0]);
        //get right arg on stack
        ASTToInstructions(output, temp, instructions, node.children[1]);
        //push binary op
        auto name = std::string(node.token.contents);
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        instructions.emplace_back(ScriptInstruction{ InstructionType::callBinary, node.token.offset, getFileIndex(*node.token.path), node.token.line, name });

        break;
    }
    case sqf::parser::sqf::bison::astkind::EXPN: {
        //push nular op
        auto name = std::string(node.token.contents);
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        instructions.emplace_back(ScriptInstruction{ InstructionType::callNular, node.token.offset, getFileIndex(*node.token.path), node.token.line, name });
        break;
    }
    case sqf::parser::sqf::bison::astkind::EXPU: {
        //unary operator
        //right arg

        //get right arg on stack
        ASTToInstructions(output, temp, instructions, node.children[0]);
        //push unary op
        auto name = std::string(node.token.contents);
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        instructions.emplace_back(ScriptInstruction{ InstructionType::callUnary, node.token.offset, getFileIndex(*node.token.path), node.token.line, name });
        break;
    }
    case sqf::parser::sqf::bison::astkind::NUMBER:
    case sqf::parser::sqf::bison::astkind::HEXNUMBER: {
        ScriptConstant newConst;

        float val;
        auto res = 
            (nodeType == sqf::parser::sqf::bison::astkind::HEXNUMBER) ?
            std::from_chars(node.token.contents.data()+2, node.token.contents.data() + node.token.contents.size(), val, std::chars_format::hex)
            :            
            std::from_chars(node.token.contents.data(), node.token.contents.data() + node.token.contents.size(), val);
        if (res.ec == std::errc::invalid_argument) {
            throw std::runtime_error("invalid scalar at: " + *node.token.path + ":" + std::to_string(node.token.line));
        }
        else if (res.ec == std::errc::result_out_of_range) {
            throw std::runtime_error("scalar out of range at: " + *node.token.path + ":" + std::to_string(node.token.line));
        }
        newConst = val;
        auto index = output.AddConstant(std::move(newConst));

        instructions.emplace_back(ScriptInstruction{ InstructionType::push, node.token.offset, getFileIndex(*node.token.path), node.token.line, index });
        break;
    }
    case sqf::parser::sqf::bison::astkind::IDENT: {
        //getvariable
        auto varname = std::string(node.token.contents);
        std::transform(varname.begin(), varname.end(), varname.begin(), ::tolower);
        instructions.emplace_back(ScriptInstruction{ InstructionType::getVariable, node.token.offset, getFileIndex(*node.token.path), node.token.line, varname });
        break;
    }
    case sqf::parser::sqf::bison::astkind::STRING: {
        ScriptConstant newConst;
        newConst = ::sqf::types::d_string::from_sqf(std::string(node.token.contents));
        auto index = output.AddConstant(std::move(newConst));

        instructions.emplace_back(ScriptInstruction{ InstructionType::push, node.token.offset, getFileIndex(*node.token.path), node.token.line, index });
        break;
    }

    case sqf::parser::sqf::bison::astkind::BOOLEAN_TRUE: {
        ScriptConstant newConst;
        newConst = true;
        auto index = output.AddConstant(std::move(newConst));

        instructions.emplace_back(ScriptInstruction{ InstructionType::push, node.token.offset, getFileIndex(*node.token.path), node.token.line, index });
        break;
    }

    case sqf::parser::sqf::bison::astkind::BOOLEAN_FALSE: {
        ScriptConstant newConst;
        //::sqf::types::d_string::from_sqf(std::string(node.token.contents))
        newConst = false;
        auto index = output.AddConstant(std::move(newConst));

        instructions.emplace_back(ScriptInstruction{ InstructionType::push, node.token.offset, getFileIndex(*node.token.path), node.token.line, index });
        break;
    }
    case sqf::parser::sqf::bison::astkind::CODE: {
        ScriptConstant newConst;
        std::vector<ScriptInstruction> instr;
        for (auto& it : node.children) {
            instr.emplace_back(ScriptInstruction{ InstructionType::endStatement, node.token.offset, 0, 0 });
            ASTToInstructions(output, temp, instr, it);
        }

        auto codeEnd = node.token.offset;

        if (!node.children.empty()) {

            auto* statements = &node.children[0];
            if (statements->kind != sqf::parser::sqf::bison::astkind::STATEMENTS)
                __debugbreak();

            size_t lastToken = 0;
            std::vector<std::vector<astnode>::const_iterator> lastChildren;

            while (!statements->children.empty()) {
                auto& lastChild = (statements->children.end() - 1);

                lastToken = lastChild->token.offset + lastChild->token.contents.size();
                statements = lastChild._Ptr;
                lastChildren.emplace_back(lastChild);
            }

            // we also need to travel the full way back. Just finding next } is not sufficient, we may be multiple code levels deep


            auto endData = node.token.contents.data() + (lastToken - node.token.offset);

            std::reverse(lastChildren.begin(), lastChildren.end());

            for (auto& it : lastChildren) {
                switch (it->kind) {
                case sqf::parser::sqf::bison::astkind::CODE:
                    // find ending }
                    while (*endData && *endData != '}') {
                        ++endData;
                        ++lastToken;
                    }
                    // after ending }
                    ++endData; ++lastToken;
                    break;
                case sqf::parser::sqf::bison::astkind::ARRAY:
                    // find ending ]
                    while (*endData && *endData != ']') {
                        ++endData; ++lastToken;
                    }
                    // after ending ]
                    ++endData; ++lastToken;
                    break;
                default:;
                }
            }


            while (*endData && *endData != '}') {
                ++endData;
                ++lastToken;
            }
            // we stop BEFORE ending }, we only want the inner code.

            codeEnd = lastToken;
            newConst = ScriptCodePiece(std::move(instr), codeEnd - node.token.offset - 1, node.token.offset + 1);
        } else {
            newConst = ScriptCodePiece(std::move(instr), 0, node.token.offset + 1);
        }

        auto index = output.AddConstant(std::move(newConst));

        instructions.emplace_back(ScriptInstruction{ InstructionType::push, node.token.offset, getFileIndex(*node.token.path), node.token.line, index });
        break;
    }
    case sqf::parser::sqf::bison::astkind::ARRAY: {
        //push elements first
        for (auto& it : node.children)
            ASTToInstructions(output, temp, instructions, it);

        //#TODO can already check here if all arguments are const and push a const
        //AST is const function. That just checks whether the whole tree only contains constants


        //make array instruction
        //array instruction has size as argument
        instructions.emplace_back(ScriptInstruction{ InstructionType::makeArray, node.token.offset, getFileIndex(*node.token.path), node.token.line, (uint16_t)node.children.size() });

        break;
    }
                                              //case sqf::parser::sqf::bison::astkind::NA:
                                              //case sqf::parser::sqf::bison::astkind::SQF:
                                              //case sqf::parser::sqf::bison::astkind::STATEMENT:
                                              //case sqf::parser::sqf::bison::astkind::BRACKETS:
                                              //    for (auto& it : node.children)
                                              //        stuffAST(output, instructions, it);
    default:
        for (size_t i = 0; i < node.children.size(); i++) {
            if (i != 0 || instructions.empty()) //end statement
                instructions.emplace_back(ScriptInstruction{ InstructionType::endStatement, node.token.offset, 0, 0 });
            ASTToInstructions(output, temp, instructions, node.children[i]);
        }
    }
}

ScriptConstantArray ScriptCompiler::ASTParseArray(CompiledCodeData& output, CompileTempData& temp, const OptimizerModuleBase::Node& node) const {
    ScriptConstantArray newConst;
    for (auto& it : node.children) {
        if (it.value.index() == 0) {//is code
            ScriptCodePiece codeConst;
            std::vector<ScriptInstruction> instr;
            for (auto& codeIt : it.children) {

                //std::stringstream dbg;
                //codeIt.dumpTree(dbg, 0);
                //auto res = dbg.str();

                instr.emplace_back(ScriptInstruction{ InstructionType::endStatement, node.offset, 0, 0 });
                ASTToInstructions(output, temp, instr, codeIt);
            }
            codeConst.contentString = std::get<ScriptCodePiece>(it.value).contentString;
            codeConst.code = std::move(instr);
            newConst.content.emplace_back(std::move(codeConst));
        } else if (it.value.index() == 4) {//array
            newConst.content.emplace_back(ASTParseArray(output, temp, it));
        } else {
            newConst.content.emplace_back(it.value);
        }
    }
    return newConst;
}

void ScriptCompiler::ASTToInstructions(CompiledCodeData& output, CompileTempData& temp,
    std::vector<ScriptInstruction>& instructions, const OptimizerModuleBase::Node& node) const {

    auto getFileIndex = [&](const std::string & filename) -> uint8_t
    {
        auto found = temp.fileLoc.find(filename);
        if (found != temp.fileLoc.end())
            return found->second;
        auto index = static_cast<uint8_t>(output.fileNames.size());
        output.fileNames.emplace_back(filename);
        temp.fileLoc.insert({ filename, index });
        return index;
    };


    switch (node.type) {
        case InstructionType::push: {
            switch (getConstantType(node.value)) {
                case ConstantType::code: {//Code
                    ScriptConstant newConst;
                    std::vector<ScriptInstruction> instr;
                    for (auto& it : node.children) {
                        instr.emplace_back(ScriptInstruction{ InstructionType::endStatement, node.offset, 0, 0 });
                        ASTToInstructions(output, temp, instr, it);
                    }

                    newConst = node.value;
                    std::get<ScriptCodePiece>(newConst).code = std::move(instr);
                    auto index = output.AddConstant(std::move(newConst));

                    instructions.emplace_back(ScriptInstruction{ InstructionType::push, node.offset, getFileIndex(node.file), node.line, index });
                } break;
                case ConstantType::string: 
                case ConstantType::scalar:
                case ConstantType::boolean:
                case ConstantType::nularCommand:                
                {
                    auto index = output.AddConstant(node.value);
                    instructions.emplace_back(ScriptInstruction{ InstructionType::push, node.offset, getFileIndex(node.file), node.line, index });
                } break;            
                case ConstantType::array: {//Array
                    auto index = output.AddConstant(ASTParseArray(output, temp, node));

                    instructions.emplace_back(ScriptInstruction{ InstructionType::push, node.offset, getFileIndex(node.file), node.line, index });
                } break;
                default: __debugbreak();
            }

        }break;
        case InstructionType::callUnary: {
            
            ASTToInstructions(output, temp, instructions, node.children[0]);
            //push unary op
            auto name = std::get<STRINGTYPE>(node.value);
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);
            instructions.emplace_back(ScriptInstruction{ InstructionType::callUnary, node.offset, getFileIndex(node.file), node.line, name });

        } break;
        case InstructionType::callBinary: {


            //get left arg on stack
            ASTToInstructions(output, temp, instructions, node.children[0]);
            //get right arg on stack
            ASTToInstructions(output, temp, instructions, node.children[1]);
            //push binary op
            auto name = std::get<STRINGTYPE>(node.value);
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);
            instructions.emplace_back(ScriptInstruction{ InstructionType::callBinary, node.offset, getFileIndex(node.file), node.line, name });



        } break;
        case InstructionType::callNular: {
            auto name = std::get<STRINGTYPE>(node.value);
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);
            instructions.emplace_back(ScriptInstruction{ InstructionType::callNular, node.offset, getFileIndex(node.file), node.line, name });

        } break;
        case InstructionType::assignTo:
        case InstructionType::assignToLocal: {
            
            auto varname = std::get<STRINGTYPE>(node.value);
            //need value on stack first
            ASTToInstructions(output, temp, instructions, node.children[0]);
            std::transform(varname.begin(), varname.end(), varname.begin(), ::tolower);
            instructions.emplace_back(ScriptInstruction{ node.type, node.offset, getFileIndex(node.file), node.line, varname });

        } break;
        case InstructionType::getVariable: {
            auto varname = std::get<STRINGTYPE>(node.value);
            std::transform(varname.begin(), varname.end(), varname.begin(), ::tolower);
            instructions.emplace_back(ScriptInstruction{ InstructionType::getVariable, node.offset, getFileIndex(node.file), node.line, varname });
        } break;
        case InstructionType::makeArray: {
            for (auto& it : node.children)
                ASTToInstructions(output, temp, instructions, it);

            //#TODO can already check here if all arguments are const and push a const

            instructions.emplace_back(ScriptInstruction{ InstructionType::makeArray, node.offset, getFileIndex(node.file), node.line, (uint16_t)node.children.size() });
        } break;


        case InstructionType::endStatement: {
            for (size_t i = 0; i < node.children.size(); i++) {
                if (i != 0 || instructions.empty()) //end statement
                    instructions.emplace_back(ScriptInstruction{ InstructionType::endStatement, node.offset, 0, 0 });
                ASTToInstructions(output, temp, instructions, node.children[i]);
            }

        } break;
    }
}

CompiledCodeData ScriptCompiler::compileScriptLua(std::filesystem::path physicalPath, std::filesystem::path virtualPath, OptimizerModuleLua& optimizer, const std::filesystem::path& outputFile)
{
    std::ifstream inputFile(physicalPath);

    auto filesize = std::filesystem::file_size(physicalPath);
    if (filesize == 0) // uh. oki
        return CompiledCodeData();

    std::string scriptCode;
    scriptCode.resize(filesize);
    inputFile.read(scriptCode.data(), filesize);

    if (
        static_cast<unsigned char>(scriptCode[0]) == 0xef &&
        static_cast<unsigned char>(scriptCode[1]) == 0xbb &&
        static_cast<unsigned char>(scriptCode[2]) == 0xbf
        ) {
        scriptCode.erase(0, 3);
    }

    auto preprocessedScript = vm->parser_preprocessor().preprocess(*vm, scriptCode, sqf::runtime::fileio::pathinfo(physicalPath.string(), virtualPath.string()));
    if (!preprocessedScript) {
        //__debugbreak();
        return CompiledCodeData();
    }
    bool errorflag = false;


    sqf::parser::sqf::parser sqfParser(vmlogger);
    sqf::parser::sqf::bison::astnode ast;
    sqf::parser::sqf::tokenizer tokenizer(preprocessedScript->begin(), preprocessedScript->end(), virtualPath.string());
    auto errflag = !sqfParser.get_tree(*vm, tokenizer, &ast);
    if (errflag || ast.children.empty()) {
        //__debugbreak();
        return CompiledCodeData();
    }

    //print_navigate_ast(&std::cout, ast, sqf::parse::sqf::astkindname);

    CompiledCodeData stuff;
    CompileTempData temp;
    ScriptCodePiece mainCode;


    auto& statementsNode = ast.children[0];
    if (statementsNode.kind != sqf::parser::sqf::bison::astkind::STATEMENTS)
        __debugbreak();

    auto node = OptimizerModuleBase::nodeFromAST(statementsNode);

    Optimizer opt;
    optimizer.optimizeLua(node);

    ASTToInstructions(stuff, temp, mainCode.code, node);
    mainCode.contentString = stuff.AddConstant(std::move(*preprocessedScript));
    stuff.codeIndex = stuff.AddConstant(std::move(mainCode));

    std::ofstream output2(outputFile, std::ofstream::binary);
    ScriptSerializer::compiledToHumanReadable(stuff, output2);
    output2.flush();

    return stuff;
}

void ScriptCompiler::initIncludePaths(const std::vector<std::filesystem::path>& paths) {
    for (auto& includefolder : paths) {

        if (includefolder.string().length() == 3 && includefolder.string().substr(1) == ":/") {
            // pdrive

            //const std::filesystem::path ignoreGit(".git");
            //const std::filesystem::path ignoreSvn(".svn");
            //
            ////recursively search for pboprefix
            //for (auto i = std::filesystem::directory_iterator(includefolder);
            //    i != std::filesystem::directory_iterator();
            //    ++i)
            //{
            //    if (!i->is_directory()) continue;
            //
            //    if ((i->path().filename() == ignoreGit || i->path().filename() == ignoreSvn))
            //    {
            //        continue;
            //    }
            //    
            //
            //    vm->fileio().add_mapping(i->path().string(), i->path().filename().string());
            //}
            auto str = includefolder.lexically_normal().string();
            if (str.back() == std::filesystem::path::preferred_separator)
                str.pop_back();
            vm->fileio().add_mapping(str, "/");
        } else {
            vm->fileio().add_mapping_auto(includefolder.string());
        }

       
    }
}

void ScriptCompiler::addMacro(sqf::runtime::parser::macro macro) {
    vm->parser_preprocessor().push_back(macro);
}

void ScriptCompiler::addPragma(sqf::runtime::parser::pragma pragma) {
    vm->parser_preprocessor().push_back(pragma);
}

std::string ScriptCompiler::preprocessScript(std::filesystem::path physicalPath, std::filesystem::path virtualPath) {
    std::ifstream inputFile(physicalPath);

    auto filesize = std::filesystem::file_size(physicalPath);
    if (filesize == 0) // uh. oki
        return "";

    std::string scriptCode;
    scriptCode.resize(filesize);
    inputFile.read(scriptCode.data(), filesize);

    if (
        static_cast<unsigned char>(scriptCode[0]) == 0xef &&
        static_cast<unsigned char>(scriptCode[1]) == 0xbb &&
        static_cast<unsigned char>(scriptCode[2]) == 0xbf
        ) {
        scriptCode.erase(0, 3);
    }

    // #OPTION force all script files to contain a include
    //if (scriptCode.find("script_component.hpp\"") == std::string::npos) {
    //    throw std::domain_error("no include");
    //}
    try {
        auto preprocessedScript = vm->parser_preprocessor().preprocess(*vm, scriptCode, sqf::runtime::fileio::pathinfo(physicalPath.string(), virtualPath.string()));
        if (!preprocessedScript) {
            //__debugbreak();
            return "";
        }

        return std::string(preprocessedScript->begin(), preprocessedScript->end());
    } catch (std::exception ex) {
        __debugbreak();
    }

    return "";
}
