//
// Created by Hugh on 2024/11/13.
//

#ifndef DERIVATION_H
#define DERIVATION_H
#include "souffle/RamTypes.h"
#include "utility/json11.h"

#include "souffle/problog/debug//Debugger.h"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>
#include "souffle/SouffleInterface.h"

inline std::string generateFilename(const std::string& prefix = "log", const std::string& suffix = ".txt") {
    std::time_t now = std::time(nullptr);
    std::tm* local = std::localtime(&now);

    std::ostringstream oss;
    oss << prefix << "_"
        << (1900 + local->tm_year)
        << (local->tm_mon + 1)
        << local->tm_mday << "_"
        << local->tm_hour
        << local->tm_min
        << local->tm_sec
        << suffix;

    return oss.str();
}

// TODO: move to another file
class FunctionTimer {
private:
    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using Duration = std::chrono::duration<double>;

    std::string function_name_;
    TimePoint start_time_;
    bool print_on_destruction_;

    Debugger& debugger = Debugger::getInstance();

public:
    // Constructor with optional function name
    explicit FunctionTimer(const std::string& name = "Function", bool print_on_destruction = true)
        : function_name_(name),
          start_time_(Clock::now()),
          print_on_destruction_(print_on_destruction) {}

    // Destructor automatically prints time if enabled
    ~FunctionTimer() {
        if (print_on_destruction_) {
            printElapsedTime();
        }
    }

    // Get elapsed time in seconds
    double getElapsedTime() const {
        TimePoint end_time = Clock::now();
        Duration duration = end_time - start_time_;
        return duration.count();
    }

    // Print elapsed time
    void printElapsedTime() const {
        double elapsed = getElapsedTime();
        std::cout << function_name_ << " took " << elapsed << " seconds" << std::endl;
    }

    // Reset the timer
    void reset() {
        start_time_ = Clock::now();
    }
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
    static std::string toString(const UntypedTuple& tuple) {
        std::string result = tuple.relation_name + '(' + toStringFields(tuple.fields) + ')';
        return result;
    }

    std::string toString() const {
        std::string result = relation_name + '(' + toStringFields(fields) + ')';
        return result;
    }

