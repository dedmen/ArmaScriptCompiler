#include "optimizerModuleBase.hpp"
#include <algorithm>
#include <charconv>
#include <queue>

#include "runtime/d_string.h"


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

    case sqf::parser::sqf::impl_default::nodetype::ASSIGNMENT:
    case sqf::parser::sqf::impl_default::nodetype::ASSIGNMENTLOCAL: {
        Node newNode;

        newNode.type = nodeType == sqf::parser::sqf::impl_default::nodetype::ASSIGNMENT ? InstructionType::assignTo : InstructionType::assignToLocal;
        newNode.file = input.path.virtual_;
        newNode.line = input.line;
        newNode.offset = input.file_offset;
        auto varname = input.children[0].content;
        std::transform(varname.begin(), varname.end(), varname.begin(), ::tolower);
        newNode.value = std::move(varname);

        newNode.children.emplace_back(nodeFromAST(input.children[1]));
        return newNode;
    }
    case sqf::parser::sqf::impl_default::nodetype::BEXP1:
    case sqf::parser::sqf::impl_default::nodetype::BEXP2:
    case sqf::parser::sqf::impl_default::nodetype::BEXP3:
    case sqf::parser::sqf::impl_default::nodetype::BEXP4:
    case sqf::parser::sqf::impl_default::nodetype::BEXP5:
    case sqf::parser::sqf::impl_default::nodetype::BEXP6:
    case sqf::parser::sqf::impl_default::nodetype::BEXP7:
    case sqf::parser::sqf::impl_default::nodetype::BEXP8:
    case sqf::parser::sqf::impl_default::nodetype::BEXP9:
    case sqf::parser::sqf::impl_default::nodetype::BEXP10:
    case sqf::parser::sqf::impl_default::nodetype::BINARYEXPRESSION: {
        Node newNode;

        newNode.type = InstructionType::callBinary;
        newNode.file = input.path.virtual_;
        newNode.line = input.line;
        newNode.offset = input.file_offset;
        auto varname = input.children[1].content;
        std::transform(varname.begin(), varname.end(), varname.begin(), ::tolower);
        newNode.value = std::move(varname);

        newNode.children.emplace_back(nodeFromAST(input.children[0]));
        newNode.children.emplace_back(nodeFromAST(input.children[2]));

        return newNode;
    }
    case sqf::parser::sqf::impl_default::nodetype::BINARYOP: __debugbreak(); break;
    case sqf::parser::sqf::impl_default::nodetype::PRIMARYEXPRESSION: __debugbreak(); break;
    case sqf::parser::sqf::impl_default::nodetype::NULAROP: {
        Node newNode;

        newNode.type = InstructionType::callNular;
        newNode.file = input.path.virtual_;
        newNode.line = input.line;
        newNode.offset = input.file_offset;
        auto varname = input.content;
        std::transform(varname.begin(), varname.end(), varname.begin(), ::tolower);
        newNode.value = std::move(varname);
        return newNode;
    }
    case sqf::parser::sqf::impl_default::nodetype::UNARYEXPRESSION: {
        Node newNode;

        newNode.type = InstructionType::callUnary;
        newNode.file = input.path.virtual_;
        newNode.line = input.line;
        newNode.offset = input.file_offset;
        auto varname = input.children[0].content;
        std::transform(varname.begin(), varname.end(), varname.begin(), ::tolower);
        newNode.value = std::move(varname);
        auto subEl = nodeFromAST(input.children[1]);

        newNode.children.emplace_back(std::move(subEl));
        return newNode;
    }
    case sqf::parser::sqf::impl_default::nodetype::UNARYOP: __debugbreak(); break;
    case sqf::parser::sqf::impl_default::nodetype::NUMBER:
    case sqf::parser::sqf::impl_default::nodetype::HEXNUMBER: {
        float val;
        auto res =
            (nodeType == sqf::parser::sqf::impl_default::nodetype::HEXNUMBER) ?
            std::from_chars(input.content.data() + 2, input.content.data() + input.content.size(), val, std::chars_format::hex)
            :
            std::from_chars(input.content.data(), input.content.data() + input.content.size(), val);
        if (res.ec == std::errc::invalid_argument) {
            throw std::runtime_error("invalid scalar at: " + input.path.virtual_ + ":" + std::to_string(input.line));
        }
        else if (res.ec == std::errc::result_out_of_range) {
            throw std::runtime_error("scalar out of range at: " + input.path.virtual_ + ":" + std::to_string(input.line));
        }


        Node newNode;

        newNode.type = InstructionType::push;
        newNode.file = input.path.virtual_;
        newNode.line = input.line;
        newNode.offset = input.file_offset;
        newNode.value = val;
        return newNode;
    }
    case sqf::parser::sqf::impl_default::nodetype::VARIABLE: {
        Node newNode;

        newNode.type = InstructionType::getVariable;
        newNode.file = input.path.virtual_;
        newNode.line = input.line;
        newNode.offset = input.file_offset;
        auto varname = input.content;
        std::transform(varname.begin(), varname.end(), varname.begin(), ::tolower);
        newNode.value = std::move(varname);
        return newNode;
    }
    case sqf::parser::sqf::impl_default::nodetype::STRING: {
        Node newNode;

        newNode.type = InstructionType::push;
        newNode.file = input.path.virtual_;
        newNode.line = input.line;
        newNode.offset = input.file_offset;
        newNode.value = ::sqf::types::d_string::from_sqf(input.content);
        return newNode;
    }
    case sqf::parser::sqf::impl_default::nodetype::CODE: {
        Node newNode;

        newNode.type = InstructionType::push;
        newNode.file = input.path.virtual_;
        newNode.line = input.line;
        newNode.offset = input.file_offset;
        newNode.value = ScriptCodePiece({}, input.length - 2, input.file_offset + 1);//instructions are empty as they are in node children

        for (auto& it : input.children) {
            newNode.children.emplace_back(nodeFromAST(it));
        }

        return newNode;
    }
    case sqf::parser::sqf::impl_default::nodetype::ARRAY: {

        Node newNode;

        newNode.type = InstructionType::makeArray;
        newNode.file = input.path.virtual_;
        newNode.line = input.line;
        newNode.offset = input.file_offset;
        for (auto& it : input.children) {
            newNode.children.emplace_back(nodeFromAST(it));
        }

        return newNode;
    }
                                              //case sqf::parser::sqf::impl_default::nodetype::NA:
                                              //case sqf::parser::sqf::impl_default::nodetype::SQF:
                                              //case sqf::parser::sqf::impl_default::nodetype::STATEMENT:
                                              //case sqf::parser::sqf::impl_default::nodetype::BRACKETS:
                                              //    for (auto& it : node.children)
                                              //        stuffAST(output, instructions, it);
    default: {

            Node newNode;

            newNode.type = InstructionType::endStatement; //just a dummy
            newNode.file = input.path.virtual_;
            newNode.line = input.line;
            newNode.offset = input.file_offset;
            for (auto& it : input.children) {
                newNode.children.emplace_back(nodeFromAST(it));
            }
            return newNode;
        }
    }
    __debugbreak(); //should never reach
}
