#pragma once
#include "optimizerModuleConstantFold.hpp"


class Optimizer : public OptimizerModuleConstantFold {
public:

    void optimize(Node& node);


};