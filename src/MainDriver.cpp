/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file MainDriver.cpp
 *
 * Main driver for Souffle
 *
 ***********************************************************************/

#include "MainDriver.h"
#include "Global.h"
#include "ast/Clause.h"
#include "ast/Node.h"
#include "ast/Program.h"
#include "ast/TranslationUnit.h"
#include "ast/analysis/PrecedenceGraph.h"
#include "ast/analysis/SCCGraph.h"
#include "ast/analysis/typesystem/Type.h"
#include "ast/transform/AddNullariesToAtomlessAggregates.h"
#include "ast/transform/ComponentChecker.h"
#include "ast/transform/ComponentInstantiation.h"
#include "ast/transform/Conditional.h"
#include "ast/transform/ConstantNormalizationTransformer.h"
#include "ast/transform/DebugDeltaRelation.h"
#include "ast/transform/EvidenceChecker.h"
#include "ast/transform/ExecutionPlanChecker.h"
#include "ast/transform/ExpandEqrels.h"
#include "ast/transform/Fixpoint.h"
#include "ast/transform/FoldAnonymousRecords.h"
#include "ast/transform/GroundWitnesses.h"
#include "ast/transform/ProbQueryChecker.h"
#include "ast/transform/GroundedTermsChecker.h"
#include "ast/transform/IOAttributes.h"
#include "ast/transform/IODefaults.h"
#include "ast/transform/InlineRelations.h"
#include "ast/transform/InsertLatticeOperations.h"
#include "ast/transform/MagicSet.h"
#include "ast/transform/MaterializeAggregationQueries.h"
#include "ast/transform/MaterializeSingletonAggregation.h"
#include "ast/transform/MinimiseProgram.h"
#include "ast/transform/NameUnnamedVariables.h"
#include "ast/transform/NormaliseGenerators.h"
#include "ast/transform/PartitionBodyLiterals.h"
#include "ast/transform/Pipeline.h"
#include "ast/transform/PragmaChecker.h"
#include "ast/transform/ProbQueryConstraintLifting.h"
#include "ast/transform/ReduceExistentials.h"
#include "ast/transform/RemoveBooleanConstraints.h"
#include "ast/transform/RemoveEmptyRelations.h"
#include "ast/transform/RemoveRedundantRelations.h"
#include "ast/transform/RemoveRedundantSums.h"
#include "ast/transform/RemoveRelationCopies.h"
#include "ast/transform/ReplaceSingletonVariables.h"
#include "ast/transform/ResolveAliases.h"
#include "ast/transform/ResolveAnonymousRecordAliases.h"
#include "ast/transform/SemanticChecker.h"
#include "ast/transform/SimplifyAggregateTargetExpression.h"
#include "ast/transform/SimplifyConstantBinaryConstraints.h"
#include "ast/transform/SubsumptionQualifier.h"
#include "ast/transform/UniqueAggregationVariables.h"
#include "ast2ram/TranslationStrategy.h"
#include "ast2ram/UnitTranslator.h"
#include "ast2ram/online/TranslationStrategy.h"
#include "ast2ram/utility/TranslatorContext.h"
#include "config.h"
#include "parser/ParserDriver.h"
#include "ram/Node.h"
#include "ram/Program.h"
#include "ram/TranslationUnit.h"
#include "ram/transform/CollapseFilters.h"
#include "ram/transform/Conditional.h"
#include "ram/transform/EliminateDuplicates.h"
#include "ram/transform/ExpandFilter.h"
#include "ram/transform/HoistAggregate.h"
#include "ram/transform/HoistConditions.h"
#include "ram/transform/IfConversion.h"
#include "ram/transform/IfExistsConversion.h"
#include "ram/transform/Loop.h"
#include "ram/transform/MakeIndex.h"
#include "ram/transform/Parallel.h"
#include "ram/transform/ReorderConditions.h"
#include "ram/transform/ReorderFilterBreak.h"
#include "ram/transform/ReportIndex.h"
#include "ram/transform/Sequence.h"
#include "ram/transform/Transformer.h"
#include "ram/transform/TupleId.h"
#include "reports/DebugReport.h"
#include "reports/ErrorReport.h"
#include "souffle/CompiledOptions.h"
#include "souffle/RamTypes.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/FileUtil.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StreamUtil.h"
#include "souffle/utility/StringUtil.h"
#include "souffle/utility/SubProcess.h"
#include "synthesiser/GenDb.h"
#include "synthesiser/Synthesiser.h"

#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace souffle {

/**
 * Executes a binary file.
 */
[[noreturn]] void executeBinaryAndExit(Global& glb, const std::string& binaryFilename) {
    assert(!binaryFilename.empty() && "binary filename cannot be blank");

    std::map<char const*, std::string> env;
    if (glb.config().has("library-dir")) {
        auto escapeLdPath = [](auto&& xs) { return escape(xs, {':', ' '}, "\\"); };
        auto ld_path = toString(
                join(map(glb.config().getMany("library-dir"), escapeLdPath), std::string(1, PATHdelimiter)));
#if defined(_MSC_VER)
        std::size_t l;
        std::wstring env_path(ld_path.length() + 1, L' ');
        ::mbstowcs_s(&l, env_path.data(), env_path.size(), ld_path.data(), ld_path.size());
        env_path.resize(l - 1);

        DWORD n = GetEnvironmentVariableW(L"PATH", nullptr, 0);
        if (n > 0) {
            // append path
            std::unique_ptr<wchar_t[]> orig(new wchar_t[n]);
            GetEnvironmentVariableW(L"PATH", orig.get(), n);
            env_path = env_path + L";" + std::wstring(orig.get());
        }
        SetEnvironmentVariableW(L"PATH", env_path.c_str());

#elif defined(__APPLE__)
        env["DYLD_LIBRARY_PATH"] = ld_path;
#else
        env["LD_LIBRARY_PATH"] = ld_path;
#endif
    }

    auto exit = execute(binaryFilename, {}, env);
    if (!exit) throw std::invalid_argument("failed to execute `" + binaryFilename + "`");

    if (!glb.config().has("dl-program")) {
        remove(binaryFilename.c_str());
        remove((binaryFilename + ".cpp").c_str());
    }

    std::exit(*exit);
}

/**
 * Compiles the given source file to a binary file.
 */
void compileToBinary(
        Global& glb, const std::string& command, std::vector<fs::path>& sourceFilenames, fs::path binary) {
    std::vector<std::string> argv;

    argv.push_back(command);

    if (glb.config().has("swig")) {
        argv.push_back("-s");
        argv.push_back(glb.config().get("swig"));
    }

    if (glb.config().has("verbose")) {
        argv.push_back("-v");
    }

    for (auto&& path : glb.config().getMany("library-dir")) {
        // The first entry may be blank
        if (path.empty()) {
            continue;
        }
        argv.push_back(tfm::format("-L%s", path));
    }
    for (auto&& library : glb.config().getMany("libraries")) {
        // The first entry may be blank
        if (library.empty()) {
            continue;
        }
        argv.push_back(tfm::format("-l%s", library));
    }

    for (fs::path srcFile : sourceFilenames) {
        argv.push_back(srcFile.string());
    }

    argv.push_back("--with-cudd");

    argv.push_back("-o");
    argv.push_back(binary.string());

#if defined(_MSC_VER)
    const char* interpreter = "python";
#else
    const char* interpreter = "python3";
#endif
    auto exit = execute(interpreter, argv);
    if (!exit) throw std::invalid_argument(tfm::format("unable to execute tool <python3 %s>", command));
    if (*exit != 0) throw std::invalid_argument("failed to compile C++ sources");
}

