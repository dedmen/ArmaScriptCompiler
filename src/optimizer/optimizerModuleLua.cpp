#include "optimizerModuleLua.hpp"
#include <algorithm>
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <x86intrin.h>
#endif
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
