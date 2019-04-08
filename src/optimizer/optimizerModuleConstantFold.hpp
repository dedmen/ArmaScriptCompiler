#pragma once
#include "optimizerModuleBase.hpp"

class OptimizerModuleConstantFold : public virtual OptimizerModuleBase {
public:
    void optimizeConstantFold(Node& node);

private:
    void processNode(Node& node);
};