class InputProvider {
public:
    virtual ~InputProvider() {}
    virtual FILE* getInputStream() = 0;
    virtual bool readInput(std::string& Buf) {
        FILE* Stream = getInputStream();
        if (Stream == nullptr) {
            return false;
        }
        const size_t ChunkSize = 4096;
        size_t TotalBytesRead = 0;
        bool Success = false;
        while (1) {
            Buf.resize(TotalBytesRead + ChunkSize);
            size_t ReadSize = fread(&Buf.data()[TotalBytesRead], 1, ChunkSize, Stream);
            if (ReadSize < ChunkSize) {
                Buf.resize(TotalBytesRead + ReadSize);
                if (feof(Stream)) {
                    Success = true;
                    break;
                } else if (ferror(Stream)) {
                    break;
                }
            }
            TotalBytesRead += ReadSize;
        }
        return Success && endInput();
    }
    virtual bool endInput() = 0;
    virtual bool reducedConsecutiveNonLeadingWhitespaces() const = 0;
};

class FileInput : public InputProvider {
public:
    FileInput(const std::filesystem::path& path) : Path(path) {}

    ~FileInput() {
        if (Stream) {
            fclose(Stream);
        }
    }

    FILE* getInputStream() override {
        if (std::filesystem::exists(Path)) {
            Stream = fopen(Path.string().c_str(), "rb");
            return Stream;
        } else {
            return nullptr;
        }
    }

    bool endInput() override {
        if (Stream == nullptr) {
            return false;
        } else {
            fclose(Stream);
            Stream = nullptr;
            return true;
        }
    }

    bool reducedConsecutiveNonLeadingWhitespaces() const override {
        return false;
    }

private:
    const std::filesystem::path Path;
    FILE* Stream = nullptr;
};

class PreprocInput : public InputProvider {
public:
    PreprocInput(const std::filesystem::path& path, MainConfig& conf, const std::string& exec,
            const std::string& options)
            : Exec(which(exec)), Options(options), InitCmd(), Path(path), Conf(conf) {}

    PreprocInput(const std::filesystem::path& path, MainConfig& conf, const std::string& cmd)
            : Exec(), Options(), InitCmd(cmd), Path(path), Conf(conf) {}

    virtual ~PreprocInput() {
        if (Stream) {
            pclose(Stream);
        }
    }

    FILE* getInputStream() override {
        Cmd.str("");

        if (Exec) {
            if (Exec->empty()) {
                return nullptr;
            }
            Cmd << *Exec;
        } else if (InitCmd) {
            Cmd << *InitCmd;
        } else {
            return nullptr;
        }

        if (Options && !Options->empty()) {
            Cmd << " ";
            Cmd << *Options;
        }

        Cmd << " ";
        Cmd << toString(join(Conf.getMany("include-dir"), " ",
                [&](auto&& os, auto&& dir) { tfm::format(os, "-I \"%s\"", dir); }));

        if (Conf.has("macro")) {
            Cmd << " " << Conf.get("macro");
        }
        // Add RamDomain size as a macro
        Cmd << " -DRAM_DOMAIN_SIZE=" << std::to_string(RAM_DOMAIN_SIZE);
        Cmd << " \"" + Path.string() + "\"";

#if defined(_MSC_VER)
        // cl.exe prints the input file name on the standard error stream,
        // we must silent it in order to preserve an empty error output
        // because Souffle test-suite is sensible to error outputs.
        Cmd << " 2> nul";
#endif

        Stream = popen(Cmd.str().c_str(), "r");
        return Stream;
    }

    bool endInput() override {
        const int Status = pclose(Stream);
        Stream = nullptr;
        if (Status == -1) {
            perror(nullptr);
            std::cerr << "Failed to close pre-processor pipe\n";
            return false;
        } else if (Status != 0) {
            std::cerr << "Pre-processor command failed with code " << Status << ": '" << Cmd.str() << "'\n";
            return false;
        }
        return true;
    }

    virtual bool reducedConsecutiveNonLeadingWhitespaces() const override {
        return true;
    }

    static bool available(const std::string& Exec) {
        return !which(Exec).empty();
    }

private:
    std::optional<std::string> Exec;
    std::optional<std::string> Options;
    std::optional<std::string> InitCmd;
    std::filesystem::path Path;
    MainConfig& Conf;
    std::stringstream Cmd;
    FILE* Stream = nullptr;
};

class GCCPreprocInput : public PreprocInput {
public:
    GCCPreprocInput(const std::filesystem::path& mainSource, MainConfig& conf)
            : PreprocInput(mainSource, conf, "gcc", "-x c -E") {}

    virtual ~GCCPreprocInput() {}

    static bool available() {
        return PreprocInput::available("gcc");
    }

    bool reducedConsecutiveNonLeadingWhitespaces() const override {
        return true;
    }
};

class MCPPPreprocInput : public PreprocInput {
public:
    MCPPPreprocInput(const std::filesystem::path& mainSource, MainConfig& conf)
            : PreprocInput(mainSource, conf, "mcpp", "-e utf8 -W0") {}

    virtual ~MCPPPreprocInput() {}

    static bool available() {
        return PreprocInput::available("mcpp");
    }

    bool reducedConsecutiveNonLeadingWhitespaces() const override {
        return true;
    }
};

static WarnSet process_warn_opts(const Global& glb) {
    WarnSet warns;
    if (!glb.config().has("no-warn")) {
        if (glb.config().has("warn")) {
            for (auto&& option : glb.config().getMany("warn")) {
                if (option == "all") {
                    warns.set();
                } else {
                    auto valid = warns.setStr(option);
                    if (!valid) {
                        throw std::runtime_error("no such warning " + std::string(option));
                    }
                }
            }
        }
        if (glb.config().has("wno")) {
            for (auto&& option : glb.config().getMany("wno")) {
                if (option == "none") {  // default
                } else if (option == "all") {
                    warns.reset();
                } else {
                    auto valid = warns.resetStr(option);
                    if (!valid) {
                        throw std::runtime_error("no such warning " + std::string(option));
                    }
                }
            }
        }
    }
    return warns;
}

