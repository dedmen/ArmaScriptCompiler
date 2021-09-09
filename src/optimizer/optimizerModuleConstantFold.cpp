#include "optimizerModuleConstantFold.hpp"
#include <algorithm>
#include <intrin.h>
#include <sstream>
#include <unordered_set>
#include <iostream>

constexpr auto NularPushConstNularCommand(std::string_view name) {
    return [name](OptimizerModuleBase::Node& node) -> void {
        node.type = InstructionType::push;
        if (!node.children.empty())
            __debugbreak();
        node.children.clear();
        node.constant = true;
        node.value = ScriptConstantNularCommand(std::string(name));
    };
}

#define ONLY_PUSH_BINARY if (node.children[0].type != InstructionType::push || node.children[1].type != InstructionType::push) return

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


        //binaryActions["+"] = [](OptimizerModuleBase::Node & node) -> void { //#TODO array
        //    ONLY_PUSH_BINARY;
        //    if (node.children[0].value.index() == 1) { //string
        //        auto leftArg = std::get<STRINGTYPE>(node.children[0].value);
        //        auto rightArg = std::get<STRINGTYPE>(node.children[1].value);
        //        node.value = leftArg + rightArg;
        //    } else {//float
        //        float leftArg = std::get<float>(node.children[0].value);
        //        float rightArg = std::get<float>(node.children[1].value);
        //        node.value = leftArg + rightArg;
        //    }
        //
        //
        //    
        //    node.type = InstructionType::push;
        //    node.children.clear();
        //    node.constant = true;
        //    
        //};

        binaryActions["-"] = [](OptimizerModuleBase::Node & node) -> void {
            ONLY_PUSH_BINARY;

            auto type = getConstantType(node.children[0].value);

            //#TODO fix array, this is safe if both are const, see params need to convert arrays to constants and merge
            if (type == ConstantType::array) { //array
                //std::unordered_set<STRINGTYPE> vals;
                //
                //for (auto& i : node.children[1].children) { //#TODO number support
                //    if (i.value.index() != 1)
                //        return; //not string, don't optimize
                //    vals.emplace(std::get<STRINGTYPE>(i.value));
                //}
                //std::vector<OptimizerModuleBase::Node> newNodes;
                //for (auto& it : node.children[0].children) {
                //    if (it.value.index() != 1)
                //        return; //not string, don't optimize
                //    auto & sval = std::get<STRINGTYPE>(it.value);
                //
                //    auto found = vals.find(sval);
                //    if (found == vals.end())
                //        newNodes.emplace_back(std::move(it));
                //}
                //node.children[0].children = std::move(newNodes);
                return;
            } else if (type == ConstantType::scalar) {//float
                float leftArg = std::get<float>(node.children[0].value);
                float rightArg = std::get<float>(node.children[1].value);
                node.value = leftArg - rightArg;
            }
            else
            {
                std::cout << "Something is very wrong, tried to optimize operator '-' but argument type is neither array nor number?! Breaking into debugger now." << std::endl;
                __debugbreak();

            }
            node.type = InstructionType::push;
            node.children.clear();
            node.constant = true;
        };

        binaryActions["/"] = [](OptimizerModuleBase::Node & node) -> void {
            ONLY_PUSH_BINARY;
            auto type = getConstantType(node.children[0].value);
            if (type != ConstantType::scalar)
                return;

            float leftArg = std::get<float>(node.children[0].value);
            float rightArg = std::get<float>(node.children[1].value);
        
            node.type = InstructionType::push;
            node.children.clear();
            node.constant = true;
            node.value = leftArg / rightArg;
        };
        binaryActions["*"] = [](OptimizerModuleBase::Node & node) -> void {
            ONLY_PUSH_BINARY;
            auto type = getConstantType(node.children[0].value);
            if (type != ConstantType::scalar)
                return;

            float leftArg = std::get<float>(node.children[0].value);
            float rightArg = std::get<float>(node.children[1].value);

            node.type = InstructionType::push;
            node.children.clear();
            node.constant = true;
            node.value = leftArg * rightArg;
        };

        binaryActions["mod"] = [](OptimizerModuleBase::Node & node) -> void {
            ONLY_PUSH_BINARY;
            auto type = getConstantType(node.children[0].value);
            if (type != ConstantType::scalar)
                return;

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


        // Params is special, it takes an array but never returns part of that array, so we can safely make it constant
        unaryActions["params"] = [](OptimizerModuleBase::Node& node) -> void {

            // convert child makeArray node into a constant push
            if (node.children[0].type == InstructionType::makeArray) {

                //convert nested arrays into constants
                
                std::function<void(OptimizerModuleBase::Node&)> resolveMakeArray = [&](OptimizerModuleBase::Node& node)
                {
                    if (node.type != InstructionType::makeArray)
                        return;

                    for (auto& it : node.children)
                        resolveMakeArray(it);

                    node.value = ScriptConstantArray(); //dummy. Children are the contents
                    node.type = InstructionType::push;
                };


                resolveMakeArray(node.children[0]);

                 bool allPush = std::all_of(node.children[0].children.begin(), node.children[0].children.end(), [](const OptimizerModuleBase::Node& it)
                     {
                         return it.type == InstructionType::push;
                     });
                 if (!allPush) {
                     std::stringstream buf;
                     node.dumpTree(buf, 0);
                     auto str = buf.str();
                     __debugbreak();
                 }



                node.children[0].value = ScriptConstantArray(); //dummy. Children are the contents
                node.children[0].type = InstructionType::push;


                // check if params array contains a value that can be modified by reference, to prevent it modifying the value in compiled code

                const auto& paramsList = node.children[0].children;

                for (auto& it : paramsList)
                {
                    //#TODO throw warning on empty array passed to params?
                    if (!it.children.empty() && std::holds_alternative<ScriptConstantArray>(it.value)) // is array, and is not empty
                    {
                        // it is a [name, default, allowedTypes] array
                        auto& paramArguments = it.children;

                        if (paramArguments.size() < 2) // only [name], no defaults, don't care
                            continue;

                        if (std::holds_alternative<ScriptConstantArray>(paramArguments[1].value))
                        {
                            // !!! [name, []] the array will be passed down by ref if parameter is not provided, and cause changes to propagate/persist to compiled code if modified by reference
                            // We know that this will be a problem, lets insert a array copy

                            // replace ourselves with a + unary

                            // move our array out so we can move it over instead of copying
                            auto myArgumentsArray = std::move(node.children[0]);
                            // our only child is now default initialized, we have no children
                            node.children.clear();

                            // convert ourselves to a unary + and add the array as argument
                            OptimizerModuleBase::Node copyNode;
                            copyNode.type = InstructionType::callUnary;
                            copyNode.constant = false; // Lets prevent optimizing this copy out, shouldn't be done anyway but better be safe
                            copyNode.value = STRINGTYPE("+"); // value is name of command
                            copyNode.file = node.file;
                            copyNode.line = myArgumentsArray.line;
                            copyNode.offset = myArgumentsArray.offset;
                            copyNode.children.emplace_back(std::move(myArgumentsArray)); // Add the arguments array back

                            node.children.emplace_back(std::move(copyNode));

                            // now changed params [...] to params +[...]
                            break; // paramsList has been invalidated, cannot keep iterating
                        }
                    }
                }

            }
        };

        //#TODO optimize param too, if non-array constant default value, see params array checking


    }

    void setupNulary() {
        nularyActions["true"] = [](OptimizerModuleBase::Node & node) -> void {
            node.type = InstructionType::push;
            if (!node.children.empty())
                __debugbreak();
            node.children.clear();
            node.constant = true;
            node.value = true;
        };
        
        nularyActions["false"] = [](OptimizerModuleBase::Node & node) -> void {
            node.type = InstructionType::push;
            if (!node.children.empty())
                __debugbreak();
            node.children.clear();
            node.constant = true;
            node.value = false;
        };

#define NULAR_CONST_COMMAND(name) nularyActions[#name] = NularPushConstNularCommand(#name)

        // These commands will be evaluated once at compilation, and then used as a constant with a push instruction

        NULAR_CONST_COMMAND(nil);

        //#TODO test this
        // NULAR_CONST_COMMAND(missionnamespace);
        NULAR_CONST_COMMAND(uinamespace);
        //#TODO test this, its not initialized at preStart
        //NULAR_CONST_COMMAND(profilenamespace);

        // constant null types
        NULAR_CONST_COMMAND(objnull);
        NULAR_CONST_COMMAND(controlnull);
        NULAR_CONST_COMMAND(displaynull);
        NULAR_CONST_COMMAND(grpnull);
        NULAR_CONST_COMMAND(locationnull);
        NULAR_CONST_COMMAND(scriptnull);
        NULAR_CONST_COMMAND(confignull);

        //#TODO test, not sure if reliable at preStart?
        //NULAR_CONST_COMMAND(hasinterface);
        NULAR_CONST_COMMAND(linebreak);
        NULAR_CONST_COMMAND(configfile);
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

                //#TODO they could also be nested arrays, so makeArray with constants in it
                bool allPush = std::all_of(node.children.begin(), node.children.end(), [](const Node & it)
                    {
                        return it.type == InstructionType::push;
                    });



                //if (!allPush) {
                //    std::stringstream buf;
                //    node.dumpTree(buf, 0);
                //    auto str = buf.str();
                //    __debugbreak();
                //}
                //
                //node.value = ScriptConstantArray(); //dummy. Children are the contents
                //node.type = InstructionType::push;
                node.constant = true;
            }
        } break;
        default: ;
    }


}
