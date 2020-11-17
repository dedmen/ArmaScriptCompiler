#include "scriptCompiler.hpp"
#include "commandList.hpp"
#include <charconv>
#include <fstream>

#include <virtualmachine.h>
#include <parsepreprocessor.h>
#include <parsesqf.h>
#include <callstack.h>
#include <commandmap.h>
#include <iostream>
#include <mutex>
#include "stringdata.h"
#include "optimizer/optimizerModuleBase.hpp"
#include "optimizer/optimizer.h"
#include "scriptSerializer.hpp"
std::once_flag commandMapInitFlag;

ScriptCompiler::ScriptCompiler(const std::vector<std::filesystem::path>& includePaths) {
    vm = std::make_unique<sqf::virtualmachine>();
    vm->err(&std::cout);
    vm->wrn(&std::cout);

    std::call_once(commandMapInitFlag, []() {
        CommandList::init();
        //sqf::commandmap::get().init();
    });
    initIncludePaths(includePaths);
}

CompiledCodeData ScriptCompiler::compileScript(std::filesystem::path file) {
    std::ifstream inputFile(file);
    
    auto filesize = std::filesystem::file_size(file);
    if (filesize == 0) // uh. oki
        return CompiledCodeData();

    std::string scriptCode;
    scriptCode.resize(filesize);
    inputFile.read(scriptCode.data(), filesize);
    bool errflag = false;


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


    auto preprocessedScript = sqf::parse::preprocessor::parse(vm.get(), scriptCode, errflag, file.string());
    vm->err_buffprint();
    vm->wrn_buffprint();
    if (errflag) {
        __debugbreak();
        vm->err_clear();
        return CompiledCodeData();
    }
    bool errorflag = false;
    auto ast = vm->parse_sqf_cst(preprocessedScript, errorflag);
    if (errorflag) __debugbreak();
    //print_navigate_ast(&std::cout, ast, sqf::parse::sqf::astkindname);

    CompiledCodeData stuff;
    CompileTempData temp;
    ScriptCodePiece mainCode;


    if (true) {
        auto node = OptimizerModuleBase::nodeFromAST(ast);
    
        //std::ofstream nodeo("P:\\node.txt");
        //node.dumpTree(nodeo, 0);
        //nodeo.close();
    
        auto res = node.bottomUpFlatten();
    
        Optimizer opt;
    
        opt.optimize(node);
    
        //std::ofstream nodeop("P:\\nodeOpt.txt");
        //node.dumpTree(nodeop, 0);
        //nodeop.close();
    
        ASTToInstructions(stuff, temp, mainCode.code, node);
        mainCode.contentString = stuff.constants.size();
        stuff.constants.emplace_back(std::move(preprocessedScript));
        stuff.codeIndex = stuff.constants.size();
        stuff.constants.emplace_back(std::move(mainCode));
    
        //std::ofstream output2("P:\\outOpt.sqfa", std::ofstream::binary);
        //ScriptSerializer::compiledToHumanReadable(stuff, output2);
        //output2.flush();
    } else {
        ASTToInstructions(stuff, temp, mainCode.code, ast);
        mainCode.contentString = stuff.constants.size();
        stuff.constants.emplace_back(std::move(preprocessedScript));
        stuff.codeIndex = stuff.constants.size();
        stuff.constants.emplace_back(std::move(mainCode));
    }

   



   


    //auto outputPath2 = file.parent_path() / (file.stem().string() + ".sqfa");
    //std::ofstream output2(outputPath2, std::ofstream::binary);
    std::ofstream output2("P:\\outOrig.sqfa", std::ofstream::binary);
    ScriptSerializer::compiledToHumanReadable(stuff, output2);
    output2.flush();
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

    auto nodeType = static_cast<sqf::parse::sqf::sqfasttypes::sqfasttypes>(node.kind);
    switch (nodeType) {

    case sqf::parse::sqf::sqfasttypes::ASSIGNMENT:
    case sqf::parse::sqf::sqfasttypes::ASSIGNMENTLOCAL: {
        auto varname = node.children[0].content;
        //need value on stack first
        ASTToInstructions(output, temp, instructions, node.children[1]);
        std::transform(varname.begin(), varname.end(), varname.begin(), ::tolower);
        instructions.emplace_back(ScriptInstruction{
            nodeType == sqf::parse::sqf::sqfasttypes::ASSIGNMENT ?
            InstructionType::assignTo
            :
            InstructionType::assignToLocal
            , node.offset, getFileIndex(node.file), node.line, varname });
    }
                                                        break;
    case sqf::parse::sqf::sqfasttypes::BEXP1:
    case sqf::parse::sqf::sqfasttypes::BEXP2:
    case sqf::parse::sqf::sqfasttypes::BEXP3:
        //number
        //binaryop
        //number
    case sqf::parse::sqf::sqfasttypes::BEXP4:
        //unary left arg
        //binary command
        //code on right
    case sqf::parse::sqf::sqfasttypes::BEXP5:
    case sqf::parse::sqf::sqfasttypes::BEXP6:
        //constant
        //binaryop
        //unaryop
    case sqf::parse::sqf::sqfasttypes::BEXP7:
    case sqf::parse::sqf::sqfasttypes::BEXP8:
    case sqf::parse::sqf::sqfasttypes::BEXP9:
    case sqf::parse::sqf::sqfasttypes::BEXP10:
    case sqf::parse::sqf::sqfasttypes::BINARYEXPRESSION: {

        //get left arg on stack
        ASTToInstructions(output, temp, instructions, node.children[0]);
        //get right arg on stack
        ASTToInstructions(output, temp, instructions, node.children[2]);
        //push binary op
        auto name = node.children[1].content;
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        instructions.emplace_back(ScriptInstruction{ InstructionType::callBinary, node.offset, getFileIndex(node.file), node.line, name });

        break;
    }
    case sqf::parse::sqf::sqfasttypes::BINARYOP: __debugbreak(); break;

    case sqf::parse::sqf::sqfasttypes::PRIMARYEXPRESSION: __debugbreak(); break;
    case sqf::parse::sqf::sqfasttypes::NULAROP: {
        //push nular op
        auto name = node.content;
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        if (name == "true"sv) {
            ScriptConstant newConst;
            newConst = true;
            auto index = output.constants.size();
            output.constants.emplace_back(std::move(newConst));
            instructions.emplace_back(ScriptInstruction{ InstructionType::push, node.offset, getFileIndex(node.file), node.line, index });
        } else if (name == "false"sv) {
            ScriptConstant newConst;
            newConst = false;
            auto index = output.constants.size();
            output.constants.emplace_back(std::move(newConst));
            instructions.emplace_back(ScriptInstruction{ InstructionType::push, node.offset, getFileIndex(node.file), node.line, index });
        } else
            instructions.emplace_back(ScriptInstruction{ InstructionType::callNular, node.offset, getFileIndex(node.file), node.line, name });
        break;
    }
    case sqf::parse::sqf::sqfasttypes::UNARYEXPRESSION: {
        //unary operator
        //right arg

        //get right arg on stack
        ASTToInstructions(output, temp, instructions, node.children[1]);
        //push unary op
        auto name = node.children[0].content;
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        instructions.emplace_back(ScriptInstruction{ InstructionType::callUnary, node.offset, getFileIndex(node.file), node.line, name });
        break;
    }
    case sqf::parse::sqf::sqfasttypes::UNARYOP: __debugbreak(); break;
    case sqf::parse::sqf::sqfasttypes::NUMBER:
    case sqf::parse::sqf::sqfasttypes::HEXNUMBER: {
        ScriptConstant newConst;

        float val;
        auto res = 
            (nodeType == sqf::parse::sqf::sqfasttypes::HEXNUMBER) ?
            std::from_chars(node.content.data()+2, node.content.data() + node.content.size(), val, std::chars_format::hex)
            :            
            std::from_chars(node.content.data(), node.content.data() + node.content.size(), val);
        if (res.ec == std::errc::invalid_argument) {
            throw std::runtime_error("invalid scalar at: " + node.file + ":" + std::to_string(node.line));
        }
        else if (res.ec == std::errc::result_out_of_range) {
            throw std::runtime_error("scalar out of range at: " + node.file + ":" + std::to_string(node.line));
        }
        newConst = val;
        auto index = output.constants.size();
        output.constants.emplace_back(std::move(newConst));

        instructions.emplace_back(ScriptInstruction{ InstructionType::push, node.offset, getFileIndex(node.file), node.line, index });
        break;
    }
    case sqf::parse::sqf::sqfasttypes::VARIABLE: {
        //getvariable
        auto varname = node.content;
        std::transform(varname.begin(), varname.end(), varname.begin(), ::tolower);
        instructions.emplace_back(ScriptInstruction{ InstructionType::getVariable, node.offset, getFileIndex(node.file), node.line, varname });
        break;
    }
    case sqf::parse::sqf::sqfasttypes::STRING: {
        ScriptConstant newConst;
        newConst = sqf::stringdata::parse_from_sqf(node.content);
        auto index = output.constants.size();
        output.constants.emplace_back(std::move(newConst));

        instructions.emplace_back(ScriptInstruction{ InstructionType::push, node.offset, getFileIndex(node.file), node.line, index });
        break;
    }
    case sqf::parse::sqf::sqfasttypes::CODE: {
        ScriptConstant newConst;
        std::vector<ScriptInstruction> instr;
        for (auto& it : node.children) {
            instr.emplace_back(ScriptInstruction{ InstructionType::endStatement, node.offset, 0, 0 });
            ASTToInstructions(output, temp, instr, it);
        }

        newConst = ScriptCodePiece(std::move(instr), node.length-2, node.offset+1 );
        //#TODO duplicate detection
        auto index = output.constants.size();
        output.constants.emplace_back(std::move(newConst));

        instructions.emplace_back(ScriptInstruction{ InstructionType::push, node.offset, getFileIndex(node.file), node.line, index });
        break;
    }
    case sqf::parse::sqf::sqfasttypes::ARRAY: {
        //push elements first
        for (auto& it : node.children)
            ASTToInstructions(output, temp, instructions, it);

        //#TODO can already check here if all arguments are const and push a const
        //AST is const function. That just checks whether the whole tree only contains constants


        //make array instruction
        //array instruction has size as argument
        instructions.emplace_back(ScriptInstruction{ InstructionType::makeArray, node.offset, getFileIndex(node.file), node.line, node.children.size() });

        break;
    }
                                              //case sqf::parse::sqf::sqfasttypes::NA:
                                              //case sqf::parse::sqf::sqfasttypes::SQF:
                                              //case sqf::parse::sqf::sqfasttypes::STATEMENT:
                                              //case sqf::parse::sqf::sqfasttypes::BRACKETS:
                                              //    for (auto& it : node.children)
                                              //        stuffAST(output, instructions, it);
    default:
        for (size_t i = 0; i < node.children.size(); i++) {
            if (i != 0 || instructions.empty()) //end statement
                instructions.emplace_back(ScriptInstruction{ InstructionType::endStatement, node.offset, 0, 0 });
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
            switch (node.value.index()) {
                case 0: {//Code
                    ScriptConstant newConst;
                    std::vector<ScriptInstruction> instr;
                    for (auto& it : node.children) {
                        instr.emplace_back(ScriptInstruction{ InstructionType::endStatement, node.offset, 0, 0 });
                        ASTToInstructions(output, temp, instr, it);
                    }

                    newConst = node.value;
                    std::get<ScriptCodePiece>(newConst).code = std::move(instr);
                    //#TODO duplicate detection
                    auto index = output.constants.size();
                    output.constants.emplace_back(std::move(newConst));

                    instructions.emplace_back(ScriptInstruction{ InstructionType::push, node.offset, getFileIndex(node.file), node.line, index });
                } break;
                case 1: {//String
                    auto index = output.constants.size();
                    output.constants.emplace_back(node.value);
                    instructions.emplace_back(ScriptInstruction{ InstructionType::push, node.offset, getFileIndex(node.file), node.line, index });
                } break;
                case 2: {//Number
                    auto index = output.constants.size();
                    output.constants.emplace_back(node.value);
                    instructions.emplace_back(ScriptInstruction{ InstructionType::push, node.offset, getFileIndex(node.file), node.line, index });
                } break;
                case 3: {//Bool
                    auto index = output.constants.size();
                    output.constants.emplace_back(node.value);
                    instructions.emplace_back(ScriptInstruction{ InstructionType::push, node.offset, getFileIndex(node.file), node.line, index });
                } break;
                case 4: {//Array
                    auto value = ASTParseArray(output, temp, node);
                    auto index = output.constants.size();
                    output.constants.emplace_back(value);

                    instructions.emplace_back(ScriptInstruction{ InstructionType::push, node.offset, getFileIndex(node.file), node.line, index });
                } break;
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

            instructions.emplace_back(ScriptInstruction{ InstructionType::makeArray, node.offset, getFileIndex(node.file), node.line, node.children.size() });
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

void ScriptCompiler::initIncludePaths(const std::vector<std::filesystem::path>& paths) const {
    for (auto& includefolder : paths) {

        if (includefolder.string().length() == 3 && includefolder.string().substr(1) == ":/") {
            // pdrive

            const std::filesystem::path ignoreGit(".git");
            const std::filesystem::path ignoreSvn(".svn");

            //recursively search for pboprefix
            for (auto i = std::filesystem::directory_iterator(includefolder);
                i != std::filesystem::directory_iterator();
                ++i)
            {
                if (!i->is_directory()) continue;

                if ((i->path().filename() == ignoreGit || i->path().filename() == ignoreSvn))
                {
                    continue;
                }
                

                vm->get_filesystem().add_mapping(i->path().filename().string(), i->path().string());
            }
        } else {
            vm->get_filesystem().add_mapping_auto(includefolder.string());
        }

       
    }
}
