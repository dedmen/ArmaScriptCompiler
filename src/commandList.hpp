#pragma once
#include "runtime/runtime.h"

class CommandList {
public:
    static void initNular(::sqf::runtime::runtime& runtime);
    static void initUnary(::sqf::runtime::runtime& runtime);
    static void initBinary(::sqf::runtime::runtime& runtime);


    static void init(::sqf::runtime::runtime& runtime) {
        initNular(runtime);
        initUnary(runtime);
        initBinary(runtime);
    }
};
