
#include <iostream>
#include <iomanip>
#include "souffle/problog/formula/CuddManager.h"

int main() {
    WeightedBDDManager manager;

    {
        // 所有 BddNodeRef 在这个作用域内创建并销毁
        // auto a = manager.createVar(0);
        // auto b = manager.createVar(1);
        // auto c = manager.createVar(2);

        // auto ab = manager.makeAnd(a, b);
        // auto notC = manager.makeNot(c);
        // auto final = manager.makeOr(ab, notC);

        // manager.printInfo(final, "F = (a ∧ b) ∨ ¬c");

        // 在这里检查不一定为 0（还没析构完）
        std::cout << "🔧 [Inner] CUDD ref count: "
                  << Cudd_CheckZeroRef(manager.getManager()) << std::endl;
    }

    // 现在所有 BddNodeRef 已析构，引用数应该为 0
    std::cout << "✅ [After destruction] CUDD ref count: "
              << Cudd_CheckZeroRef(manager.getManager()) << std::endl;

    return 0;
}
//int main() {
//    {
//        WeightedBDDManager manager;
//        auto x1 = manager.createVar(1);  // x1
//        auto x2 = manager.createVar(2);  // x2
//        manager.setVariableWeight(1, 0.9, 0.1);
//        manager.setVariableWeight(2, 0.8, 0.2);
//        auto f1 = manager.makeAnd(x1, manager.makeNot(x2));  // (x1 & !x2)
//        //    auto f1 = manager.makeAnd({x1, manager.makeNot(x2)});  // (x1 & !x2)
//        manager.printInfo(f1, "bdd1");  // Will print: formula = (x1 & !(x2))
//        auto f2 = manager.makeAnd(x1, manager.makeNot(x2));  // (x1 & !x2)
//        //    auto f2 = manager.makeAnd({x1, manager.makeNot(x2)});  // (x1 & !x2)
//        manager.printInfo(f2, "bdd2");  // Will print: formula = (x1 & !(x2))
//        std::cout << "f1 == f2: " << manager.isSame(f1, f2) << std::endl;
//
//    }
//}