Own<ast::transform::PipelineTransformer> astTransformationPipeline(Global& glb) {
    // TODO: should check the transformer sequence; keep the useful ones; modify the others to fit the prob setting
    // clang-format off
    // Equivalence pipeline
    // TODO: move 0.0 prob clauses transformer
    auto equivalencePipeline =
            mk<ast::transform::PipelineTransformer>(mk<ast::transform::NameUnnamedVariablesTransformer>(),
                    mk<ast::transform::FixpointTransformer>(mk<ast::transform::MinimiseProgramTransformer>()),
                    // mk<ast::transform::ReplaceSingletonVariablesTransformer>(),
                    mk<ast::transform::RemoveRelationCopiesTransformer>(),
                    mk<ast::transform::RemoveEmptyRelationsTransformer>()
                    ,mk<ast::transform::RemoveRedundantRelationsTransformer>()
                    );

    // Magic-Set pipeline
    auto magicPipeline = mk<ast::transform::PipelineTransformer>(
            mk<ast::transform::ConditionalTransformer>(
                    glb.config().has("magic-transform"), mk<ast::transform::ExpandEqrelsTransformer>()),
            mk<ast::transform::MagicSetTransformer>(), mk<ast::transform::ResolveAliasesTransformer>(),
            mk<ast::transform::RemoveRelationCopiesTransformer>(),
            mk<ast::transform::RemoveEmptyRelationsTransformer>(),
            mk<ast::transform::RemoveRedundantRelationsTransformer>(), clone(equivalencePipeline));
    //
    // // Partitioning pipeline
    // auto partitionPipeline =
    //         mk<ast::transform::PipelineTransformer>(mk<ast::transform::NameUnnamedVariablesTransformer>(),
    //                 mk<ast::transform::PartitionBodyLiteralsTransformer>()
    //                 ,  mk<ast::transform::ReplaceSingletonVariablesTransformer>()
    //                 );
    //
    // // Provenance pipeline
    // auto provenancePipeline = mk<ast::transform::ConditionalTransformer>(glb.config().has("provenance"),
    //         mk<ast::transform::PipelineTransformer>(mk<ast::transform::ExpandEqrelsTransformer>(),
    //                 mk<ast::transform::NameUnnamedVariablesTransformer>()));
    //
    // // Main pipeline // TODO: Maybe some passes is unused or invalid or should be changed under prob setting
    auto pipeline = mk<ast::transform::PipelineTransformer>(mk<ast::transform::ComponentChecker>(),
            mk<ast::transform::EvidenceSemanticChecker>(),
            mk<ast::transform::ComponentInstantiationTransformer>(),
            // mk<ast::transform::LatticeTransformer>(),
            mk<ast::transform::ProbQueryChecker>(),
            mk<ast::transform::DebugDeltaRelationTransformer>(),
            mk<ast::transform::IODefaultsTransformer>(),
            // mk<ast::transform::SimplifyAggregateTargetExpressionTransformer>(),
            // mk<ast::transform::UniqueAggregationVariablesTransformer>(),
            // mk<ast::transform::FixpointTransformer>(mk<ast::transform::PipelineTransformer>(  // TODO: useless
            //         mk<ast::transform::ResolveAnonymousRecordAliasesTransformer>(),
            //         mk<ast::transform::FoldAnonymousRecords>())),
            // mk<ast::transform::SubsumptionQualifierTransformer>(), mk<ast::transform::SemanticChecker>(),  // TODO: useless
            // mk<ast::transform::GroundWitnessesTransformer>(),  // TODO: we (pdatalog) will never support aggregation and other datalog extensions right?
            // mk<ast::transform::UniqueAggregationVariablesTransformer>(),
            // mk<ast::transform::MaterializeSingletonAggregationTransformer>(),
            // mk<ast::transform::FixpointTransformer>(
            //         mk<ast::transform::MaterializeAggregationQueriesTransformer>()),
            // mk<ast::transform::RemoveRedundantSumsTransformer>(),
            // mk<ast::transform::NormaliseGeneratorsTransformer>(),
            mk<ast::transform::ResolveAliasesTransformer>(),
            mk<ast::transform::RemoveBooleanConstraintsTransformer>(),
            mk<ast::transform::ResolveAliasesTransformer>(),
            mk<ast::transform::MinimiseProgramTransformer>(),  // TODO, avoid removing probablistic clauses
            // mk<ast::transform::InlineUnmarkExcludedTransform>(),
            // mk<ast::transform::InlineRelationsTransformer>(),
            mk<ast::transform::GroundedTermsChecker>(),
            mk<ast::transform::ResolveAliasesTransformer>(),
            mk<ast::transform::SimplifyConstantBinaryConstraintsTransformer>(),
            mk<ast::transform::RemoveBooleanConstraintsTransformer>(),
            mk<ast::transform::RemoveRedundantRelationsTransformer>(),
            mk<ast::transform::RemoveRelationCopiesTransformer>(),
            mk<ast::transform::RemoveEmptyRelationsTransformer>(),
            // mk<ast::transform::ReplaceSingletonVariablesTransformer>(),
            mk<ast::transform::FixpointTransformer>(mk<ast::transform::PipelineTransformer>(
                    mk<ast::transform::ReduceExistentialsTransformer>(),
                    mk<ast::transform::RemoveRedundantRelationsTransformer>())),
            mk<ast::transform::RemoveRelationCopiesTransformer>(),
            // std::move(partitionPipeline),
            std::move(equivalencePipeline),
            mk<ast::transform::RemoveRelationCopiesTransformer>(),
            std::move(magicPipeline), mk<ast::transform::RemoveEmptyRelationsTransformer>(),
            // mk<ast::transform::AddNullariesToAtomlessAggregatesTransformer>(),
            // mk<ast::transform::ExecutionPlanChecker>(), // std::move(provenancePipeline),
            mk<ast::transform::IOAttributesTransformer>(),
            mk<ast::transform::ConstantNormalizationTransformer>());
    // clang-format on

    return pipeline;
}

Own<ast2ram::UnitTranslator> getUnitTranslator(Global& glb) {
    auto translationStrategy = mk<ast2ram::TranslationStrategy, ast2ram::online::TranslationStrategy>();
    auto unitTranslator = Own<ast2ram::UnitTranslator>(translationStrategy->createUnitTranslator());
    std::cout << "unitTranslator created, " << translationStrategy->getName() << std::endl;
    return unitTranslator;
}

Own<ram::transform::Transformer> ramTransformerSequence(Global& glb) {
    using namespace ram::transform;
    // TODO: should check the transformer sequence; one of them is not compatible with unnamed variables in prob
    // clang-format off
    Own<Transformer> ramTransform = mk<TransformerSequence>(
            mk<LoopTransformer>(mk<TransformerSequence>(mk<ExpandFilterTransformer>(),
                    mk<HoistConditionsTransformer>(), mk<MakeIndexTransformer>())),
            mk<IfConversionTransformer>(),
            mk<IfExistsConversionTransformer>(),
            mk<CollapseFiltersTransformer>(),
            mk<TupleIdTransformer>(),
            mk<LoopTransformer>(
                    mk<TransformerSequence>(mk<HoistAggregateTransformer>(), mk<TupleIdTransformer>())),
            mk<ExpandFilterTransformer>(), mk<HoistConditionsTransformer>(),
            mk<CollapseFiltersTransformer>(), mk<EliminateDuplicatesTransformer>(),
            mk<ReorderConditionsTransformer>(), mk<LoopTransformer>(mk<ReorderFilterBreak>()),
            mk<ConditionalTransformer>(
                    // job count of 0 means all cores are used.
                    [&]() -> bool { return std::stoi(glb.config().get("jobs")) != 1; },
                    mk<ParallelTransformer>()),
            mk<ReportIndexTransformer>());
    // clang-format on
    // Own<Transformer> ramTransform = mk<TransformerSequence>();
    return ramTransform;
}

const char* packageVersion() {
    return PACKAGE_VERSION;
}

std::size_t ramDomainSizeInBits() {
    return RAM_DOMAIN_SIZE;
}

std::string versionFooter() {
    std::stringstream footer;
    footer << "----------------------------------------------------------------------------" << std::endl;
    footer << "Version: " << packageVersion() << std::endl;
    footer << "Word size: " << ramDomainSizeInBits() << " bits" << std::endl;
    footer << "Options enabled:";
#ifdef USE_LIBFFI
    footer << " ffi";
#endif
#ifdef _OPENMP
    footer << " openmp";
#endif
#ifdef USE_NCURSES
    footer << " ncurses";
#endif
#ifdef USE_SQLITE
    footer << " sqlite";
#endif
#ifdef USE_LIBZ
    footer << " zlib";
#endif
    footer << std::endl;
    footer << "----------------------------------------------------------------------------" << std::endl;
    footer << "Copyright (c) 2016-22 The Souffle Developers." << std::endl;
    footer << "Copyright (c) 2013-16 Oracle and/or its affiliates." << std::endl;
    footer << "All rights reserved." << std::endl;
    footer << "============================================================================" << std::endl;

    return footer.str();
}

