#pragma once
#include "optimizerModuleBase.hpp"

#include "sol/sol.hpp"



class OptimizerModuleLua : public virtual OptimizerModuleBase {
public:
    void optimizeLua(Node& node);

    sol::protected_function nodeHandler;


private:
    void processNode(Node& node);
};