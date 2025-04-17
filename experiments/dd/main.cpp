
#include <iostream>
#include <iomanip>
#include "souffle/problog/formula/CuddManager.h"

int main() {
    WeightedBDDManager manager;

    {
        int num = 10000;
        // 创建复杂表达式
        std::vector<BddNodeRef> vars;
        for (int i = 0; i < num; ++i) {
            vars.push_back(manager.createVar(i));
        }

        BddNodeRef deepAnd = vars[0];
        for (int i = 1; i < num; ++i) {
            deepAnd = manager.makeAnd(deepAnd, vars[i]);
        }

        BddNodeRef not1 = manager.makeNot(vars[1]);
        BddNodeRef orExpr = manager.makeOr(not1, vars[2]);

        BddNodeRef final = manager.makeAnd(deepAnd, orExpr);

        manager.printInfo(final, "F");
        std::cout << "🧪 [before scope ends] ref count = "
                  << Cudd_CheckZeroRef(manager.getManager()) << std::endl;
    }

    std::cout << "✅ [after internal refs released] ref count = "
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