std::vector<MainOption> getMainOptions() {
    char nextOptChar = 1;
    // clang-format off
  std::vector<MainOption> options{
      {"", 0, "", "", false, ""},
      {"auto-schedule", 'a', "FILE", "", false,
          "Use profile auto-schedule <FILE> for auto-scheduling."},
      {"compile", 'c', "", "", false,
          "Generate C++ source code, compile to a binary executable, then run this "
          "executable."},
      {"compile-many", 'C', "", "", false,
          "Generate C++ source code in multiple files, compile to a binary executable, then "
          "run this "
          "executable."},
      {"debug-report", 'r', "FILE", "", false,
          "Write HTML debug report to <FILE>."},
      {"disable-transformers", 'z', "TRANSFORMERS", "", false,
          "Disable the given AST transformers."},
      {"no-souffle-opt", nextOptChar++, "", "", false,
          "Disable RAM optimizations (transformations, join order, guard hoisting)."},
      {"dl-program", 'o', "FILE", "", false,
          "Generate C++ source code, written to <FILE>, and compile this to a "
          "binary executable (without executing it)."},
      {"emit-statistics", nextOptChar++, "", "", false,
          "Enable collection of statistics for auto-scheduling"},
      {"fact-dir", 'F', "DIR", ".", false,
          "Specify directory for fact files."},
      {"input-dir", nextOptChar++, "DIR", "", false,
          "Canonical alias for --fact-dir when baking runtime defaults."},
      {"generate", 'g', "FILE", "", false,
          "Generate C++ source code for the given Datalog program and write it to "
          "<FILE>. If <FILE> is `-` then stdout is used."},
      {"generate-many", 'G', "DIR", "", false,
          "Generate C++ source code in multiple files for the given Datalog program "
          "and write it to <DIR>."},
      {"generate-namespace", 'N', "NS", "", false,
       "The namespace of generated C++ source code. Empty name denotes the anonymous "
       "namespace."},
      {"help", 'h', "", "", false,
          "Display this help message."},
      {"include-dir", 'I', "DIR", ".", true,
          "Specify directory for include files."},
      {"inline-exclude", nextOptChar++, "RELATIONS", "", false,
          "Prevent the given relations from being inlined. Overrides any `inline` qualifiers."},
      {"jobs", 'j', "N", "1", false,
          "Run compiler in parallel using N threads, N=auto for system default."},
      {"legacy", nextOptChar++, "", "", false,
          "Enable legacy support."},
      {"libraries", 'l', "FILE", "", true,
          "Specify libraries."},
      {"library-dir", 'L', "DIR", "", true,
          "Specify directory for library files."},
      {"macro", 'M', "MACROS", "", false,
          "Set macro definitions for the pre-processor"},
      {"magic-transform", 'm', "RELATIONS", "", false,
          "Enable magic set transformation changes on the given relations, use '*' "
          "for all."},
      {"magic-transform-exclude", nextOptChar++, "RELATIONS", "", false,
          "Disable magic set transformation changes on the given relations. Overrides "
          "`magic-transform`. Implies `inline-exclude` for the given relations."},
      {"no-preprocessor", nextOptChar++, "", "", false,
          "Do not use a C preprocessor."},
      {"no-warn", 'w', "", "", false,
          "Disable warnings."},
      {"output-dir", 'D', "DIR", ".", false,
          "Specify directory for output files. If <DIR> is `-` then stdout is used."},
      {"parse-errors", nextOptChar++, "", "", false,
          "Show parsing errors, if any, then exit."},
      {"pragma", 'P', "OPTIONS", "", true,
          "Set pragma options."},
      {"preprocessor", nextOptChar++, "CMD", "", false,
          "C preprocessor to use."},
      {"profile", 'p', "FILE", "", false,
          "Enable profiling, and write profile data to <FILE>."},
      {"profile-file", nextOptChar++, "FILE", "", false,
          "Canonical alias for --profile when baking runtime defaults."},
      {"dred-profile", nextOptChar++, "", "", false,
          "Enable detailed DRed profiling (requires --profile to emit data)."},
      {"profile-frequency", nextOptChar++, "", "", false,
          "Enable the frequency counter in the profiler."},
      {"setmode", 0, incrementalModeOptionSyntax(), "inc-naive", false,
          "Set the incremental mode (default: inc-naive)."},
      {"sem-mode", nextOptChar++, semModeOptionSyntax(), "", false,
          "Canonical semantic mode for generated runtime defaults."},
      {"fc-mode", nextOptChar++, fcModeOptionSyntax(), "", false,
          "Canonical forward-compilation mode for generated runtime defaults."},
      {"full-evaluator", nextOptChar++, fullEvaluatorOptionSyntax(), "", false,
          "Canonical full evaluator for generated runtime defaults."},
      {"dd-backend", nextOptChar++, ddBackendOptionSyntax(), "", false,
          "Canonical DD backend for generated runtime defaults."},
      {"approx-backend", nextOptChar++, approxBackendOptionSyntax(), "", false,
          "Canonical approximate backend for generated runtime defaults."},
      {"rewrite-engine", nextOptChar++, rewriteEngineOptionSyntax(), "", false,
          "Canonical rewrite engine for generated runtime defaults."},
      {"rewrite-split", nextOptChar++, rewriteSplitOptionSyntax(), "", false,
          "Canonical rewrite split mode for generated runtime defaults."},
      {"rewrite-detect", nextOptChar++, rewriteDetectOptionSyntax(), "", false,
          "Canonical rewrite detection policy for generated runtime defaults."},
      {"det-mode", nextOptChar++, detModeOptionSyntax(), "", false,
          "Canonical determinism mode for generated runtime defaults."},
      {"dump", nextOptChar++, dumpKindsOptionSyntax(), "", false,
          "Canonical dump selector for generated runtime defaults."},
      {"profile-stage", nextOptChar++, profileStageOptionSyntax(), "", false,
          "Canonical profile selector for generated runtime defaults."},
      {"trace-inc-regional", nextOptChar++, "LIST", "", false,
          "Canonical alias for inc-regional tuple tracing defaults."},
      {"derv-only", 'd', "", "", false, "Only compute the derivation graph."}, // TODO
      {"derivation-only", nextOptChar++, "", "", false,
          "Canonical alias for --derv-only."},
      {"log-file", nextOptChar++, "FILE", "", false,
          "Default debugger log filename for generated runtimes."},
      {"merge-bi-imp", nextOptChar++, "", "", false,
          "Enable merge-bi-imp by default in the generated runtime."},
      {"no-merge-bi-imp", nextOptChar++, "", "", false,
          "Disable merge-bi-imp in the generated runtime."},
      {"prune-extra", nextOptChar++, "", "", false,
          "Enable extra prune pass by default in the generated runtime."},
      {"no-prune-extra", nextOptChar++, "", "", false,
          "Disable extra prune pass in the generated runtime."},
      {"fold-const", nextOptChar++, "", "", false,
          "Enable deterministic constant pre-analysis by default in the generated runtime."},
      {"no-fold-const", nextOptChar++, "", "", false,
          "Disable deterministic constant pre-analysis in the generated runtime."},
    {"online", 'O', "", "", false,
          "Enable online compilation that allows interactive incremental updates"}, // TODO
    {"full-only", 'x', "", "", false,
        "Debugging purpose... do not generate inc code for online mode"}, // TODO
      {"show", nextOptChar++, "[ <see-list> ]", "", true,
          "Print selected program information.\n"
          "Modes:\n"
              "\tinitial-ast\n"
              "\tinitial-ram\n"
              "\tparse-errors\n"
              "\tprecedence-graph\n"
              "\tprecedence-graph-text\n"
              "\tscc-graph\n"
              "\tscc-graph-text\n"
              "\ttransformed-ast\n"
              "\ttransformed-ram\n"
              "\ttype-analysis"},
      {"swig", 's', "LANG", "", false,
          "Generate SWIG interface for given language. The values <LANG> accepts is java and "
          "python. "},
      {"verbose", 'v', "", "", false,
          "Verbose output."},
      {"version", nextOptChar++, "", "", false,
          "Version."},
      {"warn", 'W', "WARN", "all", true,
          "Enable a warning."},
      {"wno", nextOptChar++, "WARN", "none", true,
          "Disable a specific warning."},
      // TODO(lb):
      // {"Werror", '\xc', "WARN", "none", false, "Turn a warning into an error."},
  };
  // clang-format off
  return options;
}

