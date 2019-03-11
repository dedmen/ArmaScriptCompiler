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
    std::string scriptCode;
    scriptCode.resize(filesize);
    inputFile.read(scriptCode.data(), filesize);
    bool errflag = false;

    if (scriptCode.find("script_component") == std::string::npos) {
        throw std::domain_error("no include");
    }


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
    //print_navigate_ast(std::cout, ast, sqf::parse::sqf::astkindname);

    CompiledCodeData stuff;
    CompileTempData temp;
    ASTToInstructions(stuff, temp, stuff.instructions, ast);

    //std::shared_ptr<sqf::callstack> cs = std::make_shared<sqf::callstack>(vm.missionnamespace());
    //vm.parse_sqf(vm.stack(), preprocessedScript, cs, "I:\\ACE3\\addons\\advanced_ballistics\\functions\\fnc_readWeaponDataFromConfig.sqf");
    //std::ofstream out("P:\\out.sqfa");
    //printSQFASM(stuff, out);
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

    auto nodeType = static_cast<sqf::parse::sqf::sqfasttypes::sqfasttypes>(node.kind);
    switch (nodeType) {

    case sqf::parse::sqf::sqfasttypes::ASSIGNMENT:
    case sqf::parse::sqf::sqfasttypes::ASSIGNMENTLOCAL: {
        auto varname = node.children[0].content;
        //need value on stack first
        ASTToInstructions(output, temp, instructions, node.children[1]);

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
        instructions.emplace_back(ScriptInstruction{ InstructionType::callBinary, node.offset, getFileIndex(node.file), node.line, node.children[1].content });

        break;
    }
    case sqf::parse::sqf::sqfasttypes::BINARYOP: __debugbreak(); break;

    case sqf::parse::sqf::sqfasttypes::PRIMARYEXPRESSION: __debugbreak(); break;
    case sqf::parse::sqf::sqfasttypes::NULAROP: {
        //push nular op
        instructions.emplace_back(ScriptInstruction{ InstructionType::callNular, node.offset, getFileIndex(node.file), node.line, node.content });
        break;
    }
    case sqf::parse::sqf::sqfasttypes::UNARYEXPRESSION: {
        //unary operator
        //right arg

        //get right arg on stack
        ASTToInstructions(output, temp, instructions, node.children[1]);
        //push unary op
        instructions.emplace_back(ScriptInstruction{ InstructionType::callUnary, node.offset, getFileIndex(node.file), node.line, node.children[0].content });
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
        instructions.emplace_back(ScriptInstruction{ InstructionType::getVariable, node.offset, getFileIndex(node.file), node.line, varname });
        break;
    }
    case sqf::parse::sqf::sqfasttypes::STRING: {
        ScriptConstant newConst;
        newConst = node.content;
        auto index = output.constants.size();
        output.constants.emplace_back(std::move(newConst));

        instructions.emplace_back(ScriptInstruction{ InstructionType::push, node.offset, getFileIndex(node.file), node.line, index });
        break;
    }
    case sqf::parse::sqf::sqfasttypes::CODE: {

        ScriptConstant newConst;
        std::vector<ScriptInstruction> instr;
        if (!node.children.empty())
            ASTToInstructions(output, temp, instr, node.children[0]);
        newConst = instr;
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
            if (i != 0) //end statement
                instructions.emplace_back(ScriptInstruction{ InstructionType::endStatement, node.offset, 0, 0 });
            ASTToInstructions(output, temp, instructions, node.children[i]);
        }
    }


}

void ScriptCompiler::initIncludePaths(const std::vector<std::filesystem::path>& paths) const {
    for (auto& includefolder : paths) {
        vm->get_filesystem().add_mapping_auto(includefolder.string());
    }
}
