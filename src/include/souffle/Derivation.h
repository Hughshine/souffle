//
// Created by Hugh on 2024/11/13.
//

#ifndef DERIVATION_H
#define DERIVATION_H

#include "souffle/RamTypes.h"
#include "souffle/SouffleInterface.h"
#include "souffle/utility/json11.h"

#include <chrono>
#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

class Debugger;

std::string generateFilename(const std::string& prefix = "log", const std::string& suffix = ".txt");
std::string basenameFromPath(const std::string& path);

class FunctionTimer {
private:
    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using Duration = std::chrono::duration<double>;

    std::string function_name_;
    TimePoint start_time_;
    bool print_on_destruction_;
    Debugger& debugger;

public:
    explicit FunctionTimer(const std::string& name = "Function", bool print_on_destruction = true);
    ~FunctionTimer();

    double getElapsedTime() const;
    void printElapsedTime() const;
    void reset();
};

// Utility function to time any function call
template<typename Func, typename... Args>
double timeFunction(const std::string& name, Func&& func, Args&&... args) {
    FunctionTimer timer(name, false);
    std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
    return timer.getElapsedTime();
}

struct UntypedTuple {
    std::string relation_name;
    std::vector<souffle::RamDomain> fields;
    static std::string toString(const UntypedTuple& tuple);
    std::string toString() const;
    json11::Json toJson() const;
    static std::string toStringFields(const std::vector<souffle::RamDomain>& fields);
    bool operator<(const UntypedTuple& other) const;
    bool operator==(const UntypedTuple& other) const;
    bool operator!=(const UntypedTuple& other) const;

    template<std::size_t N>
    static UntypedTuple fromTypedTuple(
            const std::string& relationName, const souffle::Tuple<souffle::RamDomain, N>& typedTuple) {
        UntypedTuple result;
        result.relation_name = relationName;
        for (const auto& field : typedTuple) {
            result.fields.push_back(field);
        }
        return result;
    }

    // for nullary
    static UntypedTuple fromTypedTuple(const std::string& relationName, const int* const&);
    static UntypedTuple fromSouffleTuple(const souffle::tuple& tuple);
};

template <typename T>
inline void hash_combine(std::size_t& seed, const T& val) {
    seed ^= std::hash<T>{}(val) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

// Hash specialization
namespace std {
template <>
struct hash<UntypedTuple> {
    std::size_t operator()(const UntypedTuple& tup) const {
        std::size_t seed = std::hash<std::string>{}(tup.relation_name);
        for (const auto& x : tup.fields) {
            hash_combine(seed, x);  // RamDomain is just an int
        }
        return seed;
    }
};
}

extern UntypedTuple testUntypedTuple1;
extern UntypedTuple testUntypedTuple2;

struct RuleApplication {
    souffle::RamDomain ruleId{};
    std::vector<souffle::RamDomain> varValuesPure;
    bool operator==(const RuleApplication& other) const;

    static std::string toString(const RuleApplication& ruleApplication);
    std::string toString() const;
    static std::string toStringVarValues(const std::map<std::string, souffle::RamDomain>& varValues);
    static std::string toStringVarValuesPure(const std::vector<souffle::RamDomain>& values);
    bool operator<(const RuleApplication& other) const;
    json11::Json toJson() const;
};

// hash specialization
namespace std {
template <>
struct hash<RuleApplication> {
    std::size_t operator()(const RuleApplication& app) const {
        std::size_t seed = std::hash<souffle::RamDomain>{}(app.ruleId);
        for (const auto& value : app.varValuesPure) {
            hash_combine(seed, std::hash<souffle::RamDomain>{}(value));
        }
        return seed;
    }
};
}

/** Fact is trivially true; use the naiveRuleApplication for such cases when needed */
extern RuleApplication naiveRuleApplication;
extern std::vector<souffle::RamDomain> testVarValues;
extern RuleApplication testRuleApplication1;
extern RuleApplication testRuleApplication2;
extern RuleApplication testRuleApplication3;
extern RuleApplication testRuleApplication4;

extern std::unordered_set<RuleApplication> testRuleApplicationSet1;
extern std::unordered_set<RuleApplication> testRuleApplicationSet2;

// testDerivationInfo
extern std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*> testUntypedTuple2RuleApplications;

class DerivationManager {
public:
    static std::set<souffle::RamDomain> testRules;
    static std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*> untypedTuple2RuleApplications;
    static std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*> untypedTuple2DeltaInsertRuleApplications;
    static std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*> untypedTuple2DeltaDeleteRuleApplications;
    static std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*> untypedTuple2DeltaDeltaInsertRuleApplications;
    static std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*> untypedTuple2DeltaDeltaDeleteRuleApplications;

    static bool ruleAppExistsInCompleteSet(
            const UntypedTuple& untypedTuple, const RuleApplication& ruleAppl);
    static std::string ruleApplications2Str(const std::unordered_set<RuleApplication>* ruleApplications);
    static json11::Json ruleApp2Json(const RuleApplication& ruleApp);
    static json11::Json derivationInfo2Json(
            const std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*>& derivationInfo);
    static std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*> derivationInfoFromJson(
            const json11::Json& infoJson);
    static void derivationInfo2JsonFile(
            const std::string& originalFileName, const std::string& suffix,
            const std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*>& derivationInfo,
            const std::string& outputDir = "");
    static std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*> derivationInfoFromJsonFile(
            const std::string& originalFileName, const std::string& suffix, const std::string& inputDir = "");
    static void dumpDerivationInfo(const std::string& filename, const std::string& outputDir);
};

// relation string -> int mapping, for optimization, reuse string
extern std::unordered_set<UntypedTuple> inputFactSet;
bool isInputFact(UntypedTuple tuple);
void dumpInputFacts(std::ostream& os = std::cout);

extern std::unordered_map<UntypedTuple, double> fact_prob;
extern std::unordered_map<std::string, bool> relationHasProbFact;
extern bool detOptEnabled;
extern std::unordered_map<std::string, bool> relationIsDet;
inline bool isDetRelation(const std::string& rel) {
    auto it = relationIsDet.find(rel);
    return it != relationIsDet.end() && it->second;
}

extern std::map<std::string, std::set<UntypedTuple>> initialInputRelations;
void dumpInitialInputRelations(std::string filename = "");

#endif //DERIVATION_H
