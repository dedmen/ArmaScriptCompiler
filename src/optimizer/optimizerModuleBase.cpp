#include "optimizerModuleBase.hpp"
#include <algorithm>
#include "parsesqf.h"
#include <charconv>
#include "stringdata.h"
#include <queue>


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
    auto nodeType = static_cast<sqf::parse::sqf::sqfasttypes::sqfasttypes>(input.kind);
    switch (nodeType) {

    case sqf::parse::sqf::sqfasttypes::ASSIGNMENT:
    case sqf::parse::sqf::sqfasttypes::ASSIGNMENTLOCAL: {
        Node newNode;

        newNode.type = nodeType == sqf::parse::sqf::sqfasttypes::ASSIGNMENT ? InstructionType::assignTo : InstructionType::assignToLocal;
        newNode.file = input.file;
        newNode.line = input.line;
        newNode.offset = input.offset;
        auto varname = input.children[0].content;
        std::transform(varname.begin(), varname.end(), varname.begin(), ::tolower);
        newNode.value = std::move(varname);

        newNode.children.emplace_back(nodeFromAST(input.children[1]));
        return newNode;
    }
    case sqf::parse::sqf::sqfasttypes::BEXP1:
    case sqf::parse::sqf::sqfasttypes::BEXP2:
    case sqf::parse::sqf::sqfasttypes::BEXP3:
    case sqf::parse::sqf::sqfasttypes::BEXP4:
    case sqf::parse::sqf::sqfasttypes::BEXP5:
    case sqf::parse::sqf::sqfasttypes::BEXP6:
    case sqf::parse::sqf::sqfasttypes::BEXP7:
    case sqf::parse::sqf::sqfasttypes::BEXP8:
    case sqf::parse::sqf::sqfasttypes::BEXP9:
    case sqf::parse::sqf::sqfasttypes::BEXP10:
    case sqf::parse::sqf::sqfasttypes::BINARYEXPRESSION: {
        Node newNode;

        newNode.type = InstructionType::callBinary;
        newNode.file = input.file;
        newNode.line = input.line;
        newNode.offset = input.offset;
        auto varname = input.children[1].content;
        std::transform(varname.begin(), varname.end(), varname.begin(), ::tolower);
        newNode.value = std::move(varname);

        newNode.children.emplace_back(nodeFromAST(input.children[0]));
        newNode.children.emplace_back(nodeFromAST(input.children[2]));

        return newNode;
    }
    case sqf::parse::sqf::sqfasttypes::BINARYOP: __debugbreak(); break;
    case sqf::parse::sqf::sqfasttypes::PRIMARYEXPRESSION: __debugbreak(); break;
    case sqf::parse::sqf::sqfasttypes::NULAROP: {
        Node newNode;

        newNode.type = InstructionType::callNular;
        newNode.file = input.file;
        newNode.line = input.line;
        newNode.offset = input.offset;
        auto varname = input.content;
        std::transform(varname.begin(), varname.end(), varname.begin(), ::tolower);
        newNode.value = std::move(varname);
        return newNode;
    }
    case sqf::parse::sqf::sqfasttypes::UNARYEXPRESSION: {
        Node newNode;

        newNode.type = InstructionType::callUnary;
        newNode.file = input.file;
        newNode.line = input.line;
        newNode.offset = input.offset;
        auto varname = input.children[0].content;
        std::transform(varname.begin(), varname.end(), varname.begin(), ::tolower);
        newNode.value = std::move(varname);
        auto subEl = nodeFromAST(input.children[1]);

        newNode.children.emplace_back(std::move(subEl));
        return newNode;
    }
    case sqf::parse::sqf::sqfasttypes::UNARYOP: __debugbreak(); break;
    case sqf::parse::sqf::sqfasttypes::NUMBER:
    case sqf::parse::sqf::sqfasttypes::HEXNUMBER: {
        float val;
        auto res =
            (nodeType == sqf::parse::sqf::sqfasttypes::HEXNUMBER) ?
            std::from_chars(input.content.data() + 2, input.content.data() + input.content.size(), val, std::chars_format::hex)
            :
            std::from_chars(input.content.data(), input.content.data() + input.content.size(), val);
        if (res.ec == std::errc::invalid_argument) {
            throw std::runtime_error("invalid scalar at: " + input.file + ":" + std::to_string(input.line));
        }
        else if (res.ec == std::errc::result_out_of_range) {
            throw std::runtime_error("scalar out of range at: " + input.file + ":" + std::to_string(input.line));
        }


        Node newNode;

        newNode.type = InstructionType::push;
        newNode.file = input.file;
        newNode.line = input.line;
        newNode.offset = input.offset;
        newNode.value = val;
        return newNode;
    }
    case sqf::parse::sqf::sqfasttypes::VARIABLE: {
        Node newNode;

        newNode.type = InstructionType::getVariable;
        newNode.file = input.file;
        newNode.line = input.line;
        newNode.offset = input.offset;
        auto varname = input.content;
        std::transform(varname.begin(), varname.end(), varname.begin(), ::tolower);
        newNode.value = std::move(varname);
        return newNode;
    }
    case sqf::parse::sqf::sqfasttypes::STRING: {
        Node newNode;

        newNode.type = InstructionType::push;
        newNode.file = input.file;
        newNode.line = input.line;
        newNode.offset = input.offset;
        newNode.value = sqf::stringdata::parse_from_sqf(input.content);
        return newNode;
    }
    case sqf::parse::sqf::sqfasttypes::CODE: {
        Node newNode;

        newNode.type = InstructionType::push;
        newNode.file = input.file;
        newNode.line = input.line;
        newNode.offset = input.offset;
        newNode.value = ScriptCodePiece({}, input.length - 2, input.offset + 1);//instructions are empty as they are in node children

        for (auto& it : input.children) {
            newNode.children.emplace_back(nodeFromAST(it));
        }

        return newNode;
    }
    case sqf::parse::sqf::sqfasttypes::ARRAY: {

        Node newNode;

        newNode.type = InstructionType::makeArray;
        newNode.file = input.file;
        newNode.line = input.line;
        newNode.offset = input.offset;
        for (auto& it : input.children) {
            newNode.children.emplace_back(nodeFromAST(it));
        }

        return newNode;
    }
                                              //case sqf::parse::sqf::sqfasttypes::NA:
                                              //case sqf::parse::sqf::sqfasttypes::SQF:
                                              //case sqf::parse::sqf::sqfasttypes::STATEMENT:
                                              //case sqf::parse::sqf::sqfasttypes::BRACKETS:
                                              //    for (auto& it : node.children)
                                              //        stuffAST(output, instructions, it);
    default: {

            Node newNode;

            newNode.type = InstructionType::endStatement; //just a dummy
            newNode.file = input.file;
            newNode.line = input.line;
            newNode.offset = input.offset;
            for (auto& it : input.children) {
                newNode.children.emplace_back(nodeFromAST(it));
            }
            return newNode;
        }
    }
    __debugbreak(); //should never reach
}
