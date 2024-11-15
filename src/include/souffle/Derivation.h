//
// Created by Hugh on 2024/11/13.
//

#ifndef DERIVATION_H
#define DERIVATION_H
#include "souffle/RamTypes.h"

#include <fstream>
#include <cassert>
#include <map>
#include <set>
#include <vector>

struct UntypedTuple {
    std::string relation_name;
    std::vector<souffle::RamDomain> fields;
    static std::string toString(const UntypedTuple& tuple) {
        std::string result = tuple.relation_name + '(' + toStringFields(tuple.fields) + ')';
        return result;
    }
    static std::string toStringFields(const std::vector<souffle::RamDomain>& fields) {
        std::string result;
        bool first = true;
        // may use StreamUtil::join()
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

    bool operator<(const UntypedTuple& other) const {
        // Primary sort by name, secondary sort by age
        if (relation_name != other.relation_name) {
            return relation_name < other.relation_name;
        }
        if (fields.size() != other.fields.size()) {
            return fields.size() < other.fields.size();
        }
        for (size_t i = 0; i < fields.size(); i++) {
            if (fields[i] != other.fields[i]) {
                return fields[i] < other.fields[i];
            }
        }
        return false;
    }

    // TODO: avoid non-reference passing
    template<std::size_t N>
    static UntypedTuple fromTypedTuple(const std::string& relationName, const souffle::Tuple<souffle::RamDomain, N>& typedTuple) {
        UntypedTuple result;
        result.relation_name = relationName;
        for (const auto& field : typedTuple) {
            result.fields.push_back(field);
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
    static inline std::map<UntypedTuple, std::set<souffle::RamDomain>*> untypedTuple2Rules = {
        // {testUntypedTuple, &testRules}
    };
    // mapping from tuple's pointer to its (untyped) real representation, i.e., relation name and value list
    // static inline std::map<const void*, UntypedTuple*> tuplePtr2UntypedTuple = {
    //     // {testUntypedTuple, &testUntypedTuple}
    // };

    static std::string rules2Str(const std::set<souffle::RamDomain>* rules) {
        assert(rules != nullptr && "null ruleSet");
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
        // TODO: should change to souffle's IO system later
        std::string baseFilename = filename;
        if (baseFilename.size() >= 3 && baseFilename.substr(baseFilename.size() - 3) == ".dl") {
            baseFilename = baseFilename.substr(0, baseFilename.size() - 3);
        }
        std::string derivationInfoFilename = baseFilename + "-derivation-info.txt";
        std::ofstream os{derivationInfoFilename};
        for (const auto& [tuple, ruleSet] : untypedTuple2Rules) {
            os << UntypedTuple::toString(tuple) << '\t';
            os << rules2Str(ruleSet) << std::endl;
        }
        os.close();
    }
};


#endif //DERIVATION_H
