#include "souffle/Derivation.h"

#include "souffle/problog/debug/Debugger.h"

#include <cassert>
#include <chrono>
#include <ctime>
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

namespace {
bool functionTimerOutputEnabled = false;
}

void setFunctionTimerOutputEnabled(bool enabled) {
    functionTimerOutputEnabled = enabled;
}

bool isFunctionTimerOutputEnabled() {
    return functionTimerOutputEnabled;
}

FunctionTimer::FunctionTimer(const std::string& name, bool print_on_destruction)
        : function_name_(name),
          start_time_(Clock::now()),
          print_on_destruction_(print_on_destruction),
          debugger(Debugger::getInstance()) {}

FunctionTimer::~FunctionTimer() {
    if (print_on_destruction_ && isFunctionTimerOutputEnabled()) {
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

RuleApplication naiveRuleApplication{0, {}};

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
DerivationManager::DredStats DerivationManager::dredStats = {};
std::vector<DerivationManager::DredSccStats> DerivationManager::dredSccStats = {};
std::size_t DerivationManager::dredCurrentScc = DerivationManager::kInvalidDredScc;
bool DerivationManager::semStatsEnabled = false;
std::unordered_set<UntypedTuple> DerivationManager::detDeltaDeleteTuples = {};
std::unordered_set<UntypedTuple> DerivationManager::detDeltaInsertTuples = {};
bool dredProfileEnabled = false;
bool incProfileEnabled = false;
bool fcProfileEnabled = false;
bool incDeleteProfileEnabled = false;
bool wmcProfileEnabled = false;
bool incRegionalProfileEnabled = false;
bool depGraphProfileEnabled = false;
bool reuseVarIndexEnabled = true;
std::string incReorderPolicy = "pressure";
std::size_t incReorderAutoGap = 0;
std::size_t incReorderWorkThreshold = 2500;
bool incReorderCountDead = false;
bool incReorderAllowLarge = false;
std::map<std::uintptr_t, std::size_t> incReorderAccumulatedWorkScore;

void DerivationManager::clearDetDeltaTuples() {
    detDeltaDeleteTuples.clear();
    detDeltaInsertTuples.clear();
}

void DerivationManager::recordDetDeltaDelete(const UntypedTuple& tuple) {
    detDeltaDeleteTuples.insert(tuple);
}

void DerivationManager::recordDetDeltaInsert(const UntypedTuple& tuple) {
    detDeltaInsertTuples.insert(tuple);
}

const std::unordered_set<UntypedTuple>& DerivationManager::getDetDeltaDeleteTuples() {
    return detDeltaDeleteTuples;
}

const std::unordered_set<UntypedTuple>& DerivationManager::getDetDeltaInsertTuples() {
    return detDeltaInsertTuples;
}

void DerivationManager::freeRuleApplicationMap(
        std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*>& derivationInfo) {
    for (auto& [_, ruleSet] : derivationInfo) {
        delete ruleSet;
        ruleSet = nullptr;
    }
    derivationInfo.clear();
}

void DerivationManager::DredStats::dump(std::ostream& out, const std::string& label) const {
    out << "[seminaive-dred] " << label << " del"
        << " delta_tuples=" << del_delta_tuples
        << " delta_ruleapps=" << del_delta_ruleapps
        << " ruleapps_recorded=" << del_ruleapp_recorded
        << " ruleapps_delta_delta=" << del_ruleapp_delta_delta
        << " ruleapps_overdelete=" << del_ruleapp_overdelete
        << " ruleapps_erased=" << del_ruleapp_erases
        << " complete_scan_calls=" << del_complete_scan_calls
        << " complete_scan_elems=" << del_complete_scan_elems
        << " tuples_deleted=" << del_tuple_deletes
        << " complete_sets_freed=" << del_complete_sets_freed
        << " time_total_ns=" << del_time_total_ns
        << " time_copy_old_ns=" << del_time_copy_old_ns
        << " time_preamble_ns=" << del_time_preamble_ns
        << " time_prefill_ns=" << del_time_prefill_ns
        << " time_prefill_update_ns=" << del_time_prefill_update_ns
        << " time_loop_body_ns=" << del_time_loop_body_ns
        << " time_loop_exit_ns=" << del_time_loop_exit_ns
        << " time_loop_update_ns=" << del_time_loop_update_ns
        << " time_postamble_ns=" << del_time_postamble_ns
        << " time_record_ns=" << del_time_record_ns
        << " time_overdelete_ns=" << del_time_overdelete_ns
        << " time_delta_union_ns=" << del_time_delta_union_ns
        << " time_ruleapp_erase_ns=" << del_time_ruleapp_erase_ns
        << "\n";
    out << "[seminaive-dred] " << label << " ins"
        << " delta_tuples=" << ins_delta_tuples
        << " delta_ruleapps=" << ins_delta_ruleapps
        << " ruleapps_recorded=" << ins_ruleapp_recorded
        << " ruleapps_delta_delta=" << ins_ruleapp_delta_delta
        << " ruleapps_rederive_erased=" << ins_ruleapp_rederive_erases
        << " ruleapps_merged=" << ins_ruleapp_merges
        << " rederive_delta_tuples=" << rederive_delta_tuples
        << " rederive_delta_ruleapps=" << rederive_delta_ruleapps
        << " tuples_inserted=" << ins_tuple_inserts
        << " complete_sets_attached=" << ins_complete_sets_attached
        << " time_total_ns=" << ins_time_total_ns
        << " time_preamble_ns=" << ins_time_preamble_ns
        << " time_prefill_ns=" << ins_time_prefill_ns
        << " time_prefill_update_ns=" << ins_time_prefill_update_ns
        << " time_loop_body_ns=" << ins_time_loop_body_ns
        << " time_loop_exit_ns=" << ins_time_loop_exit_ns
        << " time_loop_update_ns=" << ins_time_loop_update_ns
        << " time_postamble_ns=" << ins_time_postamble_ns
        << " time_record_ns=" << ins_time_record_ns
        << " time_delta_union_ns=" << ins_time_delta_union_ns
        << " rederive_time_total_ns=" << red_time_total_ns
        << " rederive_time_loop_body_ns=" << red_time_loop_body_ns
        << " rederive_time_loop_exit_ns=" << red_time_loop_exit_ns
        << " rederive_time_loop_update_ns=" << red_time_loop_update_ns
        << " rederive_time_postamble_ns=" << red_time_postamble_ns
        << "\n";
}

void DerivationManager::setSemStatsEnabled(bool enabled) {
    semStatsEnabled = enabled;
}

bool DerivationManager::isSemStatsEnabled() {
    return semStatsEnabled;
}

void DerivationManager::resetDredStats() {
    dredStats.reset();
    dredSccStats.clear();
    dredCurrentScc = kInvalidDredScc;
}

void DerivationManager::dumpDredStats(std::ostream& out, const std::string& label) {
    if (!semStatsEnabled) {
        return;
    }
    dredStats.dump(out, label);
    dumpDredSccStats(out, label);
}

void DerivationManager::dumpDredSccStats(std::ostream& out, const std::string& label) {
    if (!semStatsEnabled || dredSccStats.empty()) {
        return;
    }
    for (std::size_t sccId = 0; sccId < dredSccStats.size(); ++sccId) {
        const auto& stats = dredSccStats[sccId];
        if (stats.del_ruleapp_overdelete == 0 && stats.del_complete_scan_calls == 0 &&
                stats.del_complete_scan_elems == 0 && stats.rederive_delta_tuples == 0 &&
                stats.rederive_delta_ruleapps == 0 && stats.rederive_ruleapp_erases == 0 &&
                stats.del_time_total_ns == 0 && stats.del_time_loop_body_ns == 0 &&
                stats.del_time_loop_update_ns == 0 && stats.ins_time_total_ns == 0 &&
                stats.ins_time_loop_body_ns == 0 && stats.ins_time_loop_update_ns == 0 &&
                stats.red_time_total_ns == 0 && stats.red_time_loop_body_ns == 0 &&
                stats.red_time_loop_update_ns == 0) {
            continue;
        }
        out << "[seminaive-dred-scc] " << label << " scc=" << sccId
            << " ruleapps_overdelete=" << stats.del_ruleapp_overdelete
            << " complete_scan_calls=" << stats.del_complete_scan_calls
            << " complete_scan_elems=" << stats.del_complete_scan_elems
            << " rederive_delta_tuples=" << stats.rederive_delta_tuples
            << " rederive_delta_ruleapps=" << stats.rederive_delta_ruleapps
            << " ruleapps_rederive_erased=" << stats.rederive_ruleapp_erases
            << " del_time_total_ns=" << stats.del_time_total_ns
            << " del_time_loop_body_ns=" << stats.del_time_loop_body_ns
            << " del_time_loop_update_ns=" << stats.del_time_loop_update_ns
            << " ins_time_total_ns=" << stats.ins_time_total_ns
            << " ins_time_loop_body_ns=" << stats.ins_time_loop_body_ns
            << " ins_time_loop_update_ns=" << stats.ins_time_loop_update_ns
            << " rederive_time_total_ns=" << stats.red_time_total_ns
            << " rederive_time_loop_body_ns=" << stats.red_time_loop_body_ns
            << " rederive_time_loop_update_ns=" << stats.red_time_loop_update_ns
            << "\n";
    }
}

std::uint64_t DerivationManager::countRuleApplications(
        const std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*>& derivationInfo) {
    std::uint64_t total = 0;
    for (const auto& [tuple, ruleApplicationSet] : derivationInfo) {
        (void)tuple;
        if (ruleApplicationSet != nullptr) {
            total += static_cast<std::uint64_t>(ruleApplicationSet->size());
        }
    }
    return total;
}

std::uint64_t DerivationManager::countRuleApplicationTuples(
        const std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*>& derivationInfo) {
    std::uint64_t total = 0;
    for (const auto& [tuple, ruleApplicationSet] : derivationInfo) {
        (void)tuple;
        if (ruleApplicationSet != nullptr && !ruleApplicationSet->empty()) {
            total++;
        }
    }
    return total;
}

void DerivationManager::dumpRuleApplicationSummary(std::ostream& out, const std::string& label) {
    if (!semStatsEnabled) {
        return;
    }
    out << "[seminaive-ruleapps] " << label
        << " complete_tuples=" << countRuleApplicationTuples(untypedTuple2RuleApplications)
        << " complete_ruleapps=" << countRuleApplications(untypedTuple2RuleApplications)
        << " delta_insert_tuples=" << countRuleApplicationTuples(untypedTuple2DeltaInsertRuleApplications)
        << " delta_insert_ruleapps=" << countRuleApplications(untypedTuple2DeltaInsertRuleApplications)
        << " delta_delete_tuples=" << countRuleApplicationTuples(untypedTuple2DeltaDeleteRuleApplications)
        << " delta_delete_ruleapps=" << countRuleApplications(untypedTuple2DeltaDeleteRuleApplications)
        << " delta_delta_insert_tuples=" << countRuleApplicationTuples(untypedTuple2DeltaDeltaInsertRuleApplications)
        << " delta_delta_insert_ruleapps=" << countRuleApplications(untypedTuple2DeltaDeltaInsertRuleApplications)
        << " delta_delta_delete_tuples=" << countRuleApplicationTuples(untypedTuple2DeltaDeltaDeleteRuleApplications)
        << " delta_delta_delete_ruleapps=" << countRuleApplications(untypedTuple2DeltaDeltaDeleteRuleApplications)
        << "\n";
}

std::size_t DerivationManager::getDredCurrentScc() {
    return dredCurrentScc;
}

void DerivationManager::setDredCurrentScc(std::size_t sccId) {
    dredCurrentScc = sccId;
    if (dredCurrentScc == kInvalidDredScc) {
        return;
    }
    if (dredSccStats.size() <= dredCurrentScc) {
        dredSccStats.resize(dredCurrentScc + 1);
    }
}

static DerivationManager::DredSccStats* getDredSccStats() {
    if (DerivationManager::getDredCurrentScc() == DerivationManager::kInvalidDredScc) {
        return nullptr;
    }
    if (DerivationManager::dredSccStats.size() <= DerivationManager::getDredCurrentScc()) {
        DerivationManager::dredSccStats.resize(DerivationManager::getDredCurrentScc() + 1);
    }
    return &DerivationManager::dredSccStats[DerivationManager::getDredCurrentScc()];
}

void DerivationManager::bumpDredSccOverdelete(std::uint64_t inc) {
    if (auto* stats = getDredSccStats()) {
        stats->del_ruleapp_overdelete += inc;
    }
}

void DerivationManager::bumpDredSccCompleteScanCalls(std::uint64_t inc) {
    if (auto* stats = getDredSccStats()) {
        stats->del_complete_scan_calls += inc;
    }
}

void DerivationManager::bumpDredSccCompleteScanElems(std::uint64_t inc) {
    if (auto* stats = getDredSccStats()) {
        stats->del_complete_scan_elems += inc;
    }
}

void DerivationManager::bumpDredSccRederiveDeltaTuples(std::uint64_t inc) {
    if (auto* stats = getDredSccStats()) {
        stats->rederive_delta_tuples += inc;
    }
}

void DerivationManager::bumpDredSccRederiveDeltaRuleapps(std::uint64_t inc) {
    if (auto* stats = getDredSccStats()) {
        stats->rederive_delta_ruleapps += inc;
    }
}

void DerivationManager::bumpDredSccRederiveRuleappErases(std::uint64_t inc) {
    if (auto* stats = getDredSccStats()) {
        stats->rederive_ruleapp_erases += inc;
    }
}

void DerivationManager::addDredTime(DredTimeBucket bucket, std::uint64_t ns) {
    if (ns == 0) {
        return;
    }
    switch (bucket) {
        case DredTimeBucket::DelTotal:
            dredStats.del_time_total_ns += ns;
            if (auto* stats = getDredSccStats()) stats->del_time_total_ns += ns;
            break;
        case DredTimeBucket::DelCopyOld:
            dredStats.del_time_copy_old_ns += ns;
            break;
        case DredTimeBucket::DelPreamble:
            dredStats.del_time_preamble_ns += ns;
            break;
        case DredTimeBucket::DelPrefill:
            dredStats.del_time_prefill_ns += ns;
            break;
        case DredTimeBucket::DelPrefillUpdate:
            dredStats.del_time_prefill_update_ns += ns;
            break;
        case DredTimeBucket::DelLoopBody:
            dredStats.del_time_loop_body_ns += ns;
            if (auto* stats = getDredSccStats()) stats->del_time_loop_body_ns += ns;
            break;
        case DredTimeBucket::DelLoopExit:
            dredStats.del_time_loop_exit_ns += ns;
            break;
        case DredTimeBucket::DelLoopUpdate:
            dredStats.del_time_loop_update_ns += ns;
            if (auto* stats = getDredSccStats()) stats->del_time_loop_update_ns += ns;
            break;
        case DredTimeBucket::DelPostamble:
            dredStats.del_time_postamble_ns += ns;
            break;
        case DredTimeBucket::DelRecord:
            dredStats.del_time_record_ns += ns;
            break;
        case DredTimeBucket::DelOverdelete:
            dredStats.del_time_overdelete_ns += ns;
            break;
        case DredTimeBucket::DelDeltaUnion:
            dredStats.del_time_delta_union_ns += ns;
            break;
        case DredTimeBucket::DelRuleappErase:
            dredStats.del_time_ruleapp_erase_ns += ns;
            break;
        case DredTimeBucket::InsTotal:
            dredStats.ins_time_total_ns += ns;
            if (auto* stats = getDredSccStats()) stats->ins_time_total_ns += ns;
            break;
        case DredTimeBucket::InsPreamble:
            dredStats.ins_time_preamble_ns += ns;
            break;
        case DredTimeBucket::InsPrefill:
            dredStats.ins_time_prefill_ns += ns;
            break;
        case DredTimeBucket::InsPrefillUpdate:
            dredStats.ins_time_prefill_update_ns += ns;
            break;
        case DredTimeBucket::InsLoopBody:
            dredStats.ins_time_loop_body_ns += ns;
            if (auto* stats = getDredSccStats()) stats->ins_time_loop_body_ns += ns;
            break;
        case DredTimeBucket::InsLoopExit:
            dredStats.ins_time_loop_exit_ns += ns;
            break;
        case DredTimeBucket::InsLoopUpdate:
            dredStats.ins_time_loop_update_ns += ns;
            if (auto* stats = getDredSccStats()) stats->ins_time_loop_update_ns += ns;
            break;
        case DredTimeBucket::InsPostamble:
            dredStats.ins_time_postamble_ns += ns;
            break;
        case DredTimeBucket::InsRecord:
            dredStats.ins_time_record_ns += ns;
            break;
        case DredTimeBucket::InsDeltaUnion:
            dredStats.ins_time_delta_union_ns += ns;
            break;
        case DredTimeBucket::RedTotal:
            dredStats.red_time_total_ns += ns;
            if (auto* stats = getDredSccStats()) stats->red_time_total_ns += ns;
            break;
        case DredTimeBucket::RedLoopBody:
            dredStats.red_time_loop_body_ns += ns;
            if (auto* stats = getDredSccStats()) stats->red_time_loop_body_ns += ns;
            break;
        case DredTimeBucket::RedLoopExit:
            dredStats.red_time_loop_exit_ns += ns;
            break;
        case DredTimeBucket::RedLoopUpdate:
            dredStats.red_time_loop_update_ns += ns;
            if (auto* stats = getDredSccStats()) stats->red_time_loop_update_ns += ns;
            break;
        case DredTimeBucket::RedPostamble:
            dredStats.red_time_postamble_ns += ns;
            break;
    }
}

bool DerivationManager::ruleAppExistsInCompleteSet(
        const UntypedTuple& untypedTuple, const RuleApplication& ruleAppl) {
    auto it = untypedTuple2RuleApplications.find(untypedTuple);
    if (it == untypedTuple2RuleApplications.end() || it->second == nullptr) {
        return false;
    }
    if (it->second->find(ruleAppl) != it->second->end()) {
        return true;
    }
    return false;
}

std::unordered_set<UntypedTuple> inputFactSet;

bool isInputFact(UntypedTuple tuple) {
    return inputFactSet.count(tuple) != 0;
}

std::unordered_map<UntypedTuple, double> fact_prob;
std::unordered_map<std::string, bool> relationHasProbFact;
bool detOptEnabled = true;
std::unordered_map<std::string, bool> relationIsDet;

std::map<std::string, std::set<UntypedTuple>> initialInputRelations;
