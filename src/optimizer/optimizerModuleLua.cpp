#include "optimizerModuleLua.hpp"
#include <algorithm>
#include <intrin.h>
#include <sstream>
#include <unordered_set>

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



void OptimizerModuleLua::optimizeLua(Node& node) {
    auto worklist = node.bottomUpFlatten();

    for (auto& it : worklist) {
        processNode(*it);
    }
}

void OptimizerModuleLua::processNode(Node& node) {
    nodeHandler(node);
}
