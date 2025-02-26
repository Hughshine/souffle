
#include <iostream>
#include <iomanip>
#include "souffle/problog/formula/CuddManager.h"
#include "souffle/problog/formula/LogicFormulaManager.h"

int main() {
    LogicFormulaManager manager;
    auto x1 = manager.createVar(1);  // x1
    auto x2 = manager.createVar(2);  // x2
    auto f = manager.makeAnd({x1, manager.makeNot(x2)});  // (x1 & !x2)
    manager.printInfo(f, "formula");  // Will print: formula = (x1 & !(x2))


    try {
        WeightedBDDManager bddManager;
        auto bdd = transform(f, manager, bddManager);
        // auto f = bddManager.createWeightedExample();

        std::cout << std::fixed << std::setprecision(6);

        bddManager.setVariableWeight(1, 0.9, 0.1);
        bddManager.setVariableWeight(2, 0.8, 0.2);

        bddManager.printInfo(bdd, "");
        // // auto var0 = bddManager.createVar(0);
        // // auto var1 = bddManager.createVar(1);
        // // auto g = bddManager.makeAnd(var0, var1);
        //
        // std::cout << "\nWeighted model count: "
        // << bddManager.computeWeightedModelCount(bdd) << std::endl;

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    manager.printInfo(f, "formula");  // Will print: formula = (x1 & !(x2))

}