namespace {

bool isExplicitlySet(const MainConfig& config, std::string_view key) {
    return config.state(key) == MainConfig::State::set;
}

void requireNoConflict(const MainConfig& config, std::string_view onKey, std::string_view offKey,
        const std::string& label) {
    if (isExplicitlySet(config, onKey) && isExplicitlySet(config, offKey)) {
        throw std::runtime_error("Conflicting " + label + " flags: --" + std::string(onKey) +
                                 " and --" + std::string(offKey));
    }
}

void setConfigBool(MainConfig& config, std::string_view key, bool enabled) {
    if (enabled) {
        config.set(std::string(key));
    } else {
        config.unset(key);
    }
}

void applyListAliasToLegacyFlags(MainConfig& config, std::string_view key,
        const std::vector<std::pair<std::string, std::string>>& tokenToLegacyKey) {
    if (!isExplicitlySet(config, key)) {
        return;
    }
    std::vector<std::string> specs;
    appendSplitModeSpecs(specs, config.get(key));
    if (specs.empty()) {
        throw std::runtime_error("Empty value for --" + std::string(key));
    }
    for (const auto& [_, legacy] : tokenToLegacyKey) {
        config.unset(legacy);
    }
    for (const auto& spec : specs) {
        bool matched = false;
        for (const auto& [token, legacy] : tokenToLegacyKey) {
            if (normalizeFlagToken(spec) == token) {
                config.set(legacy);
                matched = true;
                break;
            }
        }
        if (!matched) {
            throw std::runtime_error("Unsupported value for --" + std::string(key) + ": " + spec);
        }
    }
}

void canonicalizeForkRuntimeDefaults(MainConfig& config) {
    if (isExplicitlySet(config, "input-dir")) {
        config.set("fact-dir", config.get("input-dir"));
    }
    if (isExplicitlySet(config, "profile-file")) {
        config.set("profile", config.get("profile-file"));
    }
    if (isExplicitlySet(config, "derivation-only")) {
        config.set("derv-only");
    }
    if (isExplicitlySet(config, "log-file")) {
        config.set("logfile", config.get("log-file"));
    }
    if (isExplicitlySet(config, "trace-inc-regional")) {
        config.set("inc-regional-trace-tuples", config.get("trace-inc-regional"));
    }

    requireNoConflict(config, "merge-bi-imp", "no-merge-bi-imp", "merge-bi-imp");
    requireNoConflict(config, "prune-extra", "no-prune-extra", "prune-extra");
    requireNoConflict(config, "fold-const", "no-fold-const", "fold-const");
    setConfigBool(config, "merge-bi-imp", isExplicitlySet(config, "merge-bi-imp"));
    if (isExplicitlySet(config, "no-merge-bi-imp")) {
        config.unset("merge-bi-imp");
    }
    setConfigBool(config, "prune-extra", isExplicitlySet(config, "prune-extra"));
    if (isExplicitlySet(config, "no-prune-extra")) {
        config.unset("prune-extra");
    }
    setConfigBool(config, "fold-const", isExplicitlySet(config, "fold-const"));
    if (isExplicitlySet(config, "no-fold-const")) {
        config.unset("fold-const");
    }

    IncrementalModeSpec mode;
    std::string canonicalMode;
    if (!parseLegacyModeToken(config.get("setmode"), mode, &canonicalMode)) {
        throw std::runtime_error(
                "--setmode expects a legacy mode (" + std::string(incrementalLegacyModeHelpText()) + ")");
    }
    config.set("setmode", canonicalMode);

    if (isExplicitlySet(config, "sem-mode")) {
        SemMode sem = mode.sem;
        if (!parseSemModeToken(config.get("sem-mode"), sem)) {
            throw std::runtime_error("--sem-mode expects " + std::string(semModeOptionSyntax()));
        }
        mode.sem = sem;
        if (!isExplicitlySet(config, "fc-mode")) {
            mode.fc = reconcileFcModeForSem(mode.sem, mode.fc);
        }
    }
    if (isExplicitlySet(config, "fc-mode")) {
        FcMode fc = mode.fc;
        if (!parseFcModeToken(config.get("fc-mode"), fc)) {
            throw std::runtime_error("--fc-mode expects " + std::string(fcModeOptionSyntax()));
        }
        mode.fc = fc;
    }
    config.set("sem-mode", semModeTokenLabel(mode.sem));
    config.set("fc-mode", fcModeTokenLabel(mode.fc));

    FullEvaluator evaluator = FullEvaluator::EXACT;
    if (isExplicitlySet(config, "full-evaluator")) {
        if (!parseFullEvaluatorToken(config.get("full-evaluator"), evaluator)) {
            throw std::runtime_error("--full-evaluator expects " + std::string(fullEvaluatorOptionSyntax()));
        }
        if (evaluator == FullEvaluator::APPROX) {
            throw std::runtime_error(
                    "--full-evaluator=approx is not supported for generated runtimes yet");
        }
    }
    config.set("full-evaluator", fullEvaluatorLabel(evaluator));
    setConfigBool(config, "scbf", evaluator == FullEvaluator::SCBF);

    if (isExplicitlySet(config, "approx-backend")) {
        ApproxBackend backend = ApproxBackend::NONE;
        if (!parseApproxBackendToken(config.get("approx-backend"), backend)) {
            throw std::runtime_error("--approx-backend expects " + std::string(approxBackendOptionSyntax()));
        }
        if (backend != ApproxBackend::NONE) {
            throw std::runtime_error(
                    "--approx-backend is not supported for generated runtimes yet");
        }
        config.set("approx-backend", approxBackendLabel(backend));
    } else {
        config.set("approx-backend", approxBackendLabel(ApproxBackend::NONE));
    }

    std::string ddBackend = "bdd";
    if (isExplicitlySet(config, "dd-backend")) {
        if (!parseDdBackendToken(config.get("dd-backend"), ddBackend)) {
            throw std::runtime_error("--dd-backend expects " + std::string(ddBackendOptionSyntax()));
        }
    }
    config.set("dd-backend", ddBackend);
    config.set("knowledge", ddBackend);

    RewriteEngine rewriteEngine = RewriteEngine::OFF;
    if (isExplicitlySet(config, "rewrite-engine")) {
        if (!parseRewriteEngineToken(config.get("rewrite-engine"), rewriteEngine)) {
            throw std::runtime_error("--rewrite-engine expects " + std::string(rewriteEngineOptionSyntax()));
        }
    }
    config.set("rewrite-engine", rewriteEngineLabel(rewriteEngine));
    setConfigBool(config, "rewrite", rewriteEngine == RewriteEngine::LEGACY);
    setConfigBool(config, "implicit-rewrite", rewriteEngine == RewriteEngine::IMPLICIT ||
                                               rewriteEngine == RewriteEngine::IMPLICIT_ITER);
    setConfigBool(config, "implicit-iterate-split-rewrite", rewriteEngine == RewriteEngine::IMPLICIT_ITER);

    RewriteSplitMode splitMode = RewriteSplitMode::NAIVE;
    if (isExplicitlySet(config, "rewrite-split")) {
        if (!parseRewriteSplitModeToken(config.get("rewrite-split"), splitMode)) {
            throw std::runtime_error("--rewrite-split expects " + std::string(rewriteSplitOptionSyntax()));
        }
    }
    config.set("rewrite-split", rewriteSplitModeLabel(splitMode));
    switch (splitMode) {
        case RewriteSplitMode::OFF:
            config.set("split-mode", "no-split");
            break;
        case RewriteSplitMode::NAIVE:
            config.set("split-mode", "naive-split");
            break;
        case RewriteSplitMode::COMPLETE:
            config.set("split-mode", "complete-split");
            break;
    }

    RewriteDetectMode detectMode = RewriteDetectMode::DIRTY_FRONTIER;
    if (isExplicitlySet(config, "rewrite-detect")) {
        if (!parseRewriteDetectModeToken(config.get("rewrite-detect"), detectMode)) {
            throw std::runtime_error("--rewrite-detect expects " + std::string(rewriteDetectOptionSyntax()));
        }
    }
    config.set("rewrite-detect", rewriteDetectModeLabel(detectMode));
    setConfigBool(config, "force-complete-siso-detect", detectMode == RewriteDetectMode::COMPLETE);

    DetMode detMode = DetMode::AUTO;
    if (isExplicitlySet(config, "det-mode")) {
        if (!parseDetModeToken(config.get("det-mode"), detMode)) {
            throw std::runtime_error("--det-mode expects " + std::string(detModeOptionSyntax()));
        }
    }
    config.set("det-mode", detModeLabel(detMode));
    setConfigBool(config, "det-opt", detMode == DetMode::AUTO);
    setConfigBool(config, "no-det-opt", detMode == DetMode::OFF);
    setConfigBool(config, "det-force", detMode == DetMode::FORCE);

    applyListAliasToLegacyFlags(config, "dump",
            {{"json", "dumpjson"}, {"dot", "dumpdot"}, {"stat", "dumpstat"}, {"const", "dumpconst"}});
    applyListAliasToLegacyFlags(config, "profile-stage",
            {{"dred", "dred-profile"}, {"inc", "inc-profile"}, {"fc", "fc-profile"},
                    {"wmc", "profile-wmc"}, {"inc-delete", "profile-inc-delete"},
                    {"inc-regional", "profile-inc-regional"},
                    {"inc-regional-heavy", "profile-inc-regional-heavy"},
                    {"dep-graph", "profile-dep-graph"}});
}

}  // namespace

