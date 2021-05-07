#include "optimizerModuleBase.hpp"
#include <algorithm>
#include <charconv>
#include <queue>
#include "runtime/d_string.h"
#include <parser/sqf/sqf_parser.hpp>

#include "parser/sqf/parser.tab.hh"


void OptimizerModuleBase::Node::dumpTree(std::ostream& output, size_t indent) const {
    for (int i = 0; i < indent; ++i) {
        output << " ";
    }
    output << instructionTypeToString(type) << ":" << line << "\n";

    for (auto& it : children) {
        it.dumpTree(output, indent + 1);
    }
}

bool OptimizerModuleBase::Node::buildConstState() {
    return false;
    //switch (type) {
    //
    //    case InstructionType::push: return constant = true;
    //    case InstructionType::assignTo: return false;
    //    case InstructionType::assignToLocal: return false;
    //    case InstructionType::getVariable: return false;
    //    case InstructionType::makeArray: return constant = true;
    //}
    //
    //if (type == InstructionType::push) {
    //    return constant = true;
    //}
    //
    //
    //
    //bool childrenConstant = std::all_of(children.begin(), children.end(), [](Node& it) {
    //        return it.buildConstState();
    //    });
    //
    //bool meIsConst = false;
    //
    //switch (type) {
    //
    //    case InstructionType::endStatement: return constant = childrenConstant;
    //    case InstructionType::callUnary: return constant = canUnaryBeConst.anyOf(*this, childrenConstant);
    //    case InstructionType::callBinary: return constant = canBinaryBeConst.anyOf(*this, childrenConstant);
    //    case InstructionType::callNular: return constant = canNularBeConst.anyOf(*this, childrenConstant);
    //}




}

bool OptimizerModuleBase::Node::areChildrenConstant() const {
    return std::all_of(children.begin(), children.end(), [](const Node& it) {
            return it.constant;
        });
}

std::vector<OptimizerModuleBase::Node*> OptimizerModuleBase::Node::bottomUpFlatten() {
    std::vector<std::vector<Node*>> result;
    std::queue<OptimizerModuleBase::Node*> myQueue;
    myQueue.push(this);
    size_t totalNodeCount = 0;
    int currentLevelNodeNum = 1;//used to record num of nodes in current level
    int nextLevelNodeNum = 0;//used to record num of nodes in next level
    std::vector<Node*> level;
    while (!myQueue.empty()) {
    
        OptimizerModuleBase::Node* temp = myQueue.front();
        myQueue.pop();
        level.push_back(temp);
        totalNodeCount++;
        currentLevelNodeNum--;
        for (auto& it : temp->children) {
            myQueue.push(&it);
            nextLevelNodeNum++;
        }
        if (currentLevelNodeNum == 0) {//if we have traversed current level, turn to next level
            result.emplace_back(std::move(level));//push the current level into result
            currentLevelNodeNum = nextLevelNodeNum;//assign next level node num to current
            nextLevelNodeNum = 0;//set next level num to 0
        }
    }
    std::reverse(result.begin(), result.end());
    std::vector<OptimizerModuleBase::Node*> endResult;
    endResult.reserve(totalNodeCount);
    for (auto& it : result)
        endResult.insert(endResult.end(), it.begin(), it.end());

    return endResult;
}

