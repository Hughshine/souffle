//
// Created by Hugh on 2024/11/13.
//

#ifndef DERIVATION_H
#define DERIVATION_H
#include "souffle/RamTypes.h"

#include <cassert>
#include <fstream>
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

struct RuleApplication {
    souffle::RamDomain ruleId;
    std::map<std::string, souffle::RamDomain> varValues;
    static std::string toString(const RuleApplication& ruleApplication) {
        std::string result = std::to_string(ruleApplication.ruleId) + "[" +
                             toStringVarValues(ruleApplication.varValues) + "]";
        return result;
    }
    static std::string toStringVarValues(const std::map<std::string, souffle::RamDomain>& varValues) {
        std::string result;
        bool first = true;
        // may use StreamUtil::join()
        for (const auto& [var, value] : varValues) {
            if (first) {
                first = false;
                result += var + "->" + std::to_string(value);
            } else {
                result += "," + var + "->" + std::to_string(value);
            }
        }
        return result;
    }
    bool operator<(const RuleApplication& other) const {
        if (ruleId != other.ruleId) {
            return ruleId < other.ruleId;
        }
        // size should equal; vars should equal
        assert(varValues.size() == other.varValues.size());
        for (const auto& [var, value1] : varValues) {
            assert(other.varValues.find(var) != other.varValues.end());
            const auto& value2 = other.varValues.at(var);
            if (value1 != value2) {
                return value1 < value2;
            }
        }
        return false;
    }
};

inline std::map<std::string, souffle::RamDomain> testVarValues = {{"x", 1}, {"y", 2}};
inline RuleApplication testRuleApplication{47906, testVarValues};

class DerivationManager {
public:
// private:
    // TODO
    // mapping from tuple's pomainter to rules that derive it
    static inline std::set<souffle::RamDomain> testRules = {
        0, 1, 2, 42
    };
    static inline std::map<UntypedTuple, std::set<RuleApplication>*> untypedTuple2RuleApplications = {
        // {testUntypedTuple, &testRules}
    };
    // mapping from tuple's pointer to its (untyped) real representation, i.e., relation name and value list
    // static inline std::map<const void*, UntypedTuple*> tuplePtr2UntypedTuple = {
    //     // {testUntypedTuple, &testUntypedTuple}
    // };

    static std::string ruleApplications2Str(const std::set<RuleApplication>* ruleApplications) {
        assert(ruleApplications != nullptr && !ruleApplications->empty() && "null ruleSet");
        std::string result = "[";
        bool first = true;
        for (auto& ruleApplication : *ruleApplications) {
            if (first) {
                first = false;
                result += RuleApplication::toString(ruleApplication);
            } else {
                result += "," + RuleApplication::toString(ruleApplication);
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
        for (const auto& [tuple, ruleApplicationSet] : untypedTuple2RuleApplications) {
            os << UntypedTuple::toString(tuple) << '\t';
            os << ruleApplications2Str(ruleApplicationSet) << std::endl;
        }
        os.close();
    }
};


#endif //DERIVATION_H