int main(Global& glb, const char* souffle_executable) {
    /* Time taking for overall runtime */
    auto souffle_start = std::chrono::high_resolution_clock::now();

    try {
        // Take in pragma options from the command line
        if (glb.config().has("pragma")) {
            ast::transform::PragmaChecker::Merger merger(glb);

            for (auto&& option : glb.config().getMany("pragma")) {
                // TODO: escape sequences for `:` to allow `:` in a pragma key?
                std::size_t splitPoint = option.find(':');

                std::string optionName = option.substr(0, splitPoint);
                std::string optionValue = (splitPoint == std::string::npos)
                                                  ? ""
                                                  : option.substr(splitPoint + 1, option.length());

                merger(optionName, optionValue);
            }
        }

        /* for the version option, if given print the version text then exit */
        if (glb.config().has("version")) {
            std::cout << versionFooter() << std::endl;
            return 0;
        }
        glb.config().set("version", packageVersion());

        /* for the help option, if given simply print the help text then exit */
        if (glb.config().has("help")) {
            std::cout << glb.config().help();
            return 0;
        }

        if (!glb.config().has("")) {
            std::cerr << "No datalog file specified.\n";
            return 0;
        }

        /* check that datalog program exists */
        if (!existFile(glb.config().get(""))) {
            throw std::runtime_error("cannot open file " + std::string(glb.config().get("")));
        }

        const bool hasCompileOption = glb.config().has("compile") || glb.config().has("compile-many") ||
                                      glb.config().has("generate") || glb.config().has("generate-many") ||
                                      glb.config().has("dl-program") || glb.config().has("swig");
        if (!hasCompileOption) {
            std::string defaultOutput = simpleName(glb.config().get(""));
            glb.config().set("dl-program", defaultOutput);
            std::cerr << "No compile option specified; defaulting to -o " << defaultOutput
                      << " (compile only)." << std::endl;
        }
        if (!glb.config().has("online")) {
            glb.config().set("online");
            std::cout << "Defaulting to --online" << std::endl;
        }
        canonicalizeForkRuntimeDefaults(glb.config());

        /* for the jobs option, to determine the number of threads used */
        if (isNumber(glb.config().get("jobs").c_str())) {
            int n = std::stoi(glb.config().get("jobs"));
            if (n < 1) {
                throw std::runtime_error("-j/--jobs may only be set to 'auto' or an integer greater than 0.");
            }
#ifndef _OPENMP
            if (n != 1 && !glb.config().has("no-warn")) {
                std::cerr << "\nThis installation of Souffle does not support concurrent jobs.\n";
            }
#endif
        } else {
            if (!glb.config().has("jobs", "auto")) {
                throw std::runtime_error("-j/--jobs may only be set to 'auto' or an integer greater than 0.");
            }
#ifdef _OPENMP
            // set jobs to zero to indicate the synthesiser to use the system default.
            glb.config().set("jobs", "0");
#else
            glb.config().set("jobs", "1");
#endif
        }

        /* if an output directory is given, check it exists */
        if (glb.config().has("output-dir") && !glb.config().has("output-dir", "-") &&
                !existDir(glb.config().get("output-dir")) &&
                !(glb.config().has("generate") ||
                        (glb.config().has("dl-program") && !glb.config().has("compile")))) {
            throw std::runtime_error(
                    "output directory " + glb.config().get("output-dir") + " does not exists");
        }

        /* verify all input directories exist (racey, but gives nicer error messages for common mistakes) */
        for (auto&& dir : glb.config().getMany("include-dir")) {
            if (!existDir(dir)) throw std::runtime_error("include directory `" + dir + "` does not exist");
        }

        /* collect all macro definitions for the pre-processor */
        if (glb.config().has("macro")) {
            std::string currentMacro = "";
            std::string allMacros = "";
            for (const char& ch : glb.config().get("macro")) {
                if (ch == ' ') {
                    allMacros += " -D";
                    allMacros += currentMacro;
                    currentMacro = "";
                } else {
                    currentMacro += ch;
                }
            }
            allMacros += " -D" + currentMacro;
            glb.config().set("macro", allMacros);
        }

        /* if emit-statistics is set then check that the profiler is also set */
        if (glb.config().has("emit-statistics")) {
            if (!glb.config().has("profile"))
                throw std::runtime_error("must be profiling to use emit-statistics");
        }

    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        exit(EXIT_FAILURE);
    }

    /**
     * Ensure that code generation is enabled if using SWIG interface option.
     */
    if (glb.config().has("swig") && !glb.config().has("generate")) {
        glb.config().set("generate", simpleName(glb.config().get("")));
    }

    // ------ start souffle -------------

    const std::string souffleExecutable = which(souffle_executable);

    if (souffleExecutable.empty()) {
        throw std::runtime_error("failed to determine souffle executable path");
    }

    const std::filesystem::path InputPath(glb.config().get(""));
    std::unique_ptr<InputProvider> Input;
    const bool use_preprocessor = !glb.config().has("no-preprocessor");
    if (use_preprocessor) {
        if (glb.config().has("preprocessor")) {
            auto cmd = glb.config().get("preprocessor");
            if (cmd == "gcc") {
                Input = std::make_unique<GCCPreprocInput>(InputPath, glb.config());
            } else if (cmd == "mcpp") {
                Input = std::make_unique<MCPPPreprocInput>(InputPath, glb.config());
            } else {
                Input = std::make_unique<PreprocInput>(InputPath, glb.config(), cmd);
            }
        } else if (MCPPPreprocInput::available()) {  // mcpp fallback
            Input = std::make_unique<MCPPPreprocInput>(InputPath, glb.config());
        } else if (GCCPreprocInput::available()) {  // gcc fallback
            Input = std::make_unique<GCCPreprocInput>(InputPath, glb.config());
        } else {
            throw std::runtime_error("failed to locate mcpp or gcc pre-processors");
        }
    } else {
        Input = std::make_unique<FileInput>(glb.config().get(""));
    }

    /* Time taking for parsing */
    auto parser_start = std::chrono::high_resolution_clock::now();

    // ------- parse program -------------

    // parse file
    ErrorReport errReport(process_warn_opts(glb));
    DebugReport debugReport(glb);
    std::string sourceBuffer;
    if (!Input->readInput(sourceBuffer)) {
        throw std::runtime_error("Failed to read input");
    }

    Own<ast::TranslationUnit> astTranslationUnit =
            ParserDriver::parseTranslationUnit(glb, InputPath.u8string(), sourceBuffer,
                    Input->reducedConsecutiveNonLeadingWhitespaces(), errReport, debugReport);

    /* Report run-time of the parser if verbose flag is set */
    if (glb.config().has("verbose")) {
        auto parser_end = std::chrono::high_resolution_clock::now();
        std::cout << "Parse time: " << std::chrono::duration<double>(parser_end - parser_start).count()
                  << "sec\n";
    }

    auto hasShowOpt = [&](auto&&... kind) { return (glb.config().has("show", kind) || ...); };

    // `--show parse-errors` is special in that it (ab?)used the return code to specify the # of errors.
    //  Other `--show` modes can be used in conjunction with each other.
    if (hasShowOpt("parse-errors")) {
        if (1 < glb.config().getMany("show").size()) {
            std::cerr << "WARNING: `--show parse-errors` inhibits other `--show` actions.\n";
        }

        std::cout << astTranslationUnit->getErrorReport();
        return static_cast<int>(astTranslationUnit->getErrorReport().getNumErrors());
    }

    // ------- check for parse errors -------------
    astTranslationUnit->getErrorReport().exitIfErrors();

    // ------- rewriting / optimizations -------------

    /* set up additional global options based on pragma declaratives */
    (mk<ast::transform::PragmaChecker>())->apply(*astTranslationUnit);
    if (hasShowOpt("initial-ast", "initial-datalog")) {
        std::cout << astTranslationUnit->getProgram() << std::endl;
        // no other show options specified -> bail, we're done.
        if (glb.config().getMany("show").size() == 1) return 0;
    }

    /* construct the transformation pipeline */
    auto pipeline = astTransformationPipeline(glb);

    // Disable unwanted transformations
    if (glb.config().has("disable-transformers")) {
        std::vector<std::string> givenTransformers =
                splitString(glb.config().get("disable-transformers"), ',');
        pipeline->disableTransformers(
                std::set<std::string>(givenTransformers.begin(), givenTransformers.end()));
    }

    // Set up the debug report if necessary
    if (glb.config().has("debug-report")) {
        auto parser_end = std::chrono::high_resolution_clock::now();
        std::stringstream ss;

        // Add current time
        std::time_t time = std::time(nullptr);
        ss << "Executed at ";
        ss << std::put_time(std::localtime(&time), "%F %T") << "\n";

        // Add config
        ss << "(\n";
        ss << join(glb.config().data(), ",\n", [](std::ostream& out, const auto& arg) {
            out << "  \"" << arg.first << "\" -> \"" << arg.second << '"';
        });
        ss << "\n)";

        debugReport.addSection("Configuration", "Configuration", ss.str());

        // Add parsing runtime
        std::string runtimeStr =
                "(" + std::to_string(std::chrono::duration<double>(parser_end - parser_start).count()) + "s)";
        debugReport.addSection("Parsing", "Parsing " + runtimeStr, "");

        pipeline->setDebugReport();
    }

    // Toggle pipeline verbosity
    pipeline->setVerbosity(glb.config().has("verbose"));

    // Apply all the transformations
    pipeline->apply(*astTranslationUnit);

    // Output the transformed datalog (support alias opt name of 'datalog')
    if (hasShowOpt("transformed-ast", "transformed-datalog")) {
        std::cout << astTranslationUnit->getProgram() << std::endl;
    }
    // TODO: if all grounded, then directly converted to derivation graph, skip semi-naive evaluation

    auto& newAstProgram = astTranslationUnit->getProgram();
    // Output the precedence graph in graphviz dot format
    if (hasShowOpt("precedence-graph")) {
        astTranslationUnit->getAnalysis<ast::analysis::PrecedenceGraphAnalysis>().printHTML(std::cout);
        std::cout << std::endl;
    }

    // Output the precedence graph in text format
    if (hasShowOpt("precedence-graph-text")) {
        astTranslationUnit->getAnalysis<ast::analysis::PrecedenceGraphAnalysis>().print(std::cout);
        std::cout << std::endl;
    }

    // Output the scc graph in graphviz dot format
    if (hasShowOpt("scc-graph")) {
        astTranslationUnit->getAnalysis<ast::analysis::SCCGraphAnalysis>().printHTML(std::cout);
        std::cout << std::endl;
    }

    // Output the scc graph in text format
    if (hasShowOpt("scc-graph-text")) {
        astTranslationUnit->getAnalysis<ast::analysis::SCCGraphAnalysis>().print(std::cout);
        std::cout << std::endl;
    }

    // Output the type analysis
    if (hasShowOpt("type-analysis")) {
        astTranslationUnit->getAnalysis<ast::analysis::TypeAnalysis>().print(std::cout);
        std::cout << std::endl;
    }

    // bail if we've nothing else left to show
    if (glb.config().has("show") && !hasShowOpt("initial-ram", "transformed-ram")) return 0;

//     if (groundness.allGroundRules) {
//         auto synthesiser = mk<synthesiser::GroundSynthesiser>(*astTranslationUnit);
//         const bool execute_mode = glb.config().has("compile") || glb.config().has("compile-many");
//         const bool compile_mode = glb.config().has("dl-program");
//         const bool generate_mode = glb.config().has("generate");
//         const bool generate_many_mode = glb.config().has("generate-many");

//         const bool must_interpret = !execute_mode && !compile_mode && !generate_mode && !generate_many_mode &&
//                                     !glb.config().has("swig");
//         const bool must_execute = execute_mode;
//         const bool must_compile = must_execute || compile_mode || glb.config().has("swig");
//         std::string baseFilename;
//         if (compile_mode) {
//             baseFilename = glb.config().get("dl-program");
//         } else if (generate_mode) {
//             baseFilename = glb.config().get("generate");

//             // trim .cpp extension if it exists
//             if (baseFilename.size() >= 4 && baseFilename.substr(baseFilename.size() - 4) == ".cpp") {
//                 baseFilename = baseFilename.substr(0, baseFilename.size() - 4);
//             }
//         } else if (generate_many_mode) {
//             baseFilename = glb.config().get("generate-many");
//         } else {
//             baseFilename = tempFile();
//         }

//         if (baseName(baseFilename) == "/" || baseName(baseFilename) == ".") {
//             baseFilename = tempFile();
//         }

//         std::string baseIdentifier = identifier(simpleName(baseFilename));

//         std::string binaryFilename = baseFilename;

//         auto synthesisStart = std::chrono::high_resolution_clock::now();
//         const bool emitToStdOut = glb.config().has("generate", "-");
//         const bool emitMultipleFiles =
//                 glb.config().has("generate-many") || glb.config().has("compile-many");

//         synthesiser::GenDb db;
//         synthesiser->generateCode(db, baseIdentifier);
//         std::vector<fs::path> srcFiles;

//         if (emitToStdOut) {
//             db.emitSingleFile(std::cout);
//         } else if (emitMultipleFiles) {
//             fs::path directory = glb.config().has("generate-many")
//                                          ? fs::path(glb.config().get("generate-many"))
//                                          : fs::temp_directory_path() / baseIdentifier;
//             std::string mainClass = db.emitMultipleFilesInDir(directory, srcFiles);
//             binaryFilename = (directory / fs::path(mainClass)).string();
//         } else {
//             {
//                 std::string sourceFilename = baseFilename + ".cpp";
//                 std::ofstream os{sourceFilename};
//                 db.emitSingleFile(os);
//                 os.close();
//                 srcFiles.push_back(fs::path(sourceFilename));
//             }
//         }


//         // Output relationId to relationStr mapping

//         if (glb.config().has("verbose")) {
//             auto synthesisEnd = std::chrono::high_resolution_clock::now();
//             std::cout << "Synthesis time: "
//                       << std::chrono::duration<double>(synthesisEnd - synthesisStart).count() << "sec\n";
//         }


//         if (must_compile) {
//             /* Fail if a souffle-compile executable is not found */
//             const auto souffle_compile = findTool("souffle-compile.py", souffleExecutable, ".");
//             if (!souffle_compile) throw std::runtime_error("failed to locate souffle-compile.py");

//             auto t_bgn = std::chrono::high_resolution_clock::now();
//             fs::path output(binaryFilename);
//             compileToBinary(glb, *souffle_compile, srcFiles, output);
//             auto t_end = std::chrono::high_resolution_clock::now();

//             if (glb.config().has("verbose")) {
//                 std::cout << "Compilation time: " << std::chrono::duration<double>(t_end - t_bgn).count()
//                           << "sec\n";
//             }
//         }

//         // run compiled C++ program if requested.
//         if (must_execute) {
// #if defined(_MSC_VER)
//             binaryFilename += ".exe";
// #endif
//             executeBinaryAndExit(glb, binaryFilename);
//         }
//     }
//     else
        {
        // ------- execution -------------
        /* translate AST to RAM */
        debugReport.startSection();
        auto unitTranslator = getUnitTranslator(glb);
        auto ramTranslationUnit = unitTranslator->translateUnit(*astTranslationUnit);
        debugReport.endSection("ast-to-ram", "Translate AST to RAM");

        if (hasShowOpt("initial-ram")) {
            std::cout << ramTranslationUnit->getProgram();
            // bail if we've nothing else left to show
            if (!hasShowOpt("transformed-ram")) return 0;
        }

        // Apply RAM transforms
        if (!glb.config().has("no-souffle-opt")) {
            auto ramTransform = ramTransformerSequence(glb);
            ramTransform->apply(*ramTranslationUnit);
        }

        if (ramTranslationUnit->getErrorReport().getNumIssues() != 0) {
            std::cerr << ramTranslationUnit->getErrorReport();
        }

        // Output the transformed RAM program and return
        if (hasShowOpt("transformed-ram")) {
            std::cout << ramTranslationUnit->getProgram();
            return 0;
        }

        const bool execute_mode = glb.config().has("compile") || glb.config().has("compile-many");
        const bool compile_mode = glb.config().has("dl-program");
        const bool generate_mode = glb.config().has("generate");
        const bool generate_many_mode = glb.config().has("generate-many");

        const bool must_execute = execute_mode;
        const bool must_compile = must_execute || compile_mode || glb.config().has("swig");

        try {
            // ------- compiler -------------
            auto synthesiser = mk<synthesiser::Synthesiser>(*ramTranslationUnit);
            // synthesiser->setInitProgram(*initAstProgramPtr);
            synthesiser->setNewProgram(newAstProgram);
            // Find the base filename for code generation and execution
            std::string baseFilename;
            if (compile_mode) {
                baseFilename = glb.config().get("dl-program");
            } else if (generate_mode) {
                baseFilename = glb.config().get("generate");

                // trim .cpp extension if it exists
                if (baseFilename.size() >= 4 && baseFilename.substr(baseFilename.size() - 4) == ".cpp") {
                    baseFilename = baseFilename.substr(0, baseFilename.size() - 4);
                }
            } else if (generate_many_mode) {
                baseFilename = glb.config().get("generate-many");
            } else {
                baseFilename = tempFile();
            }

            if (baseName(baseFilename) == "/" || baseName(baseFilename) == ".") {
                baseFilename = tempFile();
            }

            std::string baseIdentifier = identifier(simpleName(baseFilename));

            std::string binaryFilename = baseFilename;

            bool withSharedLibrary;
            auto synthesisStart = std::chrono::high_resolution_clock::now();
            const bool emitToStdOut = glb.config().has("generate", "-");
            const bool emitMultipleFiles =
                    glb.config().has("generate-many") || glb.config().has("compile-many");

            synthesiser::GenDb db;
            synthesiser->generateCode(db, baseIdentifier, withSharedLibrary);
            std::vector<fs::path> srcFiles;

            if (emitToStdOut) {
                db.emitSingleFile(std::cout);
            } else if (emitMultipleFiles) {
                fs::path directory = glb.config().has("generate-many")
                                             ? fs::path(glb.config().get("generate-many"))
                                             : fs::temp_directory_path() / baseIdentifier;
                std::string mainClass = db.emitMultipleFilesInDir(directory, srcFiles);
                binaryFilename = (directory / fs::path(mainClass)).string();
            } else {
                {
                    std::string sourceFilename = baseFilename + ".cpp";
                    std::ofstream os{sourceFilename};
                    db.emitSingleFile(os);
                    os.close();
                    srcFiles.push_back(fs::path(sourceFilename));
                }
                {
                    // TODO emit TranslationContext.clauseNums here
                    std::string clauseMapFilename = baseFilename + "-clauses.txt";
                    std::ofstream os{clauseMapFilename};
                    unitTranslator->context->dumpClauseNums(os);
                    os.close();
                }
            }


            // Output relationId to relationStr mapping

            if (glb.config().has("verbose")) {
                auto synthesisEnd = std::chrono::high_resolution_clock::now();
                std::cout << "Synthesis time: "
                          << std::chrono::duration<double>(synthesisEnd - synthesisStart).count() << "sec\n";
            }

            if (withSharedLibrary) {
                if (!glb.config().has("libraries")) {
                    glb.config().set("libraries", "functors");
                }
                if (!glb.config().has("library-dir")) {
                    glb.config().set("library-dir", ".");
                }
            }

            if (must_compile) {
                /* Fail if a souffle-compile executable is not found */
                const auto souffle_compile = findTool("souffle-compile.py", souffleExecutable, ".");
                if (!souffle_compile) throw std::runtime_error("failed to locate souffle-compile.py");

                auto t_bgn = std::chrono::high_resolution_clock::now();
                fs::path output(binaryFilename);
                compileToBinary(glb, *souffle_compile, srcFiles, output);
                auto t_end = std::chrono::high_resolution_clock::now();

                if (glb.config().has("verbose")) {
                    std::cout << "Compilation time: " << std::chrono::duration<double>(t_end - t_bgn).count()
                              << "sec\n";
                }
            }

            // run compiled C++ program if requested.
            if (must_execute) {
#if defined(_MSC_VER)
                binaryFilename += ".exe";
#endif
                executeBinaryAndExit(glb, binaryFilename);
            }
        } catch (std::exception& e) {
            std::cerr << e.what() << std::endl;
            std::exit(EXIT_FAILURE);
        }
    }

    /* Report overall run-time in verbose mode */
    if (glb.config().has("verbose")) {
        auto souffle_end = std::chrono::high_resolution_clock::now();
        std::cout << "Total time: " << std::chrono::duration<double>(souffle_end - souffle_start).count()
                  << "sec\n";
    }

    return 0;
}

}  // end of namespace souffle
