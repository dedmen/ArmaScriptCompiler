#include "optimizerModuleConstantFold.hpp"
#include <algorithm>
#include <sstream>

class OptimizerConstantFoldActionMap : public Singleton<OptimizerConstantFoldActionMap> {
public:

    OptimizerConstantFoldActionMap() {
        setupBinary();
        setupUnary();
        setupNulary();
    }

    void processBinary(OptimizerModuleBase::Node& node) {
        auto& cmdName = std::get<STRINGTYPE>(node.value);
        auto found = binaryActions.find(cmdName);
        if (found != binaryActions.end())
            found->second(node);
    }

    void processUnary(OptimizerModuleBase::Node& node) {
        auto& cmdName = std::get<STRINGTYPE>(node.value);
        auto found = unaryActions.find(cmdName);
        if (found != unaryActions.end())
            found->second(node);
    }

    void processNulary(OptimizerModuleBase::Node& node) {
        auto& cmdName = std::get<STRINGTYPE>(node.value);
        auto found = nularyActions.find(cmdName);
        if (found != nularyActions.end())
            found->second(node);
    }


private:

    void setupBinary() {
        binaryActions["else"] = [](OptimizerModuleBase::Node & node) -> void {
            node.type = InstructionType::push;
            node.constant = true;

            node.value = ScriptConstantArray(); //dummy. Children are the contents
        };

        //math


        binaryActions["+"] = [](OptimizerModuleBase::Node & node) -> void {
            float leftArg = std::get<float>(node.children[0].value);
            float rightArg = std::get<float>(node.children[1].value);

            node.type = InstructionType::push;
            node.children.clear();
            node.constant = true;
            node.value = leftArg + rightArg;
        };

        binaryActions["-"] = [](OptimizerModuleBase::Node & node) -> void {
            float leftArg = std::get<float>(node.children[0].value);
            float rightArg = std::get<float>(node.children[1].value);

            node.type = InstructionType::push;
            node.children.clear();
            node.constant = true;
            node.value = leftArg - rightArg;
        };

        binaryActions["/"] = [](OptimizerModuleBase::Node & node) -> void {
            float leftArg = std::get<float>(node.children[0].value);
            float rightArg = std::get<float>(node.children[1].value);

            node.type = InstructionType::push;
            node.children.clear();
            node.constant = true;
            node.value = leftArg / rightArg;
        };
        binaryActions["*"] = [](OptimizerModuleBase::Node & node) -> void {
            float leftArg = std::get<float>(node.children[0].value);
            float rightArg = std::get<float>(node.children[1].value);

            node.type = InstructionType::push;
            node.children.clear();
            node.constant = true;
            node.value = leftArg * rightArg;
        };

        binaryActions["mod"] = [](OptimizerModuleBase::Node & node) -> void {
            float leftArg = std::get<float>(node.children[0].value);
            float rightArg = std::get<float>(node.children[1].value);

            node.type = InstructionType::push;
            node.children.clear();
            node.constant = true;
            node.value = fmodf(leftArg, rightArg);
        };

    }

    void setupUnary() {
        unaryActions["sqrt"] = [](OptimizerModuleBase::Node & node) -> void {
            float rightArg = std::get<float>(node.children[0].value);

            node.type = InstructionType::push;
            node.children.clear();
            node.constant = true;
            node.value = sqrt(rightArg);
        };

        unaryActions["!"] = [](OptimizerModuleBase::Node & node) -> void {
            bool rightArg = std::get<bool>(node.children[0].value);

            node.type = InstructionType::push;
            node.children.clear();
            node.constant = true;
            node.value = !rightArg;
        };
    }

    void setupNulary() {
        nularyActions["true"] = [](OptimizerModuleBase::Node & node) -> void {
            node.type = InstructionType::push;
            node.children.clear();
            node.constant = true;
            node.value = true;
        };

        nularyActions["false"] = [](OptimizerModuleBase::Node & node) -> void {
            node.type = InstructionType::push;
            node.children.clear();
            node.constant = true;
            node.value = false;
        };
    }

    std::unordered_map<std::string, std::function<void(OptimizerModuleBase::Node&)>> binaryActions;
    std::unordered_map<std::string, std::function<void(OptimizerModuleBase::Node&)>> unaryActions;
    std::unordered_map<std::string, std::function<void(OptimizerModuleBase::Node&)>> nularyActions;
};






void OptimizerModuleConstantFold::optimizeConstantFold(Node& node) {
    
    auto worklist = node.bottomUpFlatten();

    for (auto& it : worklist) {
        processNode(*it);
    }




}

void OptimizerModuleConstantFold::processNode(Node& node) {
    
    switch (node.type) {
        case InstructionType::endStatement: break;
        case InstructionType::push: {
            node.constant = true;
        }break;
        case InstructionType::callUnary: {
            if (!node.areChildrenConstant()) break;
            OptimizerConstantFoldActionMap::get().processUnary(node);

        } break;
        case InstructionType::callBinary: {
            if (!node.areChildrenConstant()) break;
            OptimizerConstantFoldActionMap::get().processBinary(node);

        } break;
        case InstructionType::callNular: {
            if (!node.areChildrenConstant()) break;
            OptimizerConstantFoldActionMap::get().processNulary(node);
        } break;
        case InstructionType::assignTo: break;
        case InstructionType::assignToLocal: break;
        case InstructionType::getVariable: break;
        case InstructionType::makeArray: {
            if (node.areChildrenConstant()) {//#TODO when converting to ASM check again if all elements are push
                bool allPush = std::all_of(node.children.begin(), node.children.end(), [](const Node & it)
                    {
                        return it.type == InstructionType::push;
                    });
                if (!allPush) {
                    std::stringstream buf;
                    node.dumpTree(buf, 0);
                    auto str = buf.str();
                    __debugbreak();
                }

                node.value = ScriptConstantArray(); //dummy. Children are the contents
                node.type = InstructionType::push;
                node.constant = true;
            }
        } break;
        default: ;
    }


}
