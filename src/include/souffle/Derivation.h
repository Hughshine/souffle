//
// Created by Hugh on 2024/11/13.
//

#ifndef DERIVATION_H
#define DERIVATION_H
#include "souffle/RamTypes.h"
#include "utility/json11.h"

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

inline UntypedTuple testUntypedTuple1{"T", {0, -1, -2, -42}};
inline UntypedTuple testUntypedTuple2{"S", {0, 1, 2, 42}};

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

    json11::Json toJson() const {
        json11::Json::array mapping;
        for (const auto& [var, value] : varValues) {
            mapping.emplace_back(
                json11::Json::object{{var, (value)}}
            );
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

/** Fact is trivially true; use the naiveRuleApplication for such cases when needed */
inline RuleApplication naiveRuleApplication{0, {}};

inline std::map<std::string, souffle::RamDomain> testVarValues = {{"x", 1}, {"y", 2}};
inline RuleApplication testRuleApplication1{1, testVarValues};
inline RuleApplication testRuleApplication2{2, testVarValues};
inline RuleApplication testRuleApplication3{3, testVarValues};
inline RuleApplication testRuleApplication4{114514, testVarValues};

inline std::set<RuleApplication> testRuleApplicationSet1{
    testRuleApplication1, testRuleApplication2,
    testRuleApplication3, testRuleApplication4,
};

inline std::set<RuleApplication> testRuleApplicationSet2{
    testRuleApplication4, testRuleApplication3,
    testRuleApplication2, testRuleApplication1,
};

// testDerivationInfo
inline std::map<UntypedTuple, std::set<RuleApplication>*> testUntypedTuple2RuleApplications{
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
    static inline std::map<UntypedTuple, std::set<RuleApplication>*> untypedTuple2RuleApplications = {
        // {testUntypedTuple, &testRules}
    };
    // for incremental computation: delta insert
    static inline std::map<UntypedTuple, std::set<RuleApplication>*> untypedTuple2DeltaInsertRuleApplications = {
        // {testUntypedTuple, &testRules}
    };
    // for incremental computation: delta delete
    static inline std::map<UntypedTuple, std::set<RuleApplication>*> untypedTuple2DeltaDeleteRuleApplications = {
        // {testUntypedTuple, &testRules}
    };

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

    static json11::Json ruleApp2Json(const RuleApplication& ruleApp) {
        json11::Json result = json11::Json();
        return result;
    }
    /**
     * Derivation Info: {A(1)	[1[x->1],2[x->1]]}
     * JSON: [{}]
     */
    static json11::Json derivationInfo2Json(const std::map<UntypedTuple, std::set<RuleApplication>*>& derivationInfo) {
        json11::Json::array result;
        for (const auto& [tuple, ruleApps]: derivationInfo) {
            json11::Json::array ruleAppsJson = json11::Json::array();
            for (const auto& ruleApp: *ruleApps) {
                ruleAppsJson.push_back(ruleApp.toJson());
            }
            json11::Json item = json11::Json::object{
                {"tuple", json11::Json::object{
                    {"rel", tuple.relation_name},
                    {"fields", json11::Json::array(tuple.fields.begin(), tuple.fields.end())}}
                },
                {"edge", ruleAppsJson}
            };
            result.emplace_back(item);
        }
        return result;
    }

    static std::map<UntypedTuple, std::set<RuleApplication>*> derivationInfoFromJson(const json11::Json& infoJson) {
        std::map<UntypedTuple, std::set<RuleApplication>*> derivationInfo;
        for (const auto& item: infoJson.array_items()) {
            std::vector<souffle::RamDomain> fields;
            for (const auto& field: item["tuple"]["fields"].array_items()) {
                fields.push_back(field.int_value());
            }
            UntypedTuple tuple{item["tuple"]["rel"].string_value(), fields};
            if (derivationInfo.count(tuple) == 0) {
                derivationInfo[tuple] = new std::set<RuleApplication>();
            }
            std::set<RuleApplication>* ruleApps = derivationInfo[tuple];
            for (const auto& ruleAppJson: item["edge"].array_items()) {
                RuleApplication ruleApp;
                souffle::RamDomain ruleId = ruleAppJson["ruleId"].int_value();
                ruleApp.ruleId = ruleId;
                for (const auto& map: ruleAppJson["mapping"].array_items()) {
                    for (const auto& [var, value]: map.object_items()) { // should be only one item here
                        ruleApp.varValues[var] = value.int_value();
                    }
                }
                ruleApps->insert(ruleApp);
            }
            derivationInfo[tuple] = ruleApps;
        }
        // parse json to map
        return derivationInfo;
    }


    static void derivationInfo2JsonFile(const std::string& originalFileName, const std::string& suffix, const std::map<UntypedTuple, std::set<RuleApplication>*>& derivationInfo) {
        json11::Json result = derivationInfo2Json(derivationInfo);

        std::ofstream outputFile(
            suffix.empty()
                ? originalFileName + ".json"
                : originalFileName + "." + suffix + ".json"
            );

        outputFile << result.dump();
    }


    static std::map<UntypedTuple, std::set<RuleApplication>*> derivationInfoFromJsonFile(const std::string& originalFileName, const std::string& suffix) {
        std::ifstream inputFile((suffix.empty() ? originalFileName : originalFileName + "." + suffix ) + ".json");
        if (inputFile.good()) {
            std::string infoString = std::string((std::istreambuf_iterator<char>(inputFile)),
                           std::istreambuf_iterator<char>());;
            std::string err;
            json11::Json infoJson = json11::Json::parse(infoString, err);
            assert (err.empty() && "Json parse error");
            return std::move(derivationInfoFromJson(infoJson));
        }
        return {};  // if no cached result, start from the beginning; delta inputs should be empty too because this is the bootstrapping case TODO
    }

    // static void dumpDerivationInfo2JsonFile(const std::string& originalFileName, const std::string& suffix, const std::set<RuleApplication>* ruleApplications) {
    //
    // }

    static void dumpDerivationInfo(const std::string& filename, const std::string& outputDir) {
        // TODO: should change to souffle's IO system later
        std::string baseFilename = filename;
        if (baseFilename.size() >= 3 && baseFilename.substr(baseFilename.size() - 3) == ".dl") {
            baseFilename = baseFilename.substr(0, baseFilename.size() - 3);
        }
        {
            std::string derivationInfoFilename = baseFilename + "-derivation-info.txt";
            std::ofstream os{derivationInfoFilename};
            for (const auto& [tuple, ruleApplicationSet] : untypedTuple2RuleApplications) {
                os << UntypedTuple::toString(tuple) << '\t';
                os << ruleApplications2Str(ruleApplicationSet) << std::endl;
            }
            os.close();
        }
        {
            std::string deltaInsDerivationInfoFilename = baseFilename + "-delta-insert-derivation-info.txt";
            std::ofstream os{deltaInsDerivationInfoFilename};
            for (const auto& [tuple, ruleApplicationSet] : untypedTuple2DeltaInsertRuleApplications) {
                os << UntypedTuple::toString(tuple) << '\t';
                os << ruleApplications2Str(ruleApplicationSet) << std::endl;
            }
            os.close();
        }
        {
            std::string deltaDelDerivationInfoFilename = baseFilename + "-delta-delete-derivation-info.txt";
            std::ofstream os{deltaDelDerivationInfoFilename};
            for (const auto& [tuple, ruleApplicationSet] : untypedTuple2DeltaDeleteRuleApplications) {
                os << UntypedTuple::toString(tuple) << '\t';
                os << ruleApplications2Str(ruleApplicationSet) << std::endl;
            }
            os.close();
        }
    }
};


#endif //DERIVATION_H