OptimizerModuleBase::Node OptimizerModuleBase::nodeFromAST(const astnode& input) {
    auto nodeType = input.kind;
    switch (nodeType) {

    case sqf::parser::sqf::bison::astkind::ASSIGNMENT: {
        Node newNode;

        newNode.type = nodeType == sqf::parser::sqf::bison::astkind::ASSIGNMENT ? InstructionType::assignTo : InstructionType::assignToLocal;
        newNode.file = *input.token.path;
        newNode.line = input.token.line;
        newNode.offset = input.token.offset;
        auto varname = std::string(input.children[0].token.contents);
        std::transform(varname.begin(), varname.end(), varname.begin(), ::tolower);
        newNode.value = std::string(varname);

        newNode.children.emplace_back(nodeFromAST(input.children[1]));
        return newNode;
    }
    case sqf::parser::sqf::bison::astkind::ASSIGNMENT_LOCAL: {
        Node newNode;

        newNode.type = nodeType == sqf::parser::sqf::bison::astkind::ASSIGNMENT ? InstructionType::assignTo : InstructionType::assignToLocal;
        newNode.file = *input.token.path;
        newNode.line = input.token.line;
        newNode.offset = input.token.offset;
        auto varname = std::string(input.token.contents);
        std::transform(varname.begin(), varname.end(), varname.begin(), ::tolower);
        newNode.value = std::string(varname);

        newNode.children.emplace_back(nodeFromAST(input.children[0]));
        return newNode;
    }
    case sqf::parser::sqf::bison::astkind::EXP0:
    case sqf::parser::sqf::bison::astkind::EXP1:
    case sqf::parser::sqf::bison::astkind::EXP2:
    case sqf::parser::sqf::bison::astkind::EXP3:
    case sqf::parser::sqf::bison::astkind::EXP4:
    case sqf::parser::sqf::bison::astkind::EXP5:
    case sqf::parser::sqf::bison::astkind::EXP6:
    case sqf::parser::sqf::bison::astkind::EXP7:
    case sqf::parser::sqf::bison::astkind::EXP8:
    case sqf::parser::sqf::bison::astkind::EXP9: {
        Node newNode;

        newNode.type = InstructionType::callBinary;
        newNode.file = *input.token.path;
        newNode.line = input.token.line;
        newNode.offset = input.token.offset;
        auto varname = std::string(input.token.contents);
        std::transform(varname.begin(), varname.end(), varname.begin(), ::tolower);
        newNode.value = std::string(varname);

        newNode.children.emplace_back(nodeFromAST(input.children[0]));
        newNode.children.emplace_back(nodeFromAST(input.children[1]));

        return newNode;
    }
    case sqf::parser::sqf::bison::astkind::EXPN: {
        Node newNode;

        newNode.type = InstructionType::callNular;
        newNode.file = *input.token.path;
        newNode.line = input.token.line;
        newNode.offset = input.token.offset;
        auto varname = std::string(input.token.contents);
        std::transform(varname.begin(), varname.end(), varname.begin(), ::tolower);
        newNode.value = std::string(varname);
        return newNode;
    }
    case sqf::parser::sqf::bison::astkind::EXPU: {
        Node newNode;

        newNode.type = InstructionType::callUnary;
        newNode.file = *input.token.path;
        newNode.line = input.token.line;
        newNode.offset = input.token.offset;
        auto varname = std::string(input.token.contents);
        std::transform(varname.begin(), varname.end(), varname.begin(), ::tolower);
        newNode.value = std::string(varname);
        auto subEl = nodeFromAST(input.children[0]);

        newNode.children.emplace_back(std::move(subEl));
        return newNode;
    }
    case sqf::parser::sqf::bison::astkind::NUMBER:
    case sqf::parser::sqf::bison::astkind::HEXNUMBER: {
        float val;
        auto res =
            (nodeType == sqf::parser::sqf::bison::astkind::HEXNUMBER) ?
            std::from_chars(input.token.contents.data() + 2, input.token.contents.data() + input.token.contents.size(), val, std::chars_format::hex)
            :
            std::from_chars(input.token.contents.data(), input.token.contents.data() + input.token.contents.size(), val);
        if (res.ec == std::errc::invalid_argument) {
            throw std::runtime_error("invalid scalar at: " + *input.token.path + ":" + std::to_string(input.token.line));
        }
        else if (res.ec == std::errc::result_out_of_range) {
            throw std::runtime_error("scalar out of range at: " + *input.token.path + ":" + std::to_string(input.token.line));
        }


        Node newNode;

        newNode.type = InstructionType::push;
        newNode.file = *input.token.path;
        newNode.line = input.token.line;
        newNode.offset = input.token.offset;
        newNode.value = val;
        return newNode;
    }
    case sqf::parser::sqf::bison::astkind::IDENT: {
        Node newNode;

        newNode.type = InstructionType::getVariable;
        newNode.file = *input.token.path;
        newNode.line = input.token.line;
        newNode.offset = input.token.offset;
        auto varname = std::string(input.token.contents);
        std::transform(varname.begin(), varname.end(), varname.begin(), ::tolower);
        newNode.value = std::string(varname);
        return newNode;
    }
    case sqf::parser::sqf::bison::astkind::STRING: {
        Node newNode;

        newNode.type = InstructionType::push;
        newNode.file = *input.token.path;
        newNode.line = input.token.line;
        newNode.offset = input.token.offset;
        newNode.value = ::sqf::types::d_string::from_sqf(input.token.contents);
        return newNode;
    }

    case sqf::parser::sqf::bison::astkind::BOOLEAN_TRUE: {
        Node newNode;

        newNode.type = InstructionType::push;
        newNode.file = *input.token.path;
        newNode.line = input.token.line;
        newNode.offset = input.token.offset;
        newNode.value = true;
        return newNode;
    }
    case sqf::parser::sqf::bison::astkind::BOOLEAN_FALSE: {
        Node newNode;

        newNode.type = InstructionType::push;
        newNode.file = *input.token.path;
        newNode.line = input.token.line;
        newNode.offset = input.token.offset;
        newNode.value = false;
        return newNode;
    }




    case sqf::parser::sqf::bison::astkind::CODE: {
        Node newNode;

        newNode.type = InstructionType::push;
        newNode.file = *input.token.path;
        newNode.line = input.token.line;
        newNode.offset = input.token.offset;

        auto codeEnd = input.token.offset;

        if (!input.children.empty()) {

            auto* statements = &input.children[0];
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


            auto endData = input.token.contents.data() + (lastToken - input.token.offset);

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
                            ++endData;++lastToken;
                        }
                        // after ending ]
                        ++endData; ++lastToken;
                        break;
                    default: ;
                }
            }


            while (*endData && *endData != '}') {
                ++endData;
                ++lastToken;
            }
            // we stop BEFORE ending }, we only want the inner code.

            codeEnd = lastToken;
            newNode.value = ScriptCodePiece({}, codeEnd - input.token.offset - 1, input.token.offset + 1);//instructions are empty as they are in node children
        } else {
            newNode.value = ScriptCodePiece({}, 0, input.token.offset + 1);//instructions are empty as they are in node children

        }
        
        
        // empty code {}
        if (input.children.empty()) return newNode;

        if (input.children[0].kind != sqf::parser::sqf::bison::astkind::STATEMENTS)
            __debugbreak();

        for (auto& it : input.children[0].children) {
            newNode.children.emplace_back(nodeFromAST(it));
        }

        return newNode;
    }

    case sqf::parser::sqf::bison::astkind::STATEMENTS: { // this is probably wrong
        Node newNode;

        newNode.type = InstructionType::endStatement; //just a dummy
        //newNode.file = *input.token.path;
        newNode.line = input.token.line;
        newNode.offset = input.token.offset;

        for (auto& it : input.children) {
            newNode.children.emplace_back(nodeFromAST(it));
        }

        return newNode;
    }


    case sqf::parser::sqf::bison::astkind::ARRAY: {

        Node newNode;

        newNode.type = InstructionType::makeArray;
        newNode.file = *input.token.path;
        newNode.line = input.token.line;
        newNode.offset = input.token.offset;
        for (auto& it : input.children) {
            newNode.children.emplace_back(nodeFromAST(it));
        }

        return newNode;
    }
                                              //case sqf::parser::sqf::bison::astkind::NA:
                                              //case sqf::parser::sqf::bison::astkind::SQF:
                                              //case sqf::parser::sqf::bison::astkind::STATEMENT:
                                              //case sqf::parser::sqf::bison::astkind::BRACKETS:
                                              //    for (auto& it : node.children)
                                              //        stuffAST(output, instructions, it);
    default: {

            Node newNode; // old stuff, now handled by astkind::STATEMENTS

            newNode.type = InstructionType::endStatement; //just a dummy
            newNode.file = *input.token.path;
            newNode.line = input.token.line;
            newNode.offset = input.token.offset;
            for (auto& it : input.children) {
                newNode.children.emplace_back(nodeFromAST(it));
            }
            return newNode;
        }
    }
    __debugbreak(); //should never reach
}
