
#include <iostream>
#include <iomanip>
#include "souffle/problog/formula/CuddManager.h"
#include "souffle/problog/formula/LogicFormulaManager.h"

int main() {
    {
        LogicFormulaManager manager;
        auto x1 = manager.createVar(1);  // x1
        auto x2 = manager.createVar(2);  // x2
//        auto f1 = manager.makeAnd(x1, manager.makeNot(x2));  // (x1 & !x2)
        auto f1 = manager.makeAnd({x1, manager.makeNot(x2)});  // (x1 & !x2)
        manager.printInfo(f1, "formula1");  // Will print: formula = (x1 & !(x2))
//        auto f2 = manager.makeAnd(x1, manager.makeNot(x2));  // (x1 & !x2)
        auto f2 = manager.makeAnd({x1, manager.makeNot(x2)});  // (x1 & !x2)
        manager.printInfo(f2, "formula2");  // Will print: formula = (x1 & !(x2))
        std::cout << "f1 == f2: " << manager.isSame(f1, f2) << std::endl;
        WeightedBDDManager bddManager;
        auto bdd = transform(f1, manager, bddManager);
        bddManager.setVariableWeight(1, 0.9, 0.1);
        bddManager.setVariableWeight(2, 0.8, 0.2);
        bddManager.printInfo(bdd, "bdd");
    }
    {
        WeightedBDDManager manager;
        auto x1 = manager.createVar(1);  // x1
        auto x2 = manager.createVar(2);  // x2
        manager.setVariableWeight(1, 0.9, 0.1);
        manager.setVariableWeight(2, 0.8, 0.2);
        auto f1 = manager.makeAnd(x1, manager.makeNot(x2));  // (x1 & !x2)
        //    auto f1 = manager.makeAnd({x1, manager.makeNot(x2)});  // (x1 & !x2)
        manager.printInfo(f1, "bdd1");  // Will print: formula = (x1 & !(x2))
        auto f2 = manager.makeAnd(x1, manager.makeNot(x2));  // (x1 & !x2)
        //    auto f2 = manager.makeAnd({x1, manager.makeNot(x2)});  // (x1 & !x2)
        manager.printInfo(f2, "bdd2");  // Will print: formula = (x1 & !(x2))
        std::cout << "f1 == f2: " << manager.isSame(f1, f2) << std::endl;

    }
}