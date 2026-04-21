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

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <sys/stat.h>

#ifdef USE_CUSTOM_GETOPTLONG
#include "souffle/utility/GetOptLongImpl.h"
#else
#include <getopt.h>
#endif

namespace souffle {

inline std::string joinOutputPath(const std::string& dir, const std::string& filename) {
    if (dir.empty()) {
        return filename;
    }
    if (dir.back() == '/') {
        return dir + filename;
    }
    return dir + "/" + filename;
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
    std::string knowledge_representation;  // bdd, sdd are supported
    bool merge_bi_imp = true;  // enable merging mutually implying deterministic nodes
    bool prune_extra = false;  // enable extra prune pass (outputless components)
    bool fold_const = false;  // enable deterministic constant pre-analysis (no prune rewrite)
    bool enable_rewrite = false;  // enable smart rewrite dispatch
    bool enable_explicit_rewrite = false;  // force legacy explicit graph rewrite pipeline
    bool enable_implicit_rewrite = false;  // enable experimental implicit-split rewrite pipeline
    std::string split_mode = "naive-split";  // diagnostic split mode for rewrite: no-split/naive-split
    bool split_mode_explicit = false;  // true when --split-mode/-P was passed explicitly
    bool dump_json = false;  // dump derivation graph JSON after prune
    bool dump_dot = false;  // dump derivation graph DOT after prune
    bool dump_stat = false;  // dump derivation graph stats after prune
    bool dump_const = false;  // dump constant pre-analysis details
    bool dred_profile = false;  // enable detailed DRed profiling
    bool fc_profile = false;  // enable detailed forward-compilation profiling
    bool wmc_profile = false;  // enable weighted model counting profiling
    bool dep_graph_profile = false;  // enable dependency-graph profiling
    bool post_del = false;  // enable postprocessUselessVariables after deletion
    bool det_opt = false;  // enable deterministic-relation analysis and det gating
    bool det_force = false;  // force deterministic evaluation (skip derivation graph)
    bool reuse_var_index = true;  // reuse freed variable indices in CUDD (default on)
    bool single_rand_fast = true;  // enable single-randvar fast path in component FC
    bool force_full_siso_detect = false;  // force full-graph SISO detection (disable dirty-frontier detect)
    bool relax_compaction_dirty = true;  // dirty only the surviving compacted edge endpoints
    bool help_requested = false;  // true when help was printed intentionally
public:
    // all argument constructor
    CmdOptions(const char* s, const char* id, const char* od, bool pe, const char* pfn, std::size_t nj,
            std::string lfn = "log.txt", bool donly = false,
            bool merge_bi = true, bool foldconst = false, bool rewrite = false,
            bool dumpjson = false, bool dumpdot = false, bool dumpstat = false, bool dumpconst = false,
            bool dredProfile = false,
            const std::string& splitmode = "naive-split",
            bool fcProfile = false,
            bool wmcProfile = false,
            bool depGraphProfile = false,
            bool postDel = false)
            : src(s), input_dir(id), output_dir(od), profiling(pe), profile_name(pfn), num_jobs(nj), log_file_name(lfn), derivation_only(donly)
    , merge_bi_imp(merge_bi), fold_const(foldconst), enable_rewrite(rewrite),
      dump_json(dumpjson), dump_dot(dumpdot), dump_stat(dumpstat), dump_const(dumpconst),
      dred_profile(dredProfile), fc_profile(fcProfile),
      wmc_profile(wmcProfile),
      dep_graph_profile(depGraphProfile), post_del(postDel),
      split_mode(splitmode) {}

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
    bool isExplicitRewriteEnabled() const {
        return enable_explicit_rewrite;
    }
    bool isImplicitRewriteEnabled() const {
        return enable_implicit_rewrite;
    }
    const std::string& getSplitMode() const {
        return split_mode;
    }
    bool isSplitModeExplicit() const {
        return split_mode_explicit;
    }
    bool isDumpJsonEnabled() const {
        return dump_json;
    }
    void setDumpJsonEnabled(bool enabled) {
        dump_json = enabled;
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
    bool isDredProfileEnabled() const {
        return dred_profile;
    }
    bool isFcProfileEnabled() const {
        return fc_profile;
    }
    bool isWmcProfileEnabled() const {
        return wmc_profile;
    }
    bool isDepGraphProfileEnabled() const {
        return dep_graph_profile;
    }
    bool isPostDelEnabled() const {
        return post_del;
    }
    bool isDetOptEnabled() const {
        return det_opt;
    }
    bool isDetForceEnabled() const {
        return det_force;
    }
    bool isHelpRequested() const {
        return help_requested;
    }
    bool isReuseVarIndexEnabled() const {
        return reuse_var_index;
    }
    bool isSingleRandFastEnabled() const {
        return single_rand_fast;
    }
    bool isForceFullSisoDetectEnabled() const {
        return force_full_siso_detect;
    }
    bool isRelaxCompactionDirtyEnabled() const {
        return relax_compaction_dirty;
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
        option longOptions[] = {{"facts", true, nullptr, 'F'}, {"output", true, nullptr, 'D'},
                {"help", false, nullptr, 'h'},
                {"profile", true, nullptr, 'p'}, {"jobs", true, nullptr, 'j'}, {"index", true, nullptr, 'i'},
                {"knowledge", true, nullptr, 'k'}, {"logfile", true, nullptr, 'l'},
                {"derv-only", optional_argument, nullptr, 'd'},
                {"merge-bi-imp", false, nullptr, 'e'}, {"prune-extra", false, nullptr, 1004}, {"fold-const", false, nullptr, 'C'},
                {"rewrite", false, nullptr, 'r'},
                {"explicit-rewrite", false, nullptr, 1021},
                {"implicit-rewrite", false, nullptr, 1019},
                {"split-mode", true, nullptr, 'P'},
                {"dumpjson", false, nullptr, 'J'}, {"dumpdot", false, nullptr, 'T'},
                {"dumpstat", false, nullptr, 'S'},
                {"dumpconst", false, nullptr, 'U'},
                {"dred-profile", false, nullptr, 1002},
                {"fc-profile", false, nullptr, 1005},
                {"profile-wmc", false, nullptr, 1014},
                {"profile-dep-graph", false, nullptr, 1010},
                {"post-del", false, nullptr, 1006},
                {"det-opt", false, nullptr, 'Z'},
                {"det-force", false, nullptr, 1007},
                {"no-reuse-var-index", false, nullptr, 1008},
                {"no-single-rand-fast", false, nullptr, 1001},
                {"force-full-siso-detect", false, nullptr, 1016},
                {"no-relax-compaction-dirty", false, nullptr, 1020},
                // the terminal option -- needs to be null
                {nullptr, false, nullptr, 0}};

        // check whether all options are fine
        bool ok = true;
        knowledge_representation = "bdd";  // default knowledge representation
        int c; /* command-line arguments processing */
        while ((c = getopt_long(argc, argv, "D:F:hp:j:i:d::eC:rP:JTSUZ", longOptions, nullptr)) != EOF) {
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
                case 'h':
                    help_requested = true;
                    printHelpPage(exec_name);
                    return false;
                case 'k':
                   if (std::string(optarg) == "bdd") {
                        knowledge_representation = "bdd";
                    } else if (std::string(optarg) == "sdd") {
                        knowledge_representation = "sdd";
                    } else {
                        std::cerr << "Invalid knowledge representation: " << optarg << ", defaultly change to bdd\n";
                        knowledge_representation = "bdd";
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
                case 'e':
                    merge_bi_imp = true;
                    break;
                case 1004:
                    prune_extra = true;
                    break;
                case 'C':
                    fold_const = true;
                    break;
                case 'r':
                    enable_rewrite = true;
                    break;
                case 1021:
                    enable_rewrite = true;
                    enable_explicit_rewrite = true;
                    break;
                case 1019:
                    enable_rewrite = true;
                    enable_implicit_rewrite = true;
                    break;
                case 'P': {
                    split_mode_explicit = true;
                    std::string modeArg(optarg);
                    if (modeArg == "no-split" || modeArg == "none") {
                        split_mode = "no-split";
                    } else if (modeArg == "naive-split" || modeArg == "naive") {
                        split_mode = "naive-split";
                    } else {
                        std::cerr << "Invalid split mode [-P|--split-mode]: " << optarg << "\n";
                        ok = false;
                    }
                    break;
                }
                case 'J':
                    dump_json = true;
                    break;
                case 'T':
                    dump_dot = true;
                    break;
                case 'S':
                    dump_stat = true;
                    break;
                case 'U':
                    dump_const = true;
                    break;
                case 1002:
                    dred_profile = true;
                    break;
                case 1005:
                    fc_profile = true;
                    break;
                case 1014:
                    wmc_profile = true;
                    break;
                case 1010:
                    dep_graph_profile = true;
                    break;
                case 1006:
                    post_del = true;
                    break;
                case 'Z':
                    det_opt = true;
                    break;
                case 1007:
                    det_force = true;
                    break;
                case 1008:
                    reuse_var_index = false;
                    break;
                case 1001:
                    single_rand_fast = false;
                    break;
                case 1016:
                    force_full_siso_detect = true;
                    break;
                case 1020:
                    relax_compaction_dirty = false;
                    break;
                default: printHelpPage(exec_name); return false;
            }
        }

        // update member fields
        input_dir = fact_dir;
        output_dir = out_dir;

        if (enable_explicit_rewrite && enable_implicit_rewrite) {
            std::cerr << "Cannot combine --explicit-rewrite with --implicit-rewrite\n";
            ok = false;
        }
        if (enable_implicit_rewrite && split_mode_explicit && split_mode == "no-split") {
            std::cerr << "Cannot combine --implicit-rewrite with --split-mode=no-split; "
                      << "use --explicit-rewrite for graph-rewrite diagnostics.\n";
            ok = false;
        }
        // return success state
        return ok;
    }

private:
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
        if (profiling) {
            std::cerr << "    -p <file>, --profile=<file>  -- Specify filename for profiling\n";
            std::cerr << "                                    (default: " << profile_name << ")\n";
        }
        std::cerr << "    -k <KR>, --knowledge=<KR>    -- Specify knowledge representation (bdd or sdd)\n";
        std::cerr << "                                    (default: " << knowledge_representation << ")\n";
        std::cerr << "    -r, --rewrite                -- Enable artifact rewrite dispatcher\n";
        std::cerr << "    --det-opt                    -- Enable deterministic-relation analysis and graph gating\n";
        std::cerr << "    -l <FILE>, --logfile=<FILE>  -- Debugger JSON base name\n";
        std::cerr << "\n";
        std::cerr << " Advanced diagnostics (not needed for artifact reproduction):\n";
        std::cerr << "    -d, --derv-only[=<true|false>] -- Only compute the derivation graph\n";
        std::cerr << "    -e, --merge-bi-imp           -- Enable merging mutually implying deterministic nodes during pruning\n";
        std::cerr << "    --prune-extra                -- Enable outputless-component pruning in prune\n";
        std::cerr << "    -C, --fold-const             -- Enable deterministic constant pre-analysis (no prune rewrite; negation ignored)\n";
        std::cerr << "    --explicit-rewrite           -- Force explicit graph rewrite diagnostics\n";
        std::cerr << "    --implicit-rewrite           -- Force implicit-split rewrite diagnostics\n";
        std::cerr << "    --split-mode=<MODE>          -- Diagnostic split mode: no-split or naive-split\n";
        std::cerr << "    --dumpjson                   -- Dump derivation graph JSON after prune\n";
        std::cerr << "    --dumpdot                    -- Dump derivation graph DOT after prune\n";
        std::cerr << "    --dumpstat                   -- Dump derivation graph stats after prune\n";
        std::cerr << "    --dred-profile               -- Enable detailed DRed profiling (requires --profile)\n";
        std::cerr << "    --fc-profile                 -- Enable detailed forward-compilation profiling\n";
        std::cerr << "    --profile-wmc                -- Enable weighted model counting profiling\n";
        std::cerr << "    --profile-dep-graph          -- Enable dependency-graph profiling\n";
        std::cerr << "    --post-del                   -- Enable post-delete variable postprocess (FC)\n";
        std::cerr << "    --dumpconst                  -- Dump constant pre-analysis details to file (negation ignored)\n";
        std::cerr << "    --det-force                  -- Force deterministic mode (skip derivation graph; emit prob=1.0)\n";
        std::cerr << "    --force-full-siso-detect     -- Disable dirty-frontier SISO detect and always scan full graph\n";
        std::cerr << "    --no-relax-compaction-dirty  -- Keep conservative dirtying after deterministic edge compaction\n";
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
        std::cerr << "    -h, --help                   -- Print this help page\n";
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
};

}  // end of namespace souffle
