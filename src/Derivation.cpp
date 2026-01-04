#include "souffle/Derivation.h"

#include "souffle/problog/debug/Debugger.h"
#include "souffle/utility/json11.h"

#include <cassert>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

std::string generateFilename(const std::string& prefix, const std::string& suffix) {
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

std::string basenameFromPath(const std::string& path) {
    if (path.empty()) {
        return path;
    }
    const std::string::size_type pos = path.find_last_of("/\\");
    if (pos == std::string::npos || pos + 1 >= path.size()) {
        return path;
    }
    return path.substr(pos + 1);
}

FunctionTimer::FunctionTimer(const std::string& name, bool print_on_destruction)
        : function_name_(name),
          start_time_(Clock::now()),
          print_on_destruction_(print_on_destruction),
          debugger(Debugger::getInstance()) {}

FunctionTimer::~FunctionTimer() {
    if (print_on_destruction_) {
        printElapsedTime();
    }
}

double FunctionTimer::getElapsedTime() const {
    TimePoint end_time = Clock::now();
    Duration duration = end_time - start_time_;
    return duration.count();
}

void FunctionTimer::printElapsedTime() const {
    double elapsed = getElapsedTime();
    std::cout << function_name_ << " took " << elapsed << " seconds" << std::endl;
}

void FunctionTimer::reset() {
    start_time_ = Clock::now();
}

std::string UntypedTuple::toString(const UntypedTuple& tuple) {
    std::string result = tuple.relation_name + '(' + toStringFields(tuple.fields) + ')';
    return result;
}

std::string UntypedTuple::toString() const {
    std::string result = relation_name + '(' + toStringFields(fields) + ')';
    return result;
}

json11::Json UntypedTuple::toJson() const {
    return json11::Json::object{
            {"rel", relation_name},
            {"fields", json11::Json::array(fields.begin(), fields.end())},
    };
}

std::string UntypedTuple::toStringFields(const std::vector<souffle::RamDomain>& fields) {
    std::string result;
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

bool UntypedTuple::operator<(const UntypedTuple& other) const {
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

bool UntypedTuple::operator==(const UntypedTuple& other) const {
    return relation_name == other.relation_name && fields == other.fields;
}

bool UntypedTuple::operator!=(const UntypedTuple& other) const {
    return !(*this == other);
}

UntypedTuple UntypedTuple::fromTypedTuple(const std::string& relationName, const int* const&) {
    return UntypedTuple{relationName, {}};
}

UntypedTuple UntypedTuple::fromSouffleTuple(const souffle::tuple& tuple) {
    UntypedTuple result;
    result.relation_name = tuple.getRelation().getName();
    result.fields.reserve(tuple.getRelation().getArity());
    for (size_t i = 0; i < tuple.getRelation().getArity(); i++) {
        result.fields.push_back(tuple[i]);
    }
    return result;
}

UntypedTuple testUntypedTuple1{"T", {0, -1, -2, -42}};
UntypedTuple testUntypedTuple2{"S", {0, 1, 2, 42}};

bool RuleApplication::operator==(const RuleApplication& other) const {
    return ruleId == other.ruleId && varValuesPure == other.varValuesPure;
}

std::string RuleApplication::toString(const RuleApplication& ruleApplication) {
    std::string result = std::to_string(ruleApplication.ruleId) + "[" +
                         toStringVarValuesPure(ruleApplication.varValuesPure) + "]";
    return result;
}

std::string RuleApplication::toString() const {
    std::string result = std::to_string(ruleId) + "[" + toStringVarValuesPure(varValuesPure) + "]";
    return result;
}

std::string RuleApplication::toStringVarValues(
        const std::map<std::string, souffle::RamDomain>& varValues) {
    std::string result;
    bool first = true;
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

std::string RuleApplication::toStringVarValuesPure(const std::vector<souffle::RamDomain>& values) {
    std::string result;
    bool first = true;
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

bool RuleApplication::operator<(const RuleApplication& other) const {
    if (ruleId != other.ruleId) {
        return ruleId < other.ruleId;
    }
    assert(varValuesPure.size() == other.varValuesPure.size());
    for (int i = 0; i < varValuesPure.size(); i++) {
        if (varValuesPure[i] != other.varValuesPure[i]) {
            return varValuesPure[i] < other.varValuesPure[i];
        }
    }
    return false;
}

json11::Json RuleApplication::toJson() const {
    json11::Json::array mapping;
    for (const auto& value : varValuesPure) {
        mapping.emplace_back(value);
    }
    json11::Json result = json11::Json::object{
            {{"ruleId", ruleId}, {"mapping", mapping}},
    };
    return result;
}

RuleApplication naiveRuleApplication{0, {}};
std::vector<souffle::RamDomain> testVarValues = {1, 2};
RuleApplication testRuleApplication1{1, testVarValues};
RuleApplication testRuleApplication2{2, testVarValues};
RuleApplication testRuleApplication3{3, testVarValues};
RuleApplication testRuleApplication4{114514, testVarValues};

std::unordered_set<RuleApplication> testRuleApplicationSet1{
        testRuleApplication1,
        testRuleApplication2,
        testRuleApplication3,
        testRuleApplication4,
};

std::unordered_set<RuleApplication> testRuleApplicationSet2{
        testRuleApplication4,
        testRuleApplication3,
        testRuleApplication2,
        testRuleApplication1,
};

std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*> testUntypedTuple2RuleApplications{
        {testUntypedTuple1, &testRuleApplicationSet1},
        {testUntypedTuple2, &testRuleApplicationSet2},
};

std::set<souffle::RamDomain> DerivationManager::testRules = {
        0,
        1,
        2,
        42,
};

std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*>
        DerivationManager::untypedTuple2RuleApplications = {};
std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*>
        DerivationManager::untypedTuple2DeltaInsertRuleApplications = {};
std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*>
        DerivationManager::untypedTuple2DeltaDeleteRuleApplications = {};
std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*>
        DerivationManager::untypedTuple2DeltaDeltaInsertRuleApplications = {};
std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*>
        DerivationManager::untypedTuple2DeltaDeltaDeleteRuleApplications = {};

bool DerivationManager::ruleAppExistsInCompleteSet(
        const UntypedTuple& untypedTuple, const RuleApplication& ruleAppl) {
    if (untypedTuple2RuleApplications[untypedTuple] == nullptr) {
        return false;
    }
    if (untypedTuple2RuleApplications[untypedTuple]->find(ruleAppl) !=
            untypedTuple2RuleApplications[untypedTuple]->end()) {
        return true;
    }
    return false;
}

std::string DerivationManager::ruleApplications2Str(
        const std::unordered_set<RuleApplication>* ruleApplications) {
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

json11::Json DerivationManager::ruleApp2Json(const RuleApplication& ruleApp) {
    json11::Json result = json11::Json();
    return result;
}

json11::Json DerivationManager::derivationInfo2Json(
        const std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*>& derivationInfo) {
    json11::Json::array result;
    for (const auto& [tuple, ruleApps] : derivationInfo) {
        json11::Json::array ruleAppsJson = json11::Json::array();
        if (ruleApps != nullptr) {
            for (const auto& ruleApp : *ruleApps) {
                ruleAppsJson.push_back(ruleApp.toJson());
            }
        }
        json11::Json item = json11::Json::object{
                {"tuple",
                        json11::Json::object{{"rel", tuple.relation_name},
                                {"fields", json11::Json::array(tuple.fields.begin(),
                                                   tuple.fields.end())}}},
                {"edges", ruleAppsJson}};
        result.emplace_back(item);
    }
    return result;
}

std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*>
DerivationManager::derivationInfoFromJson(const json11::Json& infoJson) {
    std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*> derivationInfo;
    for (const auto& item : infoJson.array_items()) {
        std::vector<souffle::RamDomain> fields;
        for (const auto& field : item["tuple"]["fields"].array_items()) {
            fields.push_back(field.int_value());
        }
        UntypedTuple tuple{item["tuple"]["rel"].string_value(), fields};
        if (derivationInfo.count(tuple) == 0) {
            derivationInfo[tuple] = new std::unordered_set<RuleApplication>();
        }
        std::unordered_set<RuleApplication>* ruleApps = derivationInfo[tuple];
        for (const auto& ruleAppJson : item["edges"].array_items()) {
            RuleApplication ruleApp;
            souffle::RamDomain ruleId = ruleAppJson["ruleId"].int_value();
            ruleApp.ruleId = ruleId;
            for (const auto& map : ruleAppJson["mapping"].array_items()) {
                for (const auto& value : map.array_items()) {  // should be only one item here
                    ruleApp.varValuesPure.emplace_back(value.int_value());
                }
            }
            ruleApps->insert(ruleApp);
        }
        derivationInfo[tuple] = ruleApps;
    }
    return derivationInfo;
}

void DerivationManager::derivationInfo2JsonFile(
        const std::string& originalFileName, const std::string& suffix,
        const std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*>& derivationInfo,
        const std::string& outputDir) {
    json11::Json result = derivationInfo2Json(derivationInfo);
    std::string realFilename = std::filesystem::path(originalFileName).filename().string();

    std::ofstream outputFile(outputDir + "/" +
                             (suffix.empty() ? realFilename + ".json"
                                             : realFilename + "." + suffix + ".json"));

    outputFile << result.dump();
}

std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*>
DerivationManager::derivationInfoFromJsonFile(
        const std::string& originalFileName, const std::string& suffix, const std::string& inputDir) {
    std::string realFilename = std::filesystem::path(originalFileName).filename().string();
    std::ifstream inputFile(inputDir + "/" +
                            (suffix.empty() ? realFilename : realFilename + "." + suffix) +
                            ".json");
    if (inputFile.good()) {
        std::string infoString =
                std::string((std::istreambuf_iterator<char>(inputFile)), std::istreambuf_iterator<char>());
        std::string err;
        json11::Json infoJson = json11::Json::parse(infoString, err);
        assert(err.empty() && "Json parse error");
        return std::move(derivationInfoFromJson(infoJson));
    }
    std::cout << "derivation info inputDir: "
              << inputDir + "/" +
                         (suffix.empty() ? realFilename : realFilename + "." + suffix) + ".json"
              << std::endl;
    assert(false && "not impl");
    return {};
}

void DerivationManager::dumpDerivationInfo(const std::string& filename, const std::string& outputDir) {
    std::string baseFilename = std::filesystem::path(filename).filename().string();

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

std::unordered_set<UntypedTuple> inputFactSet;

bool isInputFact(UntypedTuple tuple) {
    return inputFactSet.count(tuple) != 0;
}

void dumpInputFacts(std::ostream& os) {
    for (auto& tuple : inputFactSet) {
        os << UntypedTuple::toString(tuple) << '\n';
    }
}

std::unordered_map<UntypedTuple, double> fact_prob;
std::unordered_map<std::string, bool> relationHasProbFact;
bool detOptEnabled = false;
std::unordered_map<std::string, bool> relationIsDet;

std::map<std::string, std::set<UntypedTuple>> initialInputRelations;

void dumpInitialInputRelations(std::string filename) {
    std::ostream* os;
    std::ofstream ofs;
    if (!filename.empty()) {
        ofs.open(filename);
        os = &ofs;
    } else {
        os = &std::cout;
    }
    for (const auto& [rel, tuples] : initialInputRelations) {
        if (tuples.empty()) continue;
        *os << "Relation: " << rel << '\n';
        for (const auto& tuple : tuples) {
            *os << "  " << tuple.toString() << '\n';
        }
    }
    if (ofs.is_open()) {
        ofs.close();
    }
}
