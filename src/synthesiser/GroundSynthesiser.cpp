#include "synthesiser/Synthesiser.h"
#include "ast/TranslationUnit.h"
#include "ast/Program.h"
#include "ast/Clause.h"
#include "ast/Atom.h"
#include "ast/Constant.h"
#include "ast/utility/Visitor.h"
#include "ast/utility/Utils.h"
#include "reports/ErrorReport.h"
#include "souffle/utility/StringUtil.h"
#include "souffle/utility/MiscUtil.h"
#include "synthesiser/GenDb.h"
#include "synthesiser/Utils.h"
#include <fstream>
#include "souffle/problog/PreDerivationGraph.h"
namespace souffle::synthesiser {

GroundSynthesiser::GroundSynthesiser(ast::TranslationUnit& tu) : translationUnit(tu), glb(tu.global()),
groundnessInfo(tu.getProgram().getGroundnessInfo()) {}

std::size_t GroundSynthesiser::getSymbolIndex(const std::string& symbol) const {
    auto it = symbolMap.find(symbol);
    if (it != symbolMap.end()) {
        return it->second;
    }
    std::size_t idx = symbolMap.size();
    symbolMap[symbol] = idx;
    symbolIndex.push_back(symbol);
    return idx;
}

RamDomain GroundSynthesiser::convertConstant(const ast::Constant& constant) const {
    if (auto* num = dynamic_cast<const ast::NumericConstant*>(&constant)) {
        return RamDomain(std::stoll(num->getConstant()));
    }
    if (auto* str = dynamic_cast<const ast::StringConstant*>(&constant)) {
        return RamDomain(getSymbolIndex(str->getConstant()));
    }
    assert (false);
    return 0; // Should not be reached in a ground program
}

void GroundSynthesiser::generateCode(GenDb& db, const std::string& id) {
    ast::Program& program = translationUnit.getProgram();
    const auto& groundInfo = program.getGroundnessInfo();

    assert(groundInfo.allGroundRules && "GroundSynthesiser requires a fully ground program.");

    // --- Class Setup ---
    std::string className = "Sf_" + id;
    GenClass& mainClass = db.getClass(className, fs::path(className + ".h"));
    // mainClass.inherits("public SouffleProgramGround");
    mainClass.addInclude("\"souffle/CompiledSouffle.h\"");
    mainClass.addInclude("\"souffle/io/IOSystem.h\"");
    mainClass.addInclude("\"souffle/problog/PreDerivationGraph.h\"");
    mainClass.addInclude("\"souffle/problog/DerivationGraph.h\"");
    mainClass.addInclude("\"souffle/problog/ForwardCompilation.h\"");
    mainClass.addInclude("\"souffle/problog/debug/Debugger.h\"");
    mainClass.addInclude("<cassert>");
    mainClass.addInclude("<unordered_map>");
    mainClass.addInclude("<vector>");
    mainClass.addInclude("<fstream>");
    mainClass.addInclude("<string>");

    if (glb.config().has("generate-namespace")) {
        db.setNS(glb.config().get("generate-namespace"));
    } else {
        db.setNS("souffle");
    }

    // --- Symbol Table ---
    visit(program, [&](const ast::StringConstant& str) { getSymbolIndex(str.getConstant()); });

    // --- Field Declarations ---
    mainClass.addField("SymbolTableImpl", "symTable", Visibility::Private);
    mainClass.addField("SpecializedRecordTable<0>", "recordTable", Visibility::Private);
    mainClass.addField("PreDerivationGraph", "preDG", Visibility::Public);
    mainClass.addField("std::map<EdgeId, double>", "edgeProbabilities", Visibility::Public);
    mainClass.addField("std::map<NodeId, double>", "factProbabilities", Visibility::Public);

    auto& getSymbTable = mainClass.addFunction("getSymbolTable", Visibility::Public);
    getSymbTable.setRetType("SymbolTable&");
    getSymbTable.body() << "return symTable;\n";

    auto& getRecordTable = mainClass.addFunction("getRecordTable", Visibility::Public);
    getRecordTable.setRetType("RecordTable&");
    getRecordTable.body() << "return recordTable;\n";

    // --- Constructor ---
    GenFunction& ctor = mainClass.addConstructor(Visibility::Public);
    ctor.setIsConstructor();
    ctor.setNextInitializer("symTable", "SymbolTableImpl()");
    ctor.setNextInitializer("recordTable", "SpecializedRecordTable<0>()");
    ctor.setNextInitializer("preDG", "PreDerivationGraph()");

    // when initializing symbol table, the index of each symbol will be automatically assigned by its position
    if (!symbolIndex.empty()) {
        std::stringstream ss;
        ss << "{\n";
        for (const auto& sym : symbolIndex) {
            ss << "    \"" << souffle::stringify(sym) << "\",\n";
        }
        ss << "}";
        ctor.body() << "symTable.insertAll(" << ss.str() << ");\n";
    }


    ctor.body() << "\n// ---- Statically build the PreDerivationGraph ----\n";
    ctor.body() << "std::unordered_map<AtomKey, NodeId, AtomKeyHash> atomToNodeId;\n";

    auto getOrAddNodeInCtor = [&](const ast::Atom* atom, const double prob = 1.0) -> std::string {
        AtomKey key;
        key.rel = atom->getQualifiedName().toString();
        for (const auto* arg : atom->getArguments()) {
            assert(isA<ast::Constant>(arg) && "Atom argument is not a constant.");
            key.args.push_back(convertConstant(*dynamic_cast<const ast::Constant*>(arg)));
        }

        std::string keyString = "AtomKey{{\"" + key.rel + "\"}, {";
        for (size_t i = 0; i < key.args.size(); ++i) {
            keyString += std::to_string(key.args[i]) + (i == key.args.size() - 1 ? "" : ", ");
        }
        keyString += "}}";

        ctor.body() << "if (atomToNodeId.find(" << keyString << ") == atomToNodeId.end()) {\n";
        ctor.body() << "    NodeId nodeId = preDG.addNode(" << keyString << "," << prob << ");\n";
        ctor.body() << "    atomToNodeId[" << keyString << "] = nodeId;\n";
        ctor.body() << "}\n";

        return "atomToNodeId.at(" + keyString + ")";
    };

    // Process facts
    // TODO: feel like I should unite AtomKey and UntypedTuple
    for (const auto* fact : groundInfo.facts) {
        const auto* head = fact->getHead();
        std::string headNodeIdStr = getOrAddNodeInCtor(head, fact->getProbability());
        ctor.body() << "preDG.seedFacts({" << headNodeIdStr << "}, PreDerivationGraph::SeedMode::Accumulate);\n";
        ctor.body() << "factProbabilities[" << headNodeIdStr << "] = " << fact->getProbability() << ";\n";
        // TODO: use fact_prob uniformly
    }

    // Process rules
    EdgeId edgeIdCounter = 0;
    for (const auto* rule : groundInfo.groundClauses) {
        const auto* head = rule->getHead();
        std::string headNodeIdStr = getOrAddNodeInCtor(head);
        if (rule->getBodyLiterals().empty()) {
            // a fact
            ctor.body() << "preDG.seedFacts({" << headNodeIdStr << "}, PreDerivationGraph::SeedMode::Accumulate);\n";
            ctor.body() << "factProbabilities[" << headNodeIdStr << "] = " << rule->getProbability() << ";\n";
            continue;
        }
        std::vector<std::string> bodyNodeIdStrs;
        std::vector<bool> negs;
        for (const auto* literal : rule->getBodyLiterals()) {
            if (const auto* atom = dynamic_cast<const ast::Atom*>(literal)) {
                bodyNodeIdStrs.push_back(getOrAddNodeInCtor(atom));
                negs.push_back(false);
            } else if (const auto* neg = dynamic_cast<const ast::Negation*>(literal)) {
                assert(isA<ast::Atom>(neg->getAtom()) && "Negated literal is not an atom.");
                bodyNodeIdStrs.push_back(getOrAddNodeInCtor(dynamic_cast<const ast::Atom*>(neg->getAtom())));
                negs.push_back(true);
            } else {
                // should be constraints, omit
                // assert(false && "Body literal is neither an atom nor a negation.");
            }
        }
        ctor.body() << "edgeProbabilities[" << edgeIdCounter << "] = " << rule->getProbability() << ";\n";
        ctor.body() << "preDG.addEdge({";
        for (size_t i = 0; i < bodyNodeIdStrs.size(); ++i) {
            ctor.body() << bodyNodeIdStrs[i] << (i == bodyNodeIdStrs.size() - 1 ? "" : ", ");
        }
        ctor.body() << "}, " << headNodeIdStr
        << ", "
        // prob
        << rule->getProbability() << ", "
        // negations
        << "{";
        for (size_t i = 0; i < negs.size(); ++i) {
            ctor.body() << (negs[i] ? "true" : "false") << (i == negs.size() - 1 ? "" : ", ");
        }
        ctor.body() << "}" << ");\n";
        edgeIdCounter++;
    }
    ctor.body() << "preDG.toDot(\"preDG.dot\");\n";

    // --- runFunction ---
    GenFunction& runFunc = mainClass.addFunction("run", Visibility::Public);
    runFunc.setRetType("void");

    runFunc.body() << "Debugger::getInstance().startTurn(\"FULL\");\n\n";

    if (!groundInfo.inputRelationNames.empty()) {
        runFunc.body() << "Debugger::getInstance().startStage(StageKind::IO_LOAD_FULL);\n";


        for (const auto& relName : groundInfo.inputRelationNames) {
            // change to manually read from .fact and .prob files
            // first get the outputFileFolder
            runFunc.body() << "assert(false && \"not supported yet\");\n";
            // runFunc.body() << "if (auto* rel = getRelation(\"" << relName.toString() << "\")) {\n";
            // runFunc.body() << "    std::vector<double> probs;\n";
            // runFunc.body() << "    std::string probFilePath = Global::config().get(\"fact-dir\") + \"/" << relName.toString() << ".prob\";\n";
            // runFunc.body() << "    std::ifstream probFile(probFilePath);\n";
            // runFunc.body() << "    if (probFile.is_open()) {\n";
            // runFunc.body() << "        double p;\n";
            // runFunc.body() << "        while (probFile >> p) { probs.push_back(p); }\n";
            // runFunc.body() << "    }\n\n";
            //
            // runFunc.body() << "    size_t factIndex = 0;\n";
            // runFunc.body() << "    for (const auto& tuple : *rel) {\n";
            // runFunc.body() << "        AtomKey key; \n";
            // runFunc.body() << "        key.rel = \"" << relName.toString() << "\";\n";
            // runFunc.body() << "        for (size_t i = 0; i < rel->getArity(); ++i) {\n";
            // runFunc.body() << "            key.args.push_back(tuple[i]);\n";
            // runFunc.body() << "        }\n";
            // runFunc.body() << "        NodeId nodeId = preDG.getOrAddNode(key);\n";
            // runFunc.body() << "        preDG.seedFacts({nodeId}, PreDerivationGraph::SeedMode::Accumulate);\n";
            // runFunc.body() << "        if (factIndex < probs.size()) {\n";
            // runFunc.body() << "            factProbabilities[nodeId] = probs[factIndex];\n";
            // runFunc.body() << "        }\n";
            // runFunc.body() << "        factIndex++;\n";
            // runFunc.body() << "    }\n";
            // runFunc.body() << "}\n";
            runFunc.body() << "reading input relation " << relName.toString() << "...\n";
        }
        runFunc.body() << "Debugger::getInstance().endStage();\n\n";
    }



    // --- Standard Public Methods ---
    GenFunction& runAll = mainClass.addFunction("runAll", Visibility::Public);
    runAll.setRetType("void");
    runAll.body() << "run();\n";

    std::ostream& hook = mainClass.hooks();

    hook << "\n#ifndef __EMBEDDED_SOUFFLE__\n";
    hook << "#include \"souffle/CompiledOptions.h\"\n";

    hook << "int main(int argc, char** argv)\n{\n";
    hook << "try{\n";

    // parse arguments
    hook << "souffle::CmdOptions opt(";
    hook << "R\"(" << glb.config().get("") << ")\",\n";
    if (glb.config().has("fact-dir")) {
        hook << "R\"(" << glb.config().get("fact-dir") << ")\",\n";
    } else {
        hook << "R\"()\",\n";
    }
    if (glb.config().has("output-dir")) {
        hook << "R\"(" << glb.config().get("output-dir") << ")\",\n";
    } else {
        hook << "R\"()\",\n";
    }
    if (glb.config().has("profile")) {
        hook << "true,\n";
        hook << "R\"(" << glb.config().get("profile") << ")\",\n";
    } else {
        hook << "false,\n";
        hook << "R\"()\",\n";
    }
    hook << std::stoi(glb.config().get("jobs"));
    hook << ", \"log.txt\"";
    hook << ", " << (glb.config().has("derv-only") ? "true" : "false");
    hook << ",\"" << glb.config().get("setmode") << "\"";
    hook << "," << (glb.config().has("merge-bi-imp") ? "true" : "false");
    hook << "," << (glb.config().has("rewrite") ? "true" : "false");
    hook << ");\n";

    hook << "if (!opt.parse(argc,argv)) return 1;\n";

    if (!db.getNS(false).empty()) {
        hook << db.getNS(false) << "::";
    }

    hook << className + " obj;\n";

    // TODO set knowledge representation

    // hook << "obj.runAll(opt.getInputFileDir(), opt.getOutputFileDir());\n";
    hook << "obj.runAll();\n";
    hook << "auto& preDG = obj.preDG;\n";
    hook << "debugger.startTurn();\n";
    hook << "try {\n";
    hook << "debugger.startStage(StageKind::IO_LOAD_FULL);\n";
    hook << "{\n";
    hook << "FunctionTimer timer(\"Reading fact probability from \" + opt.getInputFileDir());\n";
    for (auto input : groundInfo.inputRelationNames) {
        auto rel = input.toString();
        hook << "{\n";
        hook << "std::string rel = \"" << rel << "\";\n";
        hook << "std::cout << \"reading: \" << opt.getInputFileDir() << \"/\" << rel << \".facts and \" << opt.getInputFileDir() << \"/\" << rel << \".prob\" << std::endl;\n";
        hook << "std::ifstream factFile(opt.getInputFileDir() + \"/\" + rel + \".facts\");";
        hook << "std::ifstream probFile(opt.getInputFileDir() + \"/\" + rel + \".prob\");";
        hook << "std::string factLine, probLine;";
        hook << "while (std::getline(factFile, factLine) && std::getline(probFile, probLine)) {";
        hook << "std::istringstream fs(factLine);";
        hook << "std::istringstream ps(probLine);";
        hook << "double prob; ps >> prob;\n";
        hook << "assert (prob >= 0 && prob <= 1);\n";
        hook << "souffle::RamDomain field;\n";
        hook << "std::vector<souffle::RamDomain> fields;\n";
        hook << "while (fs >> field) {fields.push_back(field);}\n";
        hook << "UntypedTuple tuple{rel, fields};\n";
        hook << "fact_prob[tuple] = prob;\n";
        hook << "initialInputRelations[\"" << rel << "\"].insert(tuple);";
        hook << "}\n";
        hook << "}\n";
    }
    hook << "}\n";
    hook << "debugger.endStage();\n";
    db.addGlobalInclude("\"souffle/problog/Atom.h\"");
    db.addGlobalInclude("\"souffle/problog/Rule.h\"");
    db.addGlobalInclude("\"souffle/problog/RuleManager.h\"");
    db.addGlobalInclude("\"souffle/problog/formula/CuddManager.h\"");
    db.addGlobalInclude("\"souffle/problog/formula/SddManager.h\"");
    db.addGlobalInclude("\"souffle/problog/ForwardCompilation.h\"");
    db.addGlobalInclude("\"souffle/problog/debug/Debugger.h\"");
    db.addGlobalInclude("\"souffle/problog/Pipeline.h\"");
    if (glb.config().has("online")) {
        db.addGlobalInclude("\"souffle/cli/Cli.h\"");
    }

    // synthesize rules
    // TODO: should make this pure static?
    // hook << "fullComp(dg, opt, {";
    hook << "fullCompPlusIncForGround(preDG, opt, {";
    for (size_t i = 0; i < groundInfo.outputRelationNames.size(); ++i) {
        hook << "\"" << groundInfo.outputRelationNames[i].toString() << "\"";
        if (i != groundInfo.outputRelationNames.size() - 1) {
            hook << ", ";
        }
    }
    hook << "});\n";



    // // TODO: add online incremental&interactive computation
    // if (glb.config().has("online")) {
    //     hook << "IncrementalCLI cli(&obj, graph, &ruleManager, &bddManager, &nodeFormulas, &edgeFormulas, true, &preDG);\n";
    //     hook << "cli.setCmdOptions(opt);\n";
    //     hook << "cli.run();\n";
    // }

    hook << "return 0;\n";
    hook << "} catch (std::exception& e) {std::cerr << \"Problog calc failed\" << e.what() << std::endl;}\n";
    hook << "} catch(std::exception &e) { souffle::SignalHandler::instance()->error(e.what());}\n";
    hook << "}\n";
    hook << "#endif\n";

}

}  // namespace souffle::synthesiser
