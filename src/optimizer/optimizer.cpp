#include "optimizer.h"

void Optimizer::optimize(Node& node) {
    optimizeConstantFold(node);
}