    json11::Json toJson() const {
        // json11::Json::array fields = json11::Json::array();
        // for (const auto& field : fields) {
        //     fields.emplace_back(field);
        // }
        return json11::Json::object{
            {"rel", relation_name},
            {"fields", json11::Json::array(fields.begin(), fields.end())}
        };
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

    // bool operator==(const UntypedTuple& other) const {
    //     if (relation_name != other.relation_name) {
    //         return false;
    //     }
    //     if (fields.size() != other.fields.size()) {
    //         return false;
    //     }
    //     for (size_t i = 0; i < fields.size(); i++) {
    //         if (fields[i] != other.fields[i]) {
    //             return false;
    //         }
    //     }
    //     return true;
    // }

    bool operator==(const UntypedTuple& other) const {
        return relation_name == other.relation_name && fields == other.fields;
    }

    template<std::size_t N>
    static UntypedTuple fromTypedTuple(const std::string& relationName, const souffle::Tuple<souffle::RamDomain, N>& typedTuple) {
        UntypedTuple result;
        result.relation_name = relationName;
        for (const auto& field : typedTuple) {
            result.fields.push_back(field);
        }
        return result;
    }

    // for nullary
    static UntypedTuple fromTypedTuple(const std::string& relationName, const int* const&) {
        return UntypedTuple{relationName, {}};
    }

    static UntypedTuple fromSouffleTuple(const souffle::tuple& tuple) {
        UntypedTuple result;
        result.relation_name = tuple.getRelation().getName();
        result.fields.reserve(tuple.getRelation().getArity());
        for (size_t i = 0; i < tuple.getRelation().getArity(); i++) {
            result.fields.push_back(tuple[i]);
        }
        return result;
    }
};

template <typename T>
inline void hash_combine(std::size_t& seed, const T& val) {
    seed ^= std::hash<T>{}(val) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

// hash 特化
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

inline UntypedTuple testUntypedTuple1{"T", {0, -1, -2, -42}};
inline UntypedTuple testUntypedTuple2{"S", {0, 1, 2, 42}};

struct RuleApplication {
    souffle::RamDomain ruleId{};
    // std::map<std::string, souffle::RamDomain> varValues;
    std::vector<souffle::RamDomain> varValuesPure;
    bool operator==(const RuleApplication& other) const {
        return ruleId == other.ruleId && varValuesPure == other.varValuesPure;
    }

    static std::string toString(const RuleApplication& ruleApplication) {
        std::string result = std::to_string(ruleApplication.ruleId) + "[" +
                             toStringVarValuesPure(ruleApplication.varValuesPure) + "]";
        return result;
    }
    std::string toString() const {
        std::string result = std::to_string(ruleId) + "[" +
                     toStringVarValuesPure(varValuesPure) + "]";
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
    static std::string toStringVarValuesPure(const std::vector<souffle::RamDomain>& values) {
        std::string result;
        bool first = true;
        // may use StreamUtil::join()
        for (const auto& value : values) {
            if (first) {
                first = false;
                result += std::to_string(value);
            } else {
                result += "," + std::to_string(value);
            }
        }
        return result;
    }
    bool operator<(const RuleApplication& other) const {
        if (ruleId != other.ruleId) {
            return ruleId < other.ruleId;
        }
        // size should equal; vars should equal
        // assert(varValues.size() == other.varValues.size());
        // for (const auto& [var, value1] : varValues) {
        //     assert(other.varValues.find(var) != other.varValues.end());
        //     const auto& value2 = other.varValues.at(var);
        //     if (value1 != value2) {
        //         return value1 < value2;
        //     }
        // }
        assert(varValuesPure.size() == other.varValuesPure.size());
        for (int i = 0; i < varValuesPure.size(); i++) {
            if (varValuesPure[i] != other.varValuesPure[i]) {
                return varValuesPure[i] < other.varValuesPure[i];
            }
        }
        return false;
    }

    json11::Json toJson() const {
        json11::Json::array mapping;
        // for (const auto& [var, value] : varValues) {
        //     mapping.emplace_back(
        //         json11::Json::object{{var, (value)}}
        //     );
        // }
        for (const auto& value : varValuesPure) {
            mapping.emplace_back(value);
        }
        json11::Json result = json11::Json::object{
            {
                {"ruleId", ruleId},
                {"mapping", mapping},
            }
        };
        return result;
    }
};

// hash specialization
namespace std {
template <>
struct hash<RuleApplication> {
    std::size_t operator()(const RuleApplication& app) const {
        std::size_t seed = std::hash<souffle::RamDomain>{}(app.ruleId);
        // for (const auto& [key, value] : app.varValues) {
        //     hash_combine(seed, std::hash<std::string>{}(key));
        //     hash_combine(seed, std::hash<souffle::RamDomain>{}(value));
        // }
        for (const auto& value : app.varValuesPure) {
            // hash_combine(seed, std::hash<std::string>{}(key));
            hash_combine(seed, std::hash<souffle::RamDomain>{}(value));
        }
        return seed;
    }
};
}

/** Fact is trivially true; use the naiveRuleApplication for such cases when needed */
inline RuleApplication naiveRuleApplication{0, {}};
// {"x", 1}, {"y", 2}
inline std::vector<souffle::RamDomain> testVarValues = {1, 2};
inline RuleApplication testRuleApplication1{1, testVarValues};
inline RuleApplication testRuleApplication2{2, testVarValues};
inline RuleApplication testRuleApplication3{3, testVarValues};
inline RuleApplication testRuleApplication4{114514, testVarValues};

inline std::unordered_set<RuleApplication> testRuleApplicationSet1{
    testRuleApplication1, testRuleApplication2,
    testRuleApplication3, testRuleApplication4,
};

inline std::unordered_set<RuleApplication> testRuleApplicationSet2{
    testRuleApplication4, testRuleApplication3,
    testRuleApplication2, testRuleApplication1,
};

// testDerivationInfo
inline std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*> testUntypedTuple2RuleApplications{
    {testUntypedTuple1, &testRuleApplicationSet1},
    {testUntypedTuple2, &testRuleApplicationSet2},
};


class DerivationManager {
public:
// private:
    // TODO
    static inline std::set<souffle::RamDomain> testRules = {
        0, 1, 2, 42
    };
    static inline std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*> untypedTuple2RuleApplications = {
        // {testUntypedTuple, &testRules}
    };
    // for incremental computation: delta insert
    static inline std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*> untypedTuple2DeltaInsertRuleApplications = {
        // {testUntypedTuple, &testRules}
    };
    // for incremental computation: delta deleteC
    static inline std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*> untypedTuple2DeltaDeleteRuleApplications = {
        // {testUntypedTuple, &testRules}
    };

    // for incremental + recursive computation: delta insert
    static inline std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*> untypedTuple2DeltaDeltaInsertRuleApplications = {
        // {testUntypedTuple, &testRules}
    };
    // for incremental + recursive computation: delta deleteC
    static inline std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*> untypedTuple2DeltaDeltaDeleteRuleApplications = {
        // {testUntypedTuple, &testRules}
    };


    static bool ruleAppExistsInCompleteSet(const UntypedTuple& untypedTuple, const RuleApplication& ruleAppl) {
        if (untypedTuple2RuleApplications[untypedTuple] == nullptr) {
            return false;
        }
        if (untypedTuple2RuleApplications[untypedTuple]->find(ruleAppl) != untypedTuple2RuleApplications[untypedTuple]->end()) {
            return true;
        }
        return false;
    };

    static std::string ruleApplications2Str(const std::unordered_set<RuleApplication>* ruleApplications) {
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

    static json11::Json ruleApp2Json(const RuleApplication& ruleApp) {
        json11::Json result = json11::Json();
        return result;
    }
    /**
     * Derivation Info: {A(1)	[1[x->1],2[x->1]]}
     * JSON: [{}]
     */
    static json11::Json derivationInfo2Json(const std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*>& derivationInfo) {
        json11::Json::array result;
        for (const auto& [tuple, ruleApps]: derivationInfo) {
            json11::Json::array ruleAppsJson = json11::Json::array();
            if (ruleApps != nullptr) {
                for (const auto& ruleApp: *ruleApps) {
                    ruleAppsJson.push_back(ruleApp.toJson());
                }
            }
            json11::Json item = json11::Json::object{
                {"tuple", json11::Json::object{
                    {"rel", tuple.relation_name},
                    {"fields", json11::Json::array(tuple.fields.begin(), tuple.fields.end())}}
                },
                {"edges", ruleAppsJson}
            };
            result.emplace_back(item);
        }
        return result;
    }

    static std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*> derivationInfoFromJson(const json11::Json& infoJson) {
        std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*> derivationInfo;
        for (const auto& item: infoJson.array_items()) {
            std::vector<souffle::RamDomain> fields;
            for (const auto& field: item["tuple"]["fields"].array_items()) {
                fields.push_back(field.int_value());
            }
            UntypedTuple tuple{item["tuple"]["rel"].string_value(), fields};
            if (derivationInfo.count(tuple) == 0) {
                derivationInfo[tuple] = new std::unordered_set<RuleApplication>();
            }
            std::unordered_set<RuleApplication>* ruleApps = derivationInfo[tuple];
            for (const auto& ruleAppJson: item["edges"].array_items()) {
                RuleApplication ruleApp;
                souffle::RamDomain ruleId = ruleAppJson["ruleId"].int_value();
                ruleApp.ruleId = ruleId;
                for (const auto& map: ruleAppJson["mapping"].array_items()) {
                    for (const auto& value: map.array_items()) { // should be only one item here
                        ruleApp.varValuesPure.emplace_back(value.int_value());
                    }
                }
                ruleApps->insert(ruleApp);
            }
            derivationInfo[tuple] = ruleApps;
        }
        // parse json to map
        return derivationInfo;
    }


    static void derivationInfo2JsonFile(const std::string& originalFileName, const std::string& suffix, const std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*>& derivationInfo, const std::string& outputDir = "") {
        json11::Json result = derivationInfo2Json(derivationInfo);
        std::string realFilename = std::filesystem::path(originalFileName).filename().string();

        std::ofstream outputFile(
            outputDir + "/" +
            (suffix.empty()
                ? realFilename + ".json"
                : realFilename + "." + suffix + ".json"
            ));

        outputFile << result.dump();
    }


    static std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*> derivationInfoFromJsonFile(const std::string& originalFileName, const std::string& suffix, const std::string& inputDir = "") {
        std::string realFilename = std::filesystem::path(originalFileName).filename().string();
        std::ifstream inputFile(inputDir + "/" + (suffix.empty() ? realFilename : realFilename + "." + suffix ) + ".json");
        if (inputFile.good()) {
            std::string infoString = std::string((std::istreambuf_iterator<char>(inputFile)),
                           std::istreambuf_iterator<char>());;
            std::string err;
            json11::Json infoJson = json11::Json::parse(infoString, err);
            assert (err.empty() && "Json parse error");
            return std::move(derivationInfoFromJson(infoJson));
        }
        std::cout << "derivation info inputDir: " << inputDir + "/" + (suffix.empty() ? realFilename : realFilename + "." + suffix ) + ".json" << std::endl;
        assert (false && "not impl");
        return {};  // if no cached result, start from the beginning; delta inputs should be empty too because this is the bootstrapping case TODO
    }

    // static void dumpDerivationInfo2JsonFile(const std::string& originalFileName, const std::string& suffix, const std::set<RuleApplication>* ruleApplications) {
    //
    // }

    static void dumpDerivationInfo(const std::string& filename, const std::string& outputDir) {
        // TODO: should change to souffle's IO system later
        std::string baseFilename = std::filesystem::path(filename).filename().string();

        // std::cout << "derivation info outputDir: " << outputDir << std::endl;
        if (baseFilename.size() >= 3 && baseFilename.substr(baseFilename.size() - 3) == ".dl") {
            baseFilename = baseFilename.substr(0, baseFilename.size() - 3);
        }
        {
            std::string derivationInfoFilename = baseFilename + "-derivation-info.txt";
            std::ofstream os{outputDir + "/" + derivationInfoFilename};
            for (const auto& [tuple, ruleApplicationSet] : untypedTuple2RuleApplications) {
                os << UntypedTuple::toString(tuple) << '\t';
                os << ruleApplications2Str(ruleApplicationSet) << std::endl;
            }
            os.close();
        }
        {
            std::string deltaInsDerivationInfoFilename = baseFilename + "-delta-insert-derivation-info.txt";
            std::ofstream os{outputDir + "/" + deltaInsDerivationInfoFilename};
            for (const auto& [tuple, ruleApplicationSet] : untypedTuple2DeltaInsertRuleApplications) {
                os << UntypedTuple::toString(tuple) << '\t';
                os << ruleApplications2Str(ruleApplicationSet) << std::endl;
            }
            os.close();
        }
        {
            std::string deltaDelDerivationInfoFilename = baseFilename + "-delta-delete-derivation-info.txt";
            std::ofstream os{outputDir + "/" + deltaDelDerivationInfoFilename};
            for (const auto& [tuple, ruleApplicationSet] : untypedTuple2DeltaDeleteRuleApplications) {
                os << UntypedTuple::toString(tuple) << '\t';
                os << ruleApplications2Str(ruleApplicationSet) << std::endl;
            }
            os.close();
        }
    }
};

// relation string -> int mapping, for optimization, reuse string
static std::unordered_set<UntypedTuple> inputFactSet;
bool isInputFact(UntypedTuple tuple) {
    return inputFactSet.count(tuple) != 0;
}
void dumpInputFacts(std::ostream& os = std::cout) {
    for (auto& tuple : inputFactSet) {
        os << UntypedTuple::toString(tuple) << '\n';
    }
}

static std::unordered_map<UntypedTuple, double> fact_prob;
#endif //DERIVATION_H
