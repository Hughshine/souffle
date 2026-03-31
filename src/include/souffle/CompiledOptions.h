/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file CompiledOptions.h
 *
 * A header file offering command-line option support for compiled
 * RAM programs.
 *
 ***********************************************************************/

#pragma once

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <vector>

#ifdef USE_CUSTOM_GETOPTLONG
#include "souffle/utility/GetOptLongImpl.h"
#else
#include <getopt.h>
#endif

namespace souffle {

enum class SemMode {
    INC,
    FULL
};

enum class FcMode {
    FULL_HARD,
    FULL_SOFT,
    INC_NAIVE,
    INC_REGIONAL,
    ELASTIC
};

enum class WmcMode {
    FULL,
    INC_NAIVE,
    INC_REGIONAL
};

enum class FullEvaluator {
    EXACT,
    SCBF,
    APPROX
};

enum class RewriteEngine {
    OFF,
    LEGACY,
    IMPLICIT,
    IMPLICIT_ITER
};

enum class RewriteSplitMode {
    OFF,
    NAIVE,
    COMPLETE
};

enum class RewriteDetectMode {
    DIRTY_FRONTIER,
    COMPLETE
};

enum class DetMode {
    AUTO,
    OFF,
    FORCE
};

enum class ApproxBackend {
    NONE,
    AMC
};

struct IncrementalModeSpec {
    SemMode sem = SemMode::INC;
    FcMode fc = FcMode::INC_NAIVE;
};

inline FcMode effectiveFcMode(FcMode fc) {
    return fc == FcMode::ELASTIC ? FcMode::INC_NAIVE : fc;
}

inline bool isIncrementalSemMode(SemMode sem) {
    return sem == SemMode::INC;
}

inline bool isFullSemMode(SemMode sem) {
    return sem == SemMode::FULL;
}

inline bool isIncrementalFcMode(FcMode fc) {
    fc = effectiveFcMode(fc);
    return fc == FcMode::INC_NAIVE || fc == FcMode::INC_REGIONAL;
}

inline bool isFullFcMode(FcMode fc) {
    fc = effectiveFcMode(fc);
    return fc == FcMode::FULL_HARD || fc == FcMode::FULL_SOFT;
}

inline bool isRegionalFcMode(FcMode fc) {
    return effectiveFcMode(fc) == FcMode::INC_REGIONAL;
}

inline bool isElasticFcMode(FcMode fc) {
    return fc == FcMode::ELASTIC;
}

inline bool modeUsesIncrementalState(const IncrementalModeSpec& mode) {
    return isIncrementalSemMode(mode.sem) || isIncrementalFcMode(mode.fc);
}

inline WmcMode getWmcMode(FcMode fc) {
    switch (effectiveFcMode(fc)) {
        case FcMode::FULL_HARD:
        case FcMode::FULL_SOFT:
            return WmcMode::FULL;
        case FcMode::INC_NAIVE:
            return WmcMode::INC_NAIVE;
        case FcMode::INC_REGIONAL:
            return WmcMode::INC_REGIONAL;
        case FcMode::ELASTIC:
            break;
    }
    return WmcMode::FULL;
}

inline const char* semModeLabel(SemMode sem) {
    return sem == SemMode::FULL ? "SEM-FULL" : "SEM-INC";
}

inline const char* semModeTokenLabel(SemMode sem) {
    return sem == SemMode::FULL ? "full" : "inc";
}

inline const char* fcModeLabel(FcMode fc) {
    switch (fc) {
        case FcMode::FULL_HARD:
            return "FULL-HARD";
        case FcMode::FULL_SOFT:
            return "FULL-SOFT";
        case FcMode::INC_NAIVE:
            return "INC-NAIVE";
        case FcMode::INC_REGIONAL:
            return "INC-REGIONAL";
        case FcMode::ELASTIC:
            return "ELASTIC";
    }
    return "UNKNOWN";
}

inline const char* fcModeTokenLabel(FcMode fc) {
    switch (fc) {
        case FcMode::FULL_HARD:
            return "full-hard";
        case FcMode::FULL_SOFT:
            return "full-soft";
        case FcMode::INC_NAIVE:
            return "inc-naive";
        case FcMode::INC_REGIONAL:
            return "inc-regional";
        case FcMode::ELASTIC:
            return "elastic";
    }
    return "inc-naive";
}

inline const char* wmcModeLabel(WmcMode wmcMode) {
    switch (wmcMode) {
        case WmcMode::FULL:
            return "WMC-FULL";
        case WmcMode::INC_NAIVE:
            return "WMC-INC-NAIVE";
        case WmcMode::INC_REGIONAL:
            return "WMC-INC-REGIONAL";
    }
    return "WMC-UNKNOWN";
}

inline const char* incrementalModeOptionSyntax() {
    return "[ full | full-hard | full-soft | inc-naive | inc-regional | elastic ]";
}

inline const char* incrementalLegacyModeHelpText() {
    return "inc-naive (inc/incr), inc-regional, full (full-hard), full-soft, elastic (fallback=inc-naive)";
}

inline const char* incrementalSetModeUsageText() {
    return "setmode <legacy-mode> OR setmode sem=<inc|full> fc=<full-hard|full-soft|inc-naive|inc-regional>";
}

inline const char* semModeOptionSyntax() {
    return "[ full | inc ]";
}

inline const char* fcModeOptionSyntax() {
    return "[ full-hard | full-soft | inc-naive | inc-regional | elastic ]";
}

inline const char* fullEvaluatorOptionSyntax() {
    return "[ exact | scbf | approx ]";
}

inline const char* ddBackendOptionSyntax() {
    return "[ bdd | sdd ]";
}

inline const char* approxBackendOptionSyntax() {
    return "[ none | amc ]";
}

inline const char* rewriteEngineOptionSyntax() {
    return "[ off | legacy | implicit | implicit-iter ]";
}

inline const char* rewriteSplitOptionSyntax() {
    return "[ off | naive | complete ]";
}

inline const char* rewriteDetectOptionSyntax() {
    return "[ dirty-frontier | complete ]";
}

inline const char* detModeOptionSyntax() {
    return "[ auto | off | force ]";
}

inline const char* dumpKindsOptionSyntax() {
    return "[ json | json-before-graph | json-before-prune | dot | stat | const ]";
}

inline const char* profileStageOptionSyntax() {
    return "[ dred | inc | fc | wmc | inc-delete | inc-regional | inc-regional-heavy | dep-graph ]";
}

inline std::string trimModeSpecToken(const std::string& value);

inline std::string joinOutputPath(const std::string& dir, const std::string& filename) {
    if (dir.empty()) {
        return filename;
    }
    if (dir.back() == '/') {
        return dir + filename;
    }
    return dir + "/" + filename;
}

inline std::string normalizeFlagToken(const std::string& raw) {
    const auto trimmed = trimModeSpecToken(raw);
    std::string out;
    out.reserve(trimmed.size());
    for (unsigned char c : trimmed) {
        out.push_back(static_cast<char>(std::tolower(c)));
    }
    return out;
}

inline const char* fcProbabilityOutputSuffix(FcMode fc) {
    switch (effectiveFcMode(fc)) {
        case FcMode::FULL_HARD:
            return "-inc-full-hard";
        case FcMode::FULL_SOFT:
            return "-inc-full-soft";
        case FcMode::INC_NAIVE:
            return "-inc-naive";
        case FcMode::INC_REGIONAL:
            return "-inc-regional";
        case FcMode::ELASTIC:
            break;
    }
    return "-unknown";
}

inline const char* incrementalFcProfileModeLabel(FcMode fc) {
    switch (effectiveFcMode(fc)) {
        case FcMode::FULL_HARD:
            return "inc-full-hard";
        case FcMode::FULL_SOFT:
            return "inc-full-soft";
        case FcMode::INC_NAIVE:
            return "inc-naive";
        case FcMode::INC_REGIONAL:
            return "inc-regional";
        case FcMode::ELASTIC:
            break;
    }
    return "unknown";
}

inline const char* fullSemProfileModeLabel(FcMode fc) {
    switch (effectiveFcMode(fc)) {
        case FcMode::INC_NAIVE:
            return "cli-full-inc-naive";
        case FcMode::INC_REGIONAL:
            return "cli-full-inc-regional";
        case FcMode::FULL_HARD:
        case FcMode::FULL_SOFT:
            return "cli-full";
        case FcMode::ELASTIC:
            break;
    }
    return "cli-full-unknown";
}

inline std::string modeSummaryLabel(const IncrementalModeSpec& mode) {
    std::ostringstream oss;
    oss << semModeLabel(mode.sem) << "+";
    if (isElasticFcMode(mode.fc)) {
        oss << fcModeLabel(mode.fc) << "->" << fcModeLabel(effectiveFcMode(mode.fc));
    } else {
        oss << fcModeLabel(mode.fc);
    }
    oss << "+" << wmcModeLabel(getWmcMode(mode.fc));
    return oss.str();
}

inline const char* debuggerTurnModeLabel(const IncrementalModeSpec& mode) {
    if (mode.sem == SemMode::FULL) {
        if (mode.fc == FcMode::FULL_SOFT) {
            return "FULL-SOFT";
        }
        if (mode.fc == FcMode::FULL_HARD) {
            return "FULL-HARD";
        }
        return "FULL";
    }
    return "INC";
}

inline std::string onlineTurnProbabilityOutputSuffix(const IncrementalModeSpec& mode) {
    if (mode.sem == SemMode::FULL && isFullFcMode(mode.fc)) {
        return "-full";
    }
    return fcProbabilityOutputSuffix(mode.fc);
}

inline std::string makeOnlineTurnProbabilityPrefix(
        std::size_t iteration, const IncrementalModeSpec& mode) {
    return "fact-iter" + std::to_string(iteration) + onlineTurnProbabilityOutputSuffix(mode);
}

inline FcMode reconcileFcModeForSem(SemMode sem, FcMode currentFc) {
    if (sem == SemMode::FULL) {
        return isFullFcMode(currentFc) ? currentFc : FcMode::FULL_HARD;
    }
    return isIncrementalFcMode(currentFc) ? currentFc : FcMode::INC_NAIVE;
}

inline bool parseLegacyModeToken(const std::string& token, IncrementalModeSpec& mode,
        std::string* canonicalToken = nullptr) {
    const std::string value = normalizeFlagToken(token);
    if (value == "inc" || value == "incremental" || value == "incr" || value == "inc-naive") {
        mode.sem = SemMode::INC;
        mode.fc = FcMode::INC_NAIVE;
        if (canonicalToken) {
            *canonicalToken = "inc-naive";
        }
        return true;
    }
    if (value == "inc-regional" || value == "regional") {
        mode.sem = SemMode::INC;
        mode.fc = FcMode::INC_REGIONAL;
        if (canonicalToken) {
            *canonicalToken = "inc-regional";
        }
        return true;
    }
    if (value == "full" || value == "full-hard") {
        mode.sem = SemMode::FULL;
        mode.fc = FcMode::FULL_HARD;
        if (canonicalToken) {
            *canonicalToken = "full-hard";
        }
        return true;
    }
    if (value == "full-soft") {
        mode.sem = SemMode::FULL;
        mode.fc = FcMode::FULL_SOFT;
        if (canonicalToken) {
            *canonicalToken = "full-soft";
        }
        return true;
    }
    if (value == "elastic") {
        mode.sem = SemMode::INC;
        mode.fc = FcMode::ELASTIC;
        if (canonicalToken) {
            *canonicalToken = "elastic";
        }
        return true;
    }
    return false;
}

inline bool parseSemModeToken(const std::string& token, SemMode& sem) {
    const std::string value = normalizeFlagToken(token);
    if (value == "inc" || value == "incremental" || value == "incr" || value == "inc-naive" ||
            value == "inc-regional" || value == "regional") {
        sem = SemMode::INC;
        return true;
    }
    if (value == "full" || value == "full-hard" || value == "full-soft") {
        sem = SemMode::FULL;
        return true;
    }
    return false;
}

inline bool parseFcModeToken(const std::string& token, FcMode& fc) {
    const std::string value = normalizeFlagToken(token);
    if (value == "full" || value == "full-hard") {
        fc = FcMode::FULL_HARD;
        return true;
    }
    if (value == "full-soft") {
        fc = FcMode::FULL_SOFT;
        return true;
    }
    if (value == "inc" || value == "incremental" || value == "incr" || value == "inc-naive") {
        fc = FcMode::INC_NAIVE;
        return true;
    }
    if (value == "inc-regional" || value == "regional") {
        fc = FcMode::INC_REGIONAL;
        return true;
    }
    if (value == "elastic") {
        fc = FcMode::ELASTIC;
        return true;
    }
    return false;
}

inline bool parseFullEvaluatorToken(const std::string& token, FullEvaluator& evaluator,
        std::string* canonicalToken = nullptr) {
    const std::string value = normalizeFlagToken(token);
    if (value == "exact") {
        evaluator = FullEvaluator::EXACT;
        if (canonicalToken) {
            *canonicalToken = "exact";
        }
        return true;
    }
    if (value == "scbf") {
        evaluator = FullEvaluator::SCBF;
        if (canonicalToken) {
            *canonicalToken = "scbf";
        }
        return true;
    }
    if (value == "approx") {
        evaluator = FullEvaluator::APPROX;
        if (canonicalToken) {
            *canonicalToken = "approx";
        }
        return true;
    }
    return false;
}

inline bool parseRewriteEngineToken(const std::string& token, RewriteEngine& engine,
        std::string* canonicalToken = nullptr) {
    const std::string value = normalizeFlagToken(token);
    if (value == "off" || value == "none") {
        engine = RewriteEngine::OFF;
        if (canonicalToken) {
            *canonicalToken = "off";
        }
        return true;
    }
    if (value == "legacy" || value == "rewrite") {
        engine = RewriteEngine::LEGACY;
        if (canonicalToken) {
            *canonicalToken = "legacy";
        }
        return true;
    }
    if (value == "implicit") {
        engine = RewriteEngine::IMPLICIT;
        if (canonicalToken) {
            *canonicalToken = "implicit";
        }
        return true;
    }
    if (value == "implicit-iter" || value == "implicit-iterate" || value == "implicit-iterative") {
        engine = RewriteEngine::IMPLICIT_ITER;
        if (canonicalToken) {
            *canonicalToken = "implicit-iter";
        }
        return true;
    }
    return false;
}

inline bool parseRewriteSplitModeToken(const std::string& token, RewriteSplitMode& mode,
        std::string* canonicalToken = nullptr) {
    const std::string value = normalizeFlagToken(token);
    if (value == "off" || value == "none" || value == "no-split") {
        mode = RewriteSplitMode::OFF;
        if (canonicalToken) {
            *canonicalToken = "off";
        }
        return true;
    }
    if (value == "naive" || value == "naive-split") {
        mode = RewriteSplitMode::NAIVE;
        if (canonicalToken) {
            *canonicalToken = "naive";
        }
        return true;
    }
    if (value == "complete" || value == "complete-split") {
        mode = RewriteSplitMode::COMPLETE;
        if (canonicalToken) {
            *canonicalToken = "complete";
        }
        return true;
    }
    return false;
}

inline bool parseRewriteDetectModeToken(const std::string& token, RewriteDetectMode& mode,
        std::string* canonicalToken = nullptr) {
    const std::string value = normalizeFlagToken(token);
    if (value == "dirty-frontier" || value == "dirty" || value == "default") {
        mode = RewriteDetectMode::DIRTY_FRONTIER;
        if (canonicalToken) {
            *canonicalToken = "dirty-frontier";
        }
        return true;
    }
    if (value == "complete" || value == "full" || value == "force-complete") {
        mode = RewriteDetectMode::COMPLETE;
        if (canonicalToken) {
            *canonicalToken = "complete";
        }
        return true;
    }
    return false;
}

inline bool parseDetModeToken(const std::string& token, DetMode& mode,
        std::string* canonicalToken = nullptr) {
    const std::string value = normalizeFlagToken(token);
    if (value == "auto" || value == "det-opt" || value == "on") {
        mode = DetMode::AUTO;
        if (canonicalToken) {
            *canonicalToken = "auto";
        }
        return true;
    }
    if (value == "off" || value == "no-det-opt" || value == "disable") {
        mode = DetMode::OFF;
        if (canonicalToken) {
            *canonicalToken = "off";
        }
        return true;
    }
    if (value == "force" || value == "det-force") {
        mode = DetMode::FORCE;
        if (canonicalToken) {
            *canonicalToken = "force";
        }
        return true;
    }
    return false;
}

inline bool parseApproxBackendToken(const std::string& token, ApproxBackend& backend,
        std::string* canonicalToken = nullptr) {
    const std::string value = normalizeFlagToken(token);
    if (value == "none") {
        backend = ApproxBackend::NONE;
        if (canonicalToken) {
            *canonicalToken = "none";
        }
        return true;
    }
    if (value == "amc") {
        backend = ApproxBackend::AMC;
        if (canonicalToken) {
            *canonicalToken = "amc";
        }
        return true;
    }
    return false;
}

inline bool parseDdBackendToken(const std::string& token, std::string& backend,
        std::string* canonicalToken = nullptr) {
    const std::string value = normalizeFlagToken(token);
    if (value == "bdd" || value == "sdd") {
        backend = value;
        if (canonicalToken) {
            *canonicalToken = value;
        }
        return true;
    }
    return false;
}

inline const char* fullEvaluatorLabel(FullEvaluator evaluator) {
    switch (evaluator) {
        case FullEvaluator::EXACT:
            return "exact";
        case FullEvaluator::SCBF:
            return "scbf";
        case FullEvaluator::APPROX:
            return "approx";
    }
    return "exact";
}

inline const char* rewriteEngineLabel(RewriteEngine engine) {
    switch (engine) {
        case RewriteEngine::OFF:
            return "off";
        case RewriteEngine::LEGACY:
            return "legacy";
        case RewriteEngine::IMPLICIT:
            return "implicit";
        case RewriteEngine::IMPLICIT_ITER:
            return "implicit-iter";
    }
    return "off";
}

inline const char* rewriteSplitModeLabel(RewriteSplitMode mode) {
    switch (mode) {
        case RewriteSplitMode::OFF:
            return "off";
        case RewriteSplitMode::NAIVE:
            return "naive";
        case RewriteSplitMode::COMPLETE:
            return "complete";
    }
    return "off";
}

inline const char* rewriteDetectModeLabel(RewriteDetectMode mode) {
    switch (mode) {
        case RewriteDetectMode::DIRTY_FRONTIER:
            return "dirty-frontier";
        case RewriteDetectMode::COMPLETE:
            return "complete";
    }
    return "dirty-frontier";
}

inline const char* detModeLabel(DetMode mode) {
    switch (mode) {
        case DetMode::AUTO:
            return "auto";
        case DetMode::OFF:
            return "off";
        case DetMode::FORCE:
            return "force";
    }
    return "auto";
}

inline const char* approxBackendLabel(ApproxBackend backend) {
    switch (backend) {
        case ApproxBackend::NONE:
            return "none";
        case ApproxBackend::AMC:
            return "amc";
    }
    return "none";
}

inline bool parseDumpKindToken(const std::string& token, std::string& kind,
        std::string* canonicalToken = nullptr) {
    const std::string value = normalizeFlagToken(token);
    if (value == "json" || value == "json-before-graph" || value == "json-before-prune" ||
            value == "dot" || value == "stat" || value == "const") {
        kind = value;
        if (canonicalToken) {
            *canonicalToken = value;
        }
        return true;
    }
    return false;
}

inline bool parseProfileStageToken(const std::string& token, std::string& stage,
        std::string* canonicalToken = nullptr) {
    const std::string value = normalizeFlagToken(token);
    if (value == "dred" || value == "inc" || value == "fc" || value == "wmc" ||
            value == "inc-delete" || value == "inc-regional" ||
            value == "inc-regional-heavy" || value == "dep-graph") {
        stage = value;
        if (canonicalToken) {
            *canonicalToken = value;
        }
        return true;
    }
    return false;
}

inline std::string trimModeSpecToken(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return std::string();
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

inline void appendSplitModeSpecs(std::vector<std::string>& specs, const std::string& token) {
    std::size_t begin = 0;
    while (begin <= token.size()) {
        const std::size_t comma = token.find(',', begin);
        const std::size_t end = (comma == std::string::npos) ? token.size() : comma;
        const std::string spec = trimModeSpecToken(token.substr(begin, end - begin));
        if (!spec.empty()) {
            specs.push_back(spec);
        }
        if (comma == std::string::npos) {
            break;
        }
        begin = comma + 1;
    }
}

inline bool parseIncrementalModeSpecs(const std::vector<std::string>& specs,
        const IncrementalModeSpec& current, IncrementalModeSpec& next, std::string* error = nullptr,
        std::string* canonicalLegacyToken = nullptr) {
    if (specs.empty()) {
        if (error) {
            *error = "empty mode spec";
        }
        return false;
    }

    next = current;
    if (specs.size() == 1 && specs[0].find('=') == std::string::npos) {
        std::string canonical;
        if (!parseLegacyModeToken(specs[0], next, &canonical)) {
            if (error) {
                *error = "Unknown mode: " + specs[0];
            }
            return false;
        }
        if (canonicalLegacyToken) {
            *canonicalLegacyToken = canonical;
        }
        return true;
    }

    bool hasSem = false;
    bool hasFc = false;
    for (const auto& specRaw : specs) {
        const std::string spec = trimModeSpecToken(specRaw);
        const auto eq = spec.find('=');
        if (eq == std::string::npos) {
            if (error) {
                *error = "Invalid setmode item: " + spec + " (expected key=value)";
            }
            return false;
        }
        const std::string key = trimModeSpecToken(spec.substr(0, eq));
        const std::string value = trimModeSpecToken(spec.substr(eq + 1));
        if (key == "sem") {
            SemMode semTmp = next.sem;
            if (!parseSemModeToken(value, semTmp)) {
                if (error) {
                    *error = "Unknown sem mode: " + value;
                }
                return false;
            }
            next.sem = semTmp;
            hasSem = true;
        } else if (key == "fc") {
            FcMode fcTmp = next.fc;
            if (!parseFcModeToken(value, fcTmp)) {
                if (error) {
                    *error = "Unknown fc mode: " + value;
                }
                return false;
            }
            next.fc = fcTmp;
            hasFc = true;
        } else if (key == "wmc") {
            if (value != "follow" && value != "fc" && value != "auto") {
                if (error) {
                    *error = "Unsupported wmc mode: " + value +
                            " (wmc follows fc in current design)";
                }
                return false;
            }
        } else {
            if (error) {
                *error = "Unknown setmode key: " + key;
            }
            return false;
        }
    }

    if (hasSem && !hasFc) {
        next.fc = reconcileFcModeForSem(next.sem, next.fc);
    }
    return true;
}

/**
 * A utility class for parsing command line arguments within generated
 * query programs.
 */
class CmdOptions {
protected:
    /**
     * source file
     */
    std::string src;

    /**
     * fact directory
     */
    std::string input_dir;

    /**
     * output directory
     */
    std::string output_dir;

    /**
     * profiling flag
     */
    bool profiling;

    /**
     * profile filename
     */
    std::string profile_name;

    /**
     * number of threads
     */
    std::size_t num_jobs;
    std::string log_file_name = "log.txt";  // default log file name
    bool derivation_only = false; // default derivation graph generation flag
    /**
    * knowledge representation
    */
    std::string knowledge_representation = "bdd";  // bdd, sdd are supported
    bool merge_bi_imp = true;  // enable merging mutually implying deterministic nodes
    bool prune_extra = false;  // enable extra prune pass (outputless components)
    bool fold_const = false;  // enable deterministic constant pre-analysis (no prune rewrite)
    bool enable_rewrite = false;  // enable SISO-based graph rewriting
    bool enable_implicit_rewrite = false;  // enable experimental implicit-split rewrite pipeline
    bool enable_implicit_iterate_split_rewrite = false;  // enable iterative split<->rewrite fixpoint for implicit rewrite
    std::string split_mode = "naive-split";  // split mode for rewrite: no-split/naive-split/complete-split
    bool dump_json = false;  // dump derivation graph JSON after prune
    bool dump_json_before_graph = false;  // dump ruleapp-reconstructed JSON before graph materialization
    bool dump_json_before_prune = false;  // dump derivation graph JSON before prune
    bool dump_dot = false;  // dump derivation graph DOT after prune
    bool dump_stat = false;  // dump derivation graph stats after prune
    bool dump_const = false;  // dump constant pre-analysis details
    bool dred_profile = false;  // enable detailed DRed profiling
    bool inc_profile = false;  // enable incremental stage profiling
    bool fc_profile = false;  // enable detailed forward-compilation profiling
    bool inc_delete_profile = false;  // enable delete-phase profiling for incremental FC
    bool wmc_profile = false;  // enable weighted model counting profiling
    bool inc_regional_profile = false;  // enable inc-regional profiling/diagnostics
    bool inc_regional_profile_heavy = false;  // enable heavy inc-regional profiling
    std::string inc_regional_trace_tuples;  // comma-separated tuples for inc-regional trace
    bool dep_graph_profile = false;  // enable dependency-graph profiling
    bool post_del = false;  // enable postprocessUselessVariables after deletion
    bool enable_inc_reord = false;  // enable CUDD dynamic reordering in incremental turns
    bool det_opt = true;  // enable deterministic-relation analysis and det gating (default on)
    bool det_force = false;  // force deterministic evaluation (skip derivation graph)
    bool reuse_var_index = true;  // reuse freed variable indices in CUDD (default on)
    bool single_rand_fast = true;  // enable single-randvar fast path in component FC
    bool force_complete_siso_detect = false;  // force full-graph SISO detection (disable dirty-frontier detect)
    bool enable_scbf = false;  // enable experimental SCBF full pipeline
    FullEvaluator full_evaluator = FullEvaluator::EXACT;
    RewriteEngine rewrite_engine = RewriteEngine::OFF;
    RewriteSplitMode rewrite_split_mode = RewriteSplitMode::NAIVE;
    RewriteDetectMode rewrite_detect_mode = RewriteDetectMode::DIRTY_FRONTIER;
    DetMode det_mode = DetMode::AUTO;
    ApproxBackend approx_backend = ApproxBackend::NONE;
public:
    // all argument constructor
    CmdOptions(const char* s, const char* id, const char* od, bool pe, const char* pfn, std::size_t nj,
            std::string lfn = "log.txt", bool donly = false, const std::string& mode = "inc",
            bool merge_bi = true, bool foldconst = false, bool rewrite = false,
            bool dumpjson = false, bool dumpjsonBeforeGraph = false, bool dumpjsonBeforePrune = false,
            bool dumpdot = false, bool dumpstat = false, bool dumpconst = false,
            bool dredProfile = false,
            const std::string& splitmode = "naive-split",
            bool incProfile = false,
            bool fcProfile = false,
            bool incDeleteProfile = false,
            bool wmcProfile = false,
            bool incRegionalProfile = false,
            bool incRegionalProfileHeavy = false,
            std::string incRegionalTraceTuples = "",
            bool depGraphProfile = false,
            bool postDel = false)
            : src(s), input_dir(id), output_dir(od), profiling(pe), profile_name(pfn), num_jobs(nj), log_file_name(lfn), derivation_only(donly)
    , merge_bi_imp(merge_bi), fold_const(foldconst), enable_rewrite(rewrite),
      dump_json(dumpjson), dump_json_before_graph(dumpjsonBeforeGraph),
      dump_json_before_prune(dumpjsonBeforePrune),
      dump_dot(dumpdot), dump_stat(dumpstat), dump_const(dumpconst),
      dred_profile(dredProfile), inc_profile(incProfile), fc_profile(fcProfile),
      inc_delete_profile(incDeleteProfile),
      wmc_profile(wmcProfile),
      inc_regional_profile(incRegionalProfile), inc_regional_profile_heavy(incRegionalProfileHeavy),
      inc_regional_trace_tuples(std::move(incRegionalTraceTuples)),
      dep_graph_profile(depGraphProfile), post_del(postDel),
      split_mode(splitmode) {
        syncCanonicalStateFromLegacyFields();
        setIncrementalMode(mode);
    }

    CmdOptions() {}
    /**
     * get source code name
     */
    const std::string& getSourceFileName() const {
        return src;
    }

    /**
     * get input directory
     */
    const std::string& getInputFileDir() const {
        return input_dir;
    }

    /**
     * get output directory
     */
    const std::string& getOutputFileDir() const {
        return output_dir;
    }
    const std::string& getLogFileName() const {
        return log_file_name;
    }

    std::string incMode = "inc-naive";
    const std::string& getIncMode() const {
        return incMode;
    }
    const IncrementalModeSpec& getIncrementalModeSpec() const {
        return incModeSpec;
    }
    bool setIncrementalMode(const std::string& token) {
        IncrementalModeSpec parsed = incModeSpec;
        std::string canonical;
        if (!parseLegacyModeToken(token, parsed, &canonical)) {
            return false;
        }
        setIncrementalModeSpec(parsed, canonical);
        return true;
    }
    void setIncrementalModeSpec(const IncrementalModeSpec& mode,
            const std::string& canonicalLegacyToken = "") {
        incModeSpec = mode;
        if (!canonicalLegacyToken.empty()) {
            incMode = canonicalLegacyToken;
            return;
        }
        if (mode.sem == SemMode::FULL && mode.fc == FcMode::FULL_SOFT) {
            incMode = "full-soft";
        } else if (mode.sem == SemMode::FULL && mode.fc == FcMode::FULL_HARD) {
            incMode = "full-hard";
        } else if (mode.sem == SemMode::INC && mode.fc == FcMode::INC_REGIONAL) {
            incMode = "inc-regional";
        } else if (mode.sem == SemMode::INC && mode.fc == FcMode::ELASTIC) {
            incMode = "elastic";
        } else {
            incMode = "inc-naive";
        }
    }
    bool setSemModeToken(const std::string& token) {
        SemMode sem = incModeSpec.sem;
        if (!parseSemModeToken(token, sem)) {
            return false;
        }
        setIncrementalModeSpec(
                IncrementalModeSpec{sem, reconcileFcModeForSem(sem, incModeSpec.fc)});
        return true;
    }
    void setSemMode(SemMode sem) {
        setIncrementalModeSpec(IncrementalModeSpec{sem, reconcileFcModeForSem(sem, incModeSpec.fc)});
    }
    bool setFcModeToken(const std::string& token) {
        FcMode fc = incModeSpec.fc;
        if (!parseFcModeToken(token, fc)) {
            return false;
        }
        setIncrementalModeSpec(IncrementalModeSpec{incModeSpec.sem, fc});
        return true;
    }
    void setFcMode(FcMode fc) {
        setIncrementalModeSpec(IncrementalModeSpec{incModeSpec.sem, fc});
    }
    void setInputFileDir(std::string dir) {
        input_dir = std::move(dir);
    }
    void setOutputFileDir(std::string dir) {
        output_dir = std::move(dir);
    }
    void setLogFileName(std::string name) {
        log_file_name = std::move(name);
    }
    void setDerivationOnly(bool enabled) {
        derivation_only = enabled;
    }
    void setMergeBiImpEnabled(bool enabled) {
        merge_bi_imp = enabled;
    }
    void setPruneExtraEnabled(bool enabled) {
        prune_extra = enabled;
    }
    void setConstFoldEnabled(bool enabled) {
        fold_const = enabled;
    }
    bool setKnowledgeRepresentationToken(const std::string& token) {
        std::string backend = knowledge_representation;
        if (!parseDdBackendToken(token, backend)) {
            return false;
        }
        knowledge_representation = backend;
        return true;
    }
    void setKnowledgeRepresentation(std::string backend) {
        knowledge_representation = std::move(backend);
    }
    void setFullEvaluator(FullEvaluator evaluator) {
        full_evaluator = evaluator;
        syncLegacyFieldsFromCanonicalState();
    }
    bool setFullEvaluatorToken(const std::string& token) {
        FullEvaluator evaluator = full_evaluator;
        if (!parseFullEvaluatorToken(token, evaluator)) {
            return false;
        }
        setFullEvaluator(evaluator);
        return true;
    }
    void setRewriteEngine(RewriteEngine engine) {
        rewrite_engine = engine;
        syncLegacyFieldsFromCanonicalState();
    }
    bool setRewriteEngineToken(const std::string& token) {
        RewriteEngine engine = rewrite_engine;
        if (!parseRewriteEngineToken(token, engine)) {
            return false;
        }
        setRewriteEngine(engine);
        return true;
    }
    void setRewriteSplitMode(RewriteSplitMode mode) {
        rewrite_split_mode = mode;
        syncLegacyFieldsFromCanonicalState();
    }
    bool setRewriteSplitModeToken(const std::string& token) {
        RewriteSplitMode mode = rewrite_split_mode;
        if (!parseRewriteSplitModeToken(token, mode)) {
            return false;
        }
        setRewriteSplitMode(mode);
        return true;
    }
    void setRewriteDetectMode(RewriteDetectMode mode) {
        rewrite_detect_mode = mode;
        syncLegacyFieldsFromCanonicalState();
    }
    bool setRewriteDetectModeToken(const std::string& token) {
        RewriteDetectMode mode = rewrite_detect_mode;
        if (!parseRewriteDetectModeToken(token, mode)) {
            return false;
        }
        setRewriteDetectMode(mode);
        return true;
    }
    void setDetMode(DetMode mode) {
        det_mode = mode;
        syncLegacyFieldsFromCanonicalState();
    }
    bool setDetModeToken(const std::string& token) {
        DetMode mode = det_mode;
        if (!parseDetModeToken(token, mode)) {
            return false;
        }
        setDetMode(mode);
        return true;
    }
    void setApproxBackend(ApproxBackend backend) {
        approx_backend = backend;
    }
    bool setApproxBackendToken(const std::string& token) {
        ApproxBackend backend = approx_backend;
        if (!parseApproxBackendToken(token, backend)) {
            return false;
        }
        setApproxBackend(backend);
        return true;
    }
    FullEvaluator getFullEvaluator() const {
        return full_evaluator;
    }
    RewriteEngine getRewriteEngine() const {
        return rewrite_engine;
    }
    RewriteSplitMode getRewriteSplitMode() const {
        return rewrite_split_mode;
    }
    RewriteDetectMode getRewriteDetectMode() const {
        return rewrite_detect_mode;
    }
    DetMode getDetMode() const {
        return det_mode;
    }
    ApproxBackend getApproxBackend() const {
        return approx_backend;
    }
    /**
     * is profiling switched on
     */
    bool isProfiling() const {
        return profiling;
    }

    bool isDerivationOnly() const {
        return derivation_only;
    }

    bool isMergeBiImpEnabled() const {
        return merge_bi_imp;
    }
    bool isPruneExtraEnabled() const {
        return prune_extra;
    }
    bool isConstFoldEnabled() const {
        return fold_const;
    }
    bool isRewriteEnabled() const {
        return enable_rewrite;
    }
    bool isImplicitRewriteEnabled() const {
        return enable_implicit_rewrite;
    }
    bool isImplicitIterateSplitRewriteEnabled() const {
        return enable_implicit_iterate_split_rewrite;
    }
    const std::string& getSplitMode() const {
        return split_mode;
    }
    bool isDumpJsonEnabled() const {
        return dump_json;
    }
    void setDumpJsonEnabled(bool enabled) {
        dump_json = enabled;
    }
    bool isDumpJsonBeforeGraphEnabled() const {
        return dump_json_before_graph;
    }
    void setDumpJsonBeforeGraphEnabled(bool enabled) {
        dump_json_before_graph = enabled;
    }
    bool isDumpJsonBeforePruneEnabled() const {
        return dump_json_before_prune;
    }
    void setDumpJsonBeforePruneEnabled(bool enabled) {
        dump_json_before_prune = enabled;
    }
    bool isDumpDotEnabled() const {
        return dump_dot;
    }
    void setDumpDotEnabled(bool enabled) {
        dump_dot = enabled;
    }
    bool isDumpStatEnabled() const {
        return dump_stat;
    }
    void setDumpStatEnabled(bool enabled) {
        dump_stat = enabled;
    }
    bool isDumpConstEnabled() const {
        return dump_const;
    }
    void setDumpConstEnabled(bool enabled) {
        dump_const = enabled;
    }
    bool setDumpKindToken(const std::string& token, bool enabled) {
        std::string kind;
        if (!parseDumpKindToken(token, kind)) {
            return false;
        }
        if (kind == "json") {
            dump_json = enabled;
        } else if (kind == "json-before-graph") {
            dump_json_before_graph = enabled;
        } else if (kind == "json-before-prune") {
            dump_json_before_prune = enabled;
        } else if (kind == "dot") {
            dump_dot = enabled;
        } else if (kind == "stat") {
            dump_stat = enabled;
        } else if (kind == "const") {
            dump_const = enabled;
        }
        return true;
    }
    bool isDredProfileEnabled() const {
        return dred_profile;
    }
    bool isIncProfileEnabled() const {
        return inc_profile;
    }
    bool isFcProfileEnabled() const {
        return fc_profile;
    }
    bool isIncDeleteProfileEnabled() const {
        return inc_delete_profile;
    }
    bool isWmcProfileEnabled() const {
        return wmc_profile;
    }
    bool isIncRegionalProfileEnabled() const {
        return inc_regional_profile;
    }
    bool isIncRegionalProfileHeavyEnabled() const {
        return inc_regional_profile_heavy;
    }
    const std::string& getIncRegionalTraceTuples() const {
        return inc_regional_trace_tuples;
    }
    void setIncRegionalTraceTuples(std::string tuples) {
        inc_regional_trace_tuples = std::move(tuples);
    }
    bool isDepGraphProfileEnabled() const {
        return dep_graph_profile;
    }
    bool setProfileStageToken(const std::string& token, bool enabled) {
        std::string stage;
        if (!parseProfileStageToken(token, stage)) {
            return false;
        }
        if (stage == "dred") {
            dred_profile = enabled;
        } else if (stage == "inc") {
            inc_profile = enabled;
        } else if (stage == "fc") {
            fc_profile = enabled;
        } else if (stage == "wmc") {
            wmc_profile = enabled;
        } else if (stage == "inc-delete") {
            inc_delete_profile = enabled;
        } else if (stage == "inc-regional") {
            inc_regional_profile = enabled;
        } else if (stage == "inc-regional-heavy") {
            inc_regional_profile = enabled;
            inc_regional_profile_heavy = enabled;
        } else if (stage == "dep-graph") {
            dep_graph_profile = enabled;
        }
        if (!enabled && stage == "inc-regional") {
            inc_regional_profile_heavy = false;
        }
        return true;
    }
    std::vector<std::string> getEnabledDumpKinds() const {
        std::vector<std::string> kinds;
        if (dump_json) kinds.push_back("json");
        if (dump_json_before_graph) kinds.push_back("json-before-graph");
        if (dump_json_before_prune) kinds.push_back("json-before-prune");
        if (dump_dot) kinds.push_back("dot");
        if (dump_stat) kinds.push_back("stat");
        if (dump_const) kinds.push_back("const");
        return kinds;
    }
    std::vector<std::string> getEnabledProfileStages() const {
        std::vector<std::string> stages;
        if (dred_profile) stages.push_back("dred");
        if (inc_profile) stages.push_back("inc");
        if (fc_profile) stages.push_back("fc");
        if (wmc_profile) stages.push_back("wmc");
        if (inc_delete_profile) stages.push_back("inc-delete");
        if (inc_regional_profile) stages.push_back("inc-regional");
        if (inc_regional_profile_heavy) stages.push_back("inc-regional-heavy");
        if (dep_graph_profile) stages.push_back("dep-graph");
        return stages;
    }
    bool isPostDelEnabled() const {
        return post_del;
    }
    bool isIncReorderEnabled() const {
        return enable_inc_reord;
    }
    bool isDetOptEnabled() const {
        return det_opt;
    }
    bool isDetForceEnabled() const {
        return det_force;
    }
    bool isReuseVarIndexEnabled() const {
        return reuse_var_index;
    }
    bool isSingleRandFastEnabled() const {
        return single_rand_fast;
    }
    bool isForceCompleteSisoDetectEnabled() const {
        return force_complete_siso_detect;
    }
    bool isScbfEnabled() const {
        return enable_scbf;
    }

    /**
     * get filename of profile
     */
    const std::string& getProfileName() const {
        return profile_name;
    }

    /**
     * get number of jobs
     */
    std::size_t getNumJobs() const {
        return num_jobs;
    }

    const std::string& getKnowledgeRepresentation() const {
        return knowledge_representation;
    }

    /**
     * Parses the given command line parameters, handles -h help requests or errors
     * and returns whether the parsing was successful or not.
     */
    bool parse(int argc, char** argv) {
        // get executable name
        std::string exec_name = "analysis";
        if (argc > 0) {
            exec_name = argv[0];
        }

        // local options
        std::string fact_dir = input_dir;
        std::string out_dir = output_dir;

        // long options
        option longOptions[] = {{"facts", true, nullptr, 'F'}, {"input-dir", true, nullptr, 'F'},
                {"output", true, nullptr, 'D'}, {"output-dir", true, nullptr, 'D'},
                {"profile", true, nullptr, 'p'}, {"profile-file", true, nullptr, 'p'},
                {"jobs", true, nullptr, 'j'}, {"index", true, nullptr, 'i'},
                {"knowledge", true, nullptr, 'k'}, {"dd-backend", true, nullptr, 1021},
                {"logfile", true, nullptr, 'l'}, {"log-file", true, nullptr, 'l'},
                {"derv-only", optional_argument, nullptr, 'd'},
                {"derivation-only", optional_argument, nullptr, 'd'},
                {"setmode", true, nullptr, 'm'},
                {"sem-mode", true, nullptr, 1022}, {"fc-mode", true, nullptr, 1023},
                {"full-evaluator", true, nullptr, 1024},
                {"approx-backend", true, nullptr, 1025},
                {"merge-bi-imp", false, nullptr, 'e'}, {"no-merge-bi-imp", false, nullptr, 1026},
                {"prune-extra", false, nullptr, 1004}, {"no-prune-extra", false, nullptr, 1027},
                {"fold-const", false, nullptr, 'C'}, {"no-fold-const", false, nullptr, 1028},
                {"rewrite", false, nullptr, 'r'},
                {"rewrite-engine", true, nullptr, 1029},
                {"implicit-rewrite", false, nullptr, 1019},
                {"implicit-iterate-split-rewrite", false, nullptr, 1020},
                {"split-mode", true, nullptr, 'P'}, {"rewrite-split", true, nullptr, 1030},
                {"rewrite-detect", true, nullptr, 1031},
                {"dumpjson", false, nullptr, 'J'},
                {"dumpjson-before-graph", false, nullptr, 1036},
                {"dumpjson-before-prune", false, nullptr, 1035},
                {"dumpdot", false, nullptr, 'T'},
                {"dumpstat", false, nullptr, 'S'},
                {"dumpconst", false, nullptr, 'U'},
                {"dump", true, nullptr, 1032},
                {"dred-profile", false, nullptr, 1002},
                {"inc-profile", false, nullptr, 1003},
                {"fc-profile", false, nullptr, 1005},
                {"profile-inc-delete", false, nullptr, 1013},
                {"profile-wmc", false, nullptr, 1014},
                {"profile-inc-regional", false, nullptr, 1009},
                {"profile-inc-regional-heavy", false, nullptr, 1011},
                {"inc-regional-trace-tuples", true, nullptr, 1012},
                {"trace-inc-regional", true, nullptr, 1012},
                {"profile-stage", true, nullptr, 1033},
                {"profile-dep-graph", false, nullptr, 1010},
                {"post-del", false, nullptr, 1006},
                {"enable-inc-reord", false, nullptr, 1015},
                {"det-opt", false, nullptr, 'Z'},
                {"no-det-opt", false, nullptr, 1017},
                {"det-force", false, nullptr, 1007},
                {"det-mode", true, nullptr, 1034},
                {"no-reuse-var-index", false, nullptr, 1008},
                {"no-single-rand-fast", false, nullptr, 1001},
                {"force-complete-siso-detect", false, nullptr, 1016},
                {"scbf", false, nullptr, 1018},
                // the terminal option -- needs to be null
                {nullptr, false, nullptr, 0}};

        // check whether all options are fine
        bool ok = true;
        bool fcModeExplicit = false;
        int c; /* command-line arguments processing */
        while ((c = getopt_long(argc, argv, "D:F:hp:j:i:d::em:C:rP:JTSUZ", longOptions, nullptr)) != EOF) {
            switch (c) {
                /* Fact directories */
                case 'F':
                    if (!existDir(optarg)) {
                        printf("Fact directory %s does not exists!\n", optarg);
                        ok = false;
                    }
                    fact_dir = optarg;
                    break;
                /* Output directory for resulting .csv files */
                case 'D':
                    if (*optarg && !existDir(optarg) && !dirIsStdout(optarg)) {
                        printf("Output directory %s does not exists!\n", optarg);
                        ok = false;
                    }
                    out_dir = optarg;
                    break;
                case 'p':
                    if (!profiling) {
                        std::cerr << "\nError: profiling was not enabled in compilation\n\n";
                        printHelpPage(exec_name);
                        exit(EXIT_FAILURE);
                    }
                    profile_name = optarg;
                    break;
                case 'j':
#ifdef _OPENMP
                    if (std::string(optarg) == "auto") {
                        num_jobs = 0;
                    } else {
                        int num = atoi(optarg);
                        if (num > 0) {
                            num_jobs = num;
                        } else {
                            std::cerr << "Invalid number of jobs [-j]: " << optarg << "\n";
                            ok = false;
                        }
                    }
#else
                    std::cerr << "\nWarning: OpenMP was not enabled in compilation\n\n";
#endif
                    break;
                case 'k':
                case 1021:
                    if (!setKnowledgeRepresentationToken(optarg)) {
                        std::cerr << "Invalid DD backend [" << (c == 'k' ? "-k|--knowledge" : "--dd-backend")
                                  << "]: " << optarg << "\n";
                        ok = false;
                    }
                    break;
                case 'l':
                    if (*optarg)
                        log_file_name = optarg;
                    else {
                        log_file_name = "log";
                    }
                    break;
                case 'd': {
                    if (optarg == nullptr) {
                        derivation_only = true;
                        break;
                    }
                    const std::string value(optarg);
                    if (value == "true") {
                        derivation_only = true;
                    } else if (value == "false") {
                        derivation_only = false;
                    } else {
                        std::cerr << "Invalid value for derv-only [-d]: " << optarg << "\n";
                        ok = false;
                    }
                    break;
                }
                case 'm': {
                    IncrementalModeSpec parsed = incModeSpec;
                    std::string error;
                    std::string canonical;
                    std::vector<std::string> specs;
                    appendSplitModeSpecs(specs, optarg);
                    if (!parseIncrementalModeSpecs(specs, currentIncrementalModeSpec(), parsed, &error, &canonical)) {
                        std::cerr << "Invalid incremental mode [-m]: " << optarg << "\n";
                        ok = false;
                    } else {
                        setIncrementalModeSpec(parsed, canonical);
                        fcModeExplicit = false;
                    }
                    break;
                }
                case 1022:
                    if (fcModeExplicit) {
                        SemMode sem = incModeSpec.sem;
                        if (!parseSemModeToken(optarg, sem)) {
                            std::cerr << "Invalid semantic mode [--sem-mode]: " << optarg << "\n";
                            ok = false;
                        } else {
                            setIncrementalModeSpec(IncrementalModeSpec{sem, incModeSpec.fc});
                        }
                    } else if (!setSemModeToken(optarg)) {
                        std::cerr << "Invalid semantic mode [--sem-mode]: " << optarg << "\n";
                        ok = false;
                    }
                    break;
                case 1023:
                    if (!setFcModeToken(optarg)) {
                        std::cerr << "Invalid forward-compilation mode [--fc-mode]: " << optarg << "\n";
                        ok = false;
                    } else {
                        fcModeExplicit = true;
                    }
                    break;
                case 1024:
                    if (!setFullEvaluatorToken(optarg)) {
                        std::cerr << "Invalid full evaluator [--full-evaluator]: " << optarg << "\n";
                        ok = false;
                    }
                    break;
                case 1025:
                    if (!setApproxBackendToken(optarg)) {
                        std::cerr << "Invalid approx backend [--approx-backend]: " << optarg << "\n";
                        ok = false;
                    }
                    break;
                case 'e':
                    setMergeBiImpEnabled(true);
                    break;
                case 1026:
                    setMergeBiImpEnabled(false);
                    break;
                case 1004:
                    setPruneExtraEnabled(true);
                    break;
                case 1027:
                    setPruneExtraEnabled(false);
                    break;
                case 'C':
                    setConstFoldEnabled(true);
                    break;
                case 1028:
                    setConstFoldEnabled(false);
                    break;
                case 'r':
                    setRewriteEngine(RewriteEngine::LEGACY);
                    break;
                case 1029:
                    if (!setRewriteEngineToken(optarg)) {
                        std::cerr << "Invalid rewrite engine [--rewrite-engine]: " << optarg << "\n";
                        ok = false;
                    }
                    break;
                case 1019:
                    setRewriteEngine(RewriteEngine::IMPLICIT);
                    break;
                case 1020:
                    setRewriteEngine(RewriteEngine::IMPLICIT_ITER);
                    break;
                case 'P': {
                    if (!setRewriteSplitModeToken(optarg)) {
                        std::cerr << "Invalid split mode [-P|--split-mode]: " << optarg << "\n";
                        ok = false;
                    }
                    break;
                }
                case 1030:
                    if (!setRewriteSplitModeToken(optarg)) {
                        std::cerr << "Invalid rewrite split mode [--rewrite-split]: " << optarg << "\n";
                        ok = false;
                    }
                    break;
                case 1031:
                    if (!setRewriteDetectModeToken(optarg)) {
                        std::cerr << "Invalid rewrite detect mode [--rewrite-detect]: " << optarg << "\n";
                        ok = false;
                    }
                    break;
                case 'J':
                    setDumpJsonEnabled(true);
                    break;
                case 1036:
                    setDumpJsonBeforeGraphEnabled(true);
                    break;
                case 1035:
                    setDumpJsonBeforePruneEnabled(true);
                    break;
                case 'T':
                    setDumpDotEnabled(true);
                    break;
                case 'S':
                    setDumpStatEnabled(true);
                    break;
                case 'U':
                    setDumpConstEnabled(true);
                    break;
                case 1032: {
                    std::vector<std::string> specs;
                    appendSplitModeSpecs(specs, optarg);
                    if (specs.empty()) {
                        std::cerr << "Invalid dump kind [--dump]: " << optarg << "\n";
                        ok = false;
                        break;
                    }
                    for (const auto& spec : specs) {
                        if (!setDumpKindToken(spec, true)) {
                            std::cerr << "Invalid dump kind [--dump]: " << spec << "\n";
                            ok = false;
                        }
                    }
                    break;
                }
                case 1002:
                    setProfileStageToken("dred", true);
                    break;
                case 1003:
                    setProfileStageToken("inc", true);
                    break;
                case 1005:
                    setProfileStageToken("fc", true);
                    break;
                case 1013:
                    setProfileStageToken("inc-delete", true);
                    break;
                case 1014:
                    setProfileStageToken("wmc", true);
                    break;
                case 1009:
                    setProfileStageToken("inc-regional", true);
                    break;
                case 1011:
                    setProfileStageToken("inc-regional-heavy", true);
                    break;
                case 1012:
                    inc_regional_trace_tuples = optarg ? optarg : "";
                    break;
                case 1033: {
                    std::vector<std::string> specs;
                    appendSplitModeSpecs(specs, optarg);
                    if (specs.empty()) {
                        std::cerr << "Invalid profile stage [--profile-stage]: " << optarg << "\n";
                        ok = false;
                        break;
                    }
                    for (const auto& spec : specs) {
                        if (!setProfileStageToken(spec, true)) {
                            std::cerr << "Invalid profile stage [--profile-stage]: " << spec << "\n";
                            ok = false;
                        }
                    }
                    break;
                }
                case 1010:
                    setProfileStageToken("dep-graph", true);
                    break;
                case 1006:
                    post_del = true;
                    break;
                case 1015:
                    enable_inc_reord = true;
                    break;
                case 'Z':
                    setDetMode(DetMode::AUTO);
                    break;
                case 1017:
                    setDetMode(DetMode::OFF);
                    break;
                case 1007:
                    setDetMode(DetMode::FORCE);
                    break;
                case 1034:
                    if (!setDetModeToken(optarg)) {
                        std::cerr << "Invalid determinism mode [--det-mode]: " << optarg << "\n";
                        ok = false;
                    }
                    break;
                case 1008:
                    reuse_var_index = false;
                    break;
                case 1001:
                    single_rand_fast = false;
                    break;
                case 1016:
                    setRewriteDetectMode(RewriteDetectMode::COMPLETE);
                    break;
                case 1018:
                    setFullEvaluator(FullEvaluator::SCBF);
                    break;
                default: printHelpPage(exec_name); return false;
            }
        }

        if (full_evaluator == FullEvaluator::APPROX || approx_backend != ApproxBackend::NONE) {
            std::cerr << "Approximate full evaluators/backends are not supported by compiled runtimes yet\n";
            ok = false;
        }

        // update member fields
        input_dir = fact_dir;
        output_dir = out_dir;

        // return success state
        return ok;
    }

private:
    const IncrementalModeSpec& currentIncrementalModeSpec() const {
        return incModeSpec;
    }

    void syncCanonicalStateFromLegacyFields() {
        if (enable_implicit_iterate_split_rewrite) {
            rewrite_engine = RewriteEngine::IMPLICIT_ITER;
        } else if (enable_implicit_rewrite) {
            rewrite_engine = RewriteEngine::IMPLICIT;
        } else if (enable_rewrite) {
            rewrite_engine = RewriteEngine::LEGACY;
        } else {
            rewrite_engine = RewriteEngine::OFF;
        }

        RewriteSplitMode split = RewriteSplitMode::NAIVE;
        parseRewriteSplitModeToken(split_mode, split);
        rewrite_split_mode = split;
        rewrite_detect_mode = force_complete_siso_detect ? RewriteDetectMode::COMPLETE
                                                         : RewriteDetectMode::DIRTY_FRONTIER;
        if (det_force) {
            det_mode = DetMode::FORCE;
        } else if (det_opt) {
            det_mode = DetMode::AUTO;
        } else {
            det_mode = DetMode::OFF;
        }
        full_evaluator = enable_scbf ? FullEvaluator::SCBF : FullEvaluator::EXACT;
        approx_backend = ApproxBackend::NONE;
    }

    void syncLegacyFieldsFromCanonicalState() {
        enable_scbf = (full_evaluator == FullEvaluator::SCBF);

        enable_rewrite = rewrite_engine != RewriteEngine::OFF;
        enable_implicit_rewrite =
                rewrite_engine == RewriteEngine::IMPLICIT || rewrite_engine == RewriteEngine::IMPLICIT_ITER;
        enable_implicit_iterate_split_rewrite = rewrite_engine == RewriteEngine::IMPLICIT_ITER;

        switch (rewrite_split_mode) {
            case RewriteSplitMode::OFF:
                split_mode = "no-split";
                break;
            case RewriteSplitMode::NAIVE:
                split_mode = "naive-split";
                break;
            case RewriteSplitMode::COMPLETE:
                split_mode = "complete-split";
                break;
        }

        force_complete_siso_detect = rewrite_detect_mode == RewriteDetectMode::COMPLETE;

        switch (det_mode) {
            case DetMode::AUTO:
                det_opt = true;
                det_force = false;
                break;
            case DetMode::OFF:
                det_opt = false;
                det_force = false;
                break;
            case DetMode::FORCE:
                det_opt = false;
                det_force = true;
                break;
        }
    }

    /**
     * Prints the help page if it has been requested or there was a typo in the command line arguments.
     */
    void printHelpPage(const std::string& exec_name) const {
        std::cerr << "====================================================================\n";
        std::cerr << " Datalog Program: " << src << "\n";
        std::cerr << " Usage: " << exec_name << " [OPTION]\n\n";
        std::cerr << " Options:\n";
        std::cerr << "    -D <DIR>, --output=<DIR>     -- Specify directory for output relations\n";
        std::cerr << "                                    (default: " << output_dir << ")\n";
        std::cerr << "                                    (suppress output with \"\")\n";
        std::cerr << "    -F <DIR>, --facts=<DIR>      -- Specify directory for fact files\n";
        std::cerr << "                                    (default: " << input_dir << ")\n";
        std::cerr << "             --input-dir=<DIR>   -- Canonical alias for --facts\n";
        if (profiling) {
            std::cerr << "    -p <file>, --profile=<file>  -- Specify filename for profiling\n";
            std::cerr << "                                    (default: " << profile_name << ")\n";
            std::cerr << "             --profile-file=<file> -- Canonical alias for --profile\n";
        }
        std::cerr << "    -k <KR>, --knowledge=<KR>    -- Specify knowledge representation (bdd or sdd)\n";
        std::cerr << "                                    (default: " << knowledge_representation << ")\n";
        std::cerr << "             --dd-backend=<KR>   -- Canonical alias for --knowledge "
                  << ddBackendOptionSyntax() << "\n";
        std::cerr << "    -d, --derv-only[=<true|false>] -- Only compute the derivation graph\n";
        std::cerr << "             --derivation-only[=<true|false>] -- Canonical alias for --derv-only\n";
        std::cerr << "    -m, --setmode=<MODE>          -- Default online mode: "
                  << incrementalLegacyModeHelpText() << "\n";
        std::cerr << "             --sem-mode=<MODE>    -- Canonical semantic mode "
                  << semModeOptionSyntax() << "\n";
        std::cerr << "             --fc-mode=<MODE>     -- Canonical FC mode "
                  << fcModeOptionSyntax() << "\n";
        std::cerr << "             --full-evaluator=<MODE> -- Full evaluator "
                  << fullEvaluatorOptionSyntax() << "\n";
        std::cerr << "             --approx-backend=<MODE> -- Approx backend "
                  << approxBackendOptionSyntax() << " (reserved; runtime rejects for now)\n";
        std::cerr << "    -e, --merge-bi-imp           -- Enable merging mutually implying deterministic nodes during pruning\n";
        std::cerr << "             --no-merge-bi-imp   -- Disable merge-bi-imp even if compiled default enabled\n";
        std::cerr << "    --prune-extra                -- Enable outputless-component pruning in prune\n";
        std::cerr << "    --no-prune-extra             -- Disable outputless-component pruning\n";
        std::cerr << "    -C, --fold-const             -- Enable deterministic constant pre-analysis (no prune rewrite; negation ignored)\n";
        std::cerr << "    --no-fold-const              -- Disable deterministic constant pre-analysis\n";
        std::cerr << "    -r, --rewrite                -- Enable SISO-based graph rewriting\n";
        std::cerr << "    --rewrite-engine=<MODE>      -- Canonical rewrite engine "
                  << rewriteEngineOptionSyntax() << "\n";
        std::cerr << "    --implicit-rewrite           -- Enable experimental implicit-split rewrite pipeline\n";
        std::cerr << "    --implicit-iterate-split-rewrite -- Enable iterative implicit split<->rewrite fixpoint\n";
        std::cerr << "    --split-mode=<MODE>          -- Split mode for rewrite: no-split, naive-split, complete-split\n";
        std::cerr << "    --rewrite-split=<MODE>       -- Canonical rewrite split "
                  << rewriteSplitOptionSyntax() << "\n";
        std::cerr << "    --rewrite-detect=<MODE>      -- Canonical rewrite detect "
                  << rewriteDetectOptionSyntax() << "\n";
        std::cerr << "    --dumpjson                   -- Dump derivation graph JSON after prune\n";
        std::cerr << "    --dumpjson-before-graph      -- Dump ruleapp-reconstructed JSON before graph materialization\n";
        std::cerr << "    --dumpjson-before-prune      -- Dump derivation graph JSON before prune\n";
        std::cerr << "    --dumpdot                    -- Dump derivation graph DOT after prune\n";
        std::cerr << "    --dumpstat                   -- Dump derivation graph stats after prune\n";
        std::cerr << "    --dump=<LIST>                -- Canonical dump selector "
                  << dumpKindsOptionSyntax() << "\n";
        std::cerr << "    --dred-profile               -- Enable detailed DRed profiling (requires --profile)\n";
        std::cerr << "    --inc-profile                -- Enable incremental stage profiling\n";
        std::cerr << "    --fc-profile                 -- Enable detailed forward-compilation profiling\n";
        std::cerr << "    --profile-inc-delete         -- Enable incremental delete-phase profiling\n";
        std::cerr << "    --profile-wmc                -- Enable weighted model counting profiling\n";
        std::cerr << "    --profile-inc-regional       -- Enable inc-regional diagnostics/profiling\n";
        std::cerr << "    --profile-inc-regional-heavy -- Enable heavy inc-regional diagnostics (closure trace)\n";
        std::cerr << "    --inc-regional-trace-tuples=<LIST> -- Comma-separated tuples to trace\n";
        std::cerr << "    --trace-inc-regional=<LIST>  -- Canonical alias for --inc-regional-trace-tuples\n";
        std::cerr << "    --profile-stage=<LIST>       -- Canonical profile selector "
                  << profileStageOptionSyntax() << "\n";
        std::cerr << "    --profile-dep-graph          -- Enable dependency-graph profiling\n";
        std::cerr << "    --post-del                   -- Enable post-delete variable postprocess (FC)\n";
        std::cerr << "    --enable-inc-reord           -- Enable CUDD dynamic reordering in incremental turns\n";
        std::cerr << "    --dumpconst                  -- Dump constant pre-analysis details to file (negation ignored)\n";
        std::cerr << "    --det-opt                    -- Enable deterministic-relation analysis (default on)\n";
        std::cerr << "    --no-det-opt                 -- Disable deterministic-relation analysis\n";
        std::cerr << "    --det-force                  -- Force deterministic mode (skip derivation graph; emit prob=1.0)\n";
        std::cerr << "    --det-mode=<MODE>            -- Canonical determinism mode "
                  << detModeOptionSyntax() << "\n";
        std::cerr << "    --force-complete-siso-detect -- Disable dirty-frontier SISO detect and always scan full graph\n";
        std::cerr << "    --scbf                       -- Enable experimental SCBF full pipeline (opt-in)\n";
        std::cerr << "    --no-reuse-var-index         -- Disable reuse of freed CUDD variable indices (reuse is unsafe unless deletion fully removes vars)\n";
        std::cerr << "    --no-single-rand-fast        -- Disable single-randvar fast path in component FC\n";
#ifdef _OPENMP
        std::cerr << "    -j <NUM>, --jobs=<NUM>       -- Specify number of threads\n";
        if (num_jobs > 0) {
            std::cerr << "                                    (default: " << num_jobs << ")\n";
        } else {
            std::cerr << "                                    (default: auto)\n";
        }
#endif
        std::cerr << "    -h                           -- prints this help page.\n";
        std::cerr << "--------------------------------------------------------------------\n";
#ifdef SOUFFLE_GENERATOR_VERSION
        std::cerr << " Version: " << SOUFFLE_GENERATOR_VERSION << std::endl;
#endif
        std::cerr << " Word size: " << RAM_DOMAIN_SIZE << " bits" << std::endl;
        std::cerr << "--------------------------------------------------------------------\n";
        std::cerr << " Copyright (c) 2016-22 The Souffle Developers." << std::endl;
        std::cerr << " Copyright (c) 2013-16 Oracle and/or its affiliates." << std::endl;
        std::cerr << " All rights reserved.\n";
        std::cerr << "====================================================================\n";
    }

    /**
     *  Check whether a file exists in the file system
     */
    inline bool existFile(const std::string& name) const {
        struct stat buffer;
        if (stat(name.c_str(), &buffer) == 0) {
            if ((buffer.st_mode & S_IFREG) != 0) {
                return true;
            }
        }
        return false;
    }

    /**
     *  Check whether a directory exists in the file system
     */
    bool existDir(const std::string& name) const {
        struct stat buffer;
        if (stat(name.c_str(), &buffer) == 0) {
            if ((buffer.st_mode & S_IFDIR) != 0) {
                return true;
            }
        }
        return false;
    }

    /**
     *  Check whether the output is "-", for which the output should be stdout
     */
    bool dirIsStdout(const std::string& name) const {
        return name == "-";
    }

    IncrementalModeSpec incModeSpec;
};

}  // end of namespace souffle
