//
// Created by Hugh on 2024/11/13.
//

#ifndef DERIVATION_H
#define DERIVATION_H
#include "souffle/RamTypes.h"

#include <fstream>
#include <map>
#include <set>
#include <vector>

struct UntypedTuple {
    std::string relation_name;
    std::vector<souffle::RamDomain> fields;
    static std::string to_string(UntypedTuple& tuple) {
        std::string result = tuple.relation_name + '(' + to_string_fields(tuple.fields) + ')';
        return result;
    }
    static std::string to_string_fields(std::vector<souffle::RamDomain>& fields) {
        std::string result = "";
        bool first = true;
        for (const auto& field : fields) {
            if (first) {
                first = false;
                result += std::to_string(field);
            } else {
                result += "," + std::to_string(field);
            }
        }
        return result;
    }
};

inline UntypedTuple testUntypedTuple{"T", {0, -1, -2, -42}};

class DerivationManager {
public:
// private:
    // TODO
    // mapping from tuple's pointer to rules that derive it

    static inline std::set<souffle::RamDomain> testRules = {
        0, 1, 2, 42
    };
    static inline std::map<void*, std::set<souffle::RamDomain>*> tuplePtr2Rules = {
        // {nullptr, &testRules}
    };
    // mapping from tuple's pointer to its (untyped) real representation, i.e., relation name and value list
    static inline std::map<void*, UntypedTuple*> tuplePtr2UntypedTuple = {
        // {nullptr, &testUntypedTuple}
    };

    static std::string rules2Str(const std::set<souffle::RamDomain>* rules) {
        std::string result = "[";
        bool first = true;
        for (auto& ruleId : *rules) {
            if (first) {
                first = false;
                result += std::to_string(ruleId);
            } else {
                result += "," + std::to_string(ruleId);
            }
        }
        return result + ']';
    }

    static void dumpDerivationInfo(const std::string& filename, const std::string& outputDir) {
        std::cout << "DerivationManager::dumpDerivationInfo()" << std::endl;
        std::cout << filename << std::endl;
        // TODO: should change to souffle's IO system later
        std::string baseFilename = filename;
        if (baseFilename.size() >= 3 && baseFilename.substr(baseFilename.size() - 3) == ".dl") {
            baseFilename = baseFilename.substr(0, baseFilename.size() - 3);
        }
        std::cout << baseFilename << std::endl;
        std::string derivationInfoFilename = baseFilename + "-derivation-info.txt";
        std::ofstream os{derivationInfoFilename};
        for (const auto& [tuplePtr, tuple] : tuplePtr2UntypedTuple) {
            os << UntypedTuple::to_string(*tuple) << '\t' << rules2Str(tuplePtr2Rules[tuplePtr]) << std::endl;
        }
        os.close();

    }
};


#endif //DERIVATION_H
