// Debugger.h
#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <optional>
#include "souffle/utility/json11.h" // Replace nlohmann with json11

class Debugger {
public:
    enum class Level { INFO, WARNING, ERROR, DEBUG };

    struct IterationInfo {
        int iteration;
        double duration_sec;
        size_t memory_kb;
        size_t bdd_node_count;
        std::optional<double> gc_time;
        std::optional<double> reordering_time;
    };

    struct MetaInfo {
        std::chrono::steady_clock::time_point program_start;
        std::chrono::steady_clock::time_point program_end;
        size_t peak_memory_kb = 0;
        std::unordered_map<std::string, std::string> config;

        double getDurationInSeconds() const {
            return std::chrono::duration<double>(program_end - program_start).count();
        }
    };

    struct StageInfo {
        std::string name;
        std::chrono::steady_clock::time_point start_time;
        std::chrono::steady_clock::time_point end_time;
        size_t peak_memory_kb = 0;
        size_t bdd_node_count = 0;
        std::vector<std::pair<Level, std::string>> logs;
        std::vector<IterationInfo> iterations;

        double getDurationInSeconds() const {
            return std::chrono::duration<double>(end_time - start_time).count();
        }
    };

    void startStage(const std::string& stageName);
    void endStage(const std::string& stageName, size_t bdd_nodes = 0);
    void log(Level level, const std::string& stageName, const std::string& message);
    void addIteration(const std::string& stageName, const IterationInfo& iter);
    void finalize();
    json11::Json toJson(const std::string& path = "");

    // meta-info API
    void markProgramStart();
    void markProgramEnd();
    void setMetaConfig(const std::string& key, const std::string& value);

private:
    std::unordered_map<std::string, StageInfo> stages;
    MetaInfo meta;

    size_t getCurrentMemoryKB() const;
    std::string levelToString(Level level) const;
};

// Debugger.cpp
#include "Debugger.h"

void Debugger::startStage(const std::string& name) {
    stages[name] = StageInfo{name, std::chrono::steady_clock::now()};
}

void Debugger::endStage(const std::string& name, size_t bdd_nodes) {
    auto& stage = stages.at(name);
    stage.end_time = std::chrono::steady_clock::now();
    stage.peak_memory_kb = getCurrentMemoryKB();
    stage.bdd_node_count = bdd_nodes;
}

void Debugger::log(Level level, const std::string& stageName, const std::string& message) {
    stages[stageName].logs.emplace_back(level, message);
}

void Debugger::addIteration(const std::string& stageName, const IterationInfo& iter) {
    stages[stageName].iterations.push_back(iter);
}

void Debugger::markProgramStart() {
    meta.program_start = std::chrono::steady_clock::now();
}

void Debugger::markProgramEnd() {
    meta.program_end = std::chrono::steady_clock::now();
    meta.peak_memory_kb = getCurrentMemoryKB();
}

void Debugger::setMetaConfig(const std::string& key, const std::string& value) {
    meta.config[key] = value;
}

size_t Debugger::getCurrentMemoryKB() const {
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        if (line.substr(0, 6) == "VmRSS:") {
            std::istringstream iss(line);
            std::string key, unit;
            size_t value;
            iss >> key >> value >> unit;
            return value;
        }
    }
    return 0;
}

std::string Debugger::levelToString(Level level) const {
    switch (level) {
        case Level::INFO: return "INFO";
        case Level::WARNING: return "WARNING";
        case Level::ERROR: return "ERROR";
        case Level::DEBUG: return "DEBUG";
        default: return "UNKNOWN";
    }
}

void Debugger::finalize() {
    std::cout << "========== Debug Summary ==========" << std::endl;
    std::cout << "[Meta] Duration: " << std::fixed << std::setprecision(3) << meta.getDurationInSeconds()
              << "s, Peak Mem: " << meta.peak_memory_kb << " KB" << std::endl;
    for (const auto& [k, v] : meta.config) {
        std::cout << "  Config: " << k << " = " << v << std::endl;
    }
    for (const auto& [name, stage] : stages) {
        std::cout << "[Stage] " << name << "\n"
                  << "  Duration: " << std::fixed << std::setprecision(3) << stage.getDurationInSeconds() << "s\n"
                  << "  Peak Memory: " << stage.peak_memory_kb << " KB\n"
                  << "  BDD Nodes: " << stage.bdd_node_count << "\n";
        for (const auto& iter : stage.iterations) {
            std::cout << "  Iteration " << iter.iteration << ": " << iter.duration_sec << "s, Mem: " << iter.memory_kb << "KB, BDD: " << iter.bdd_node_count;
            if (iter.gc_time) std::cout << ", GC: " << *iter.gc_time << "s";
            if (iter.reordering_time) std::cout << ", Reorder: " << *iter.reordering_time << "s";
            std::cout << std::endl;
        }
        std::cout << "  Logs:" << std::endl;
        for (const auto& [lvl, msg] : stage.logs) {
            std::cout << "    [" << levelToString(lvl) << "] " << msg << std::endl;
        }
    }
    std::cout << "===================================" << std::endl;
}

json11::Json Debugger::toJson(const std::string& path) {
    using namespace json11;
    Json::object j;

    // meta info
    Json::object meta_j;
    meta_j["duration"] = meta.getDurationInSeconds();
    meta_j["peak_memory_kb"] = static_cast<double>(meta.peak_memory_kb);
    Json::object config_j;
    for (const auto& [k, v] : meta.config) {
        config_j[k] = v;
    }
    meta_j["config"] = config_j;
    j["meta"] = meta_j;

    // stages
    Json::object stage_j;
    for (const auto& [name, stage] : stages) {
        Json::object s;
        s["duration"] = stage.getDurationInSeconds();
        s["peak_memory_kb"] = static_cast<double>(stage.peak_memory_kb);
        s["bdd_nodes"] = static_cast<double>(stage.bdd_node_count);

        Json::array logs;
        for (const auto& [lvl, msg] : stage.logs) {
            logs.push_back(Json::object{{"level", levelToString(lvl)}, {"msg", msg}});
        }
        s["logs"] = logs;

        Json::array iters;
        for (const auto& iter : stage.iterations) {
            Json::object one = {
                {"iteration", iter.iteration},
                {"duration", iter.duration_sec},
                {"memory_kb", static_cast<double>(iter.memory_kb)},
                {"bdd_nodes", static_cast<double>(iter.bdd_node_count)}
            };
            if (iter.gc_time) one["gc_time"] = *iter.gc_time;
            if (iter.reordering_time) one["reordering_time"] = *iter.reordering_time;
            iters.push_back(one);
        }
        s["iterations"] = iters;

        stage_j[name] = s;
    }
    j["stages"] = stage_j;

    Json result = j;
    if (!path.empty()) {
        std::ofstream out(path);
        out << result.dump() << std::endl;
    }
    return result;
}

// RAII helper
class DebuggerFinalizer {
    Debugger& dbg;
    std::string jsonPath;
public:
    DebuggerFinalizer(Debugger& d, const std::string& path = "debug_output.json") : dbg(d), jsonPath(path) {
        dbg.markProgramStart();
    }
    ~DebuggerFinalizer() {
        dbg.markProgramEnd();
        dbg.finalize();
        dbg.toJson(jsonPath);
    }
};