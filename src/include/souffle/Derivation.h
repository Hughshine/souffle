//
// Created by Hugh on 2024/11/13.
//

#ifndef DERIVATION_H
#define DERIVATION_H
#include <map>
#include <set>
#include "souffle/RamTypes.h"

class DerivationManager {
public:
// private:
    static std::map<void*, std::set<souffle::RamDomain>*> tuple2Rules;
    static void print() {
        std::cout << "DerivationManager::print()" << std::endl;
    }
};


#endif //DERIVATION_H
