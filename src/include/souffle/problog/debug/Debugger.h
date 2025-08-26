#ifndef DEBUGGER_HPP
#define DEBUGGER_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <iostream>
#include <fstream>
#include <sstream>
#include <utility>
#include <cassert>
class Debugger;
enum class Level { INFO, WARNING, ERROR, DEBUG };

inline std::string levelToString(Level level) {
    switch (level) {
        case Level::INFO: return "INFO";
        case Level::WARNING: return "WARNING";
        case Level::ERROR: return "ERROR";
        case Level::DEBUG: return "DEBUG";
        default: return "UNKNOWN";
    }
}

enum class StageKind {
    IO_LOAD_FULL,
    CONSTRUCT_RULE_FULL,
    SEMINAIVE_FULL,
    CREATE_GRAPH_FULL,
    PRUNING_FULL,
    PRECONFIG_FULL,
    FORWARD_COMPILATION_FULL,
    WEIGHTED_MODEL_COUNTING_FULL,
    IO_DUMP_FULL,
    SEMINAIVE_INC,
    CREATE_GRAPH_INC,
    PRUNING_INC,
    PRECONFIG_INC,
    FORWARD_COMPILATION_INC,
    WEIGHTED_MODEL_COUNTING_INC
};

inline std::string stageKindToString(const StageKind kind) {
    switch (kind) {
        case StageKind::IO_LOAD_FULL: return "IO_LOAD_FULL";
        case StageKind::CONSTRUCT_RULE_FULL: return "CONSTRUCT_RULE_FULL";
        case StageKind::SEMINAIVE_FULL: return "SEMINAIVE_FULL";
        case StageKind::CREATE_GRAPH_FULL: return "CREATE_GRAPH_FULL";
        case StageKind::PRUNING_FULL: return "PRUNING_FULL";
        case StageKind::PRECONFIG_FULL: return "PRECONFIG_FULL";
        case StageKind::FORWARD_COMPILATION_FULL: return "FORWARD_COMPILATION_FULL";
        case StageKind::WEIGHTED_MODEL_COUNTING_FULL: return "WEIGHTED_MODEL_COUNTING_FULL";
        case StageKind::IO_DUMP_FULL: return "IO_DUMP_FULL";
        case StageKind::SEMINAIVE_INC: return "SEMINAIVE_INC";
        case StageKind::CREATE_GRAPH_INC: return "CREATE_GRAPH_INC";
        case StageKind::PRUNING_INC: return "PRUNING_INC";
        case StageKind::PRECONFIG_INC: return "PRECONFIG_INC";
        case StageKind::FORWARD_COMPILATION_INC: return "FORWARD_COMPILATION_INC";
        case StageKind::WEIGHTED_MODEL_COUNTING_INC: return "WEIGHTED_MODEL_COUNTING_INC";
        default: return "UNKNOWN";
    }
}

class Info {
protected:
    std::unordered_map<std::string, std::string> infoMap_;
    std::unordered_map<Level, std::vector<std::string>> logs_;
    size_t memStart_ = 0;
    size_t memEnd_ = 0;
    size_t memPeak_ = 0;

    std::chrono::steady_clock::time_point startTime_;
    std::chrono::steady_clock::time_point endTime_;

public:
    void setInfo(const std::string& key, const std::string& value) {
        infoMap_[key] = value;
    }

    std::string getInfo(const std::string& key) const {
        const auto it = infoMap_.find(key);
        return it != infoMap_.end() ? it->second : "";
    }

    void logMessage(const Level level, const std::string& message) {
        logs_[level].push_back(message);
    }

    void setMemStart(size_t m) { memStart_ = m; }
    size_t getMemStart() const { return memStart_; }

    void setMemEnd(size_t m) { memEnd_ = m; }
    size_t getMemEnd() const { return memEnd_; }

    void setMemPeak(size_t m) { memPeak_ = m; }
    size_t getMemPeak() const { return memPeak_; }

    void markStartTime() { startTime_ = std::chrono::steady_clock::now(); }
    void markEndTime() { endTime_ = std::chrono::steady_clock::now(); }

    double getDurationSeconds() const {
        return std::chrono::duration<double>(endTime_ - startTime_).count();
    }

    const std::unordered_map<std::string, std::string>& getInfoMap() const { return infoMap_; }
    const std::unordered_map<Level, std::vector<std::string>>& getLogs() const { return logs_; }

    virtual ~Info() = default;
};

class IterationInfo : public Info {
public:
    int iterationIndex;
    explicit IterationInfo(const int idx) : iterationIndex(idx) {}
};

class StageInfo : public Info {
public:
    StageKind kind;
    std::vector<IterationInfo> iterations;

    explicit StageInfo(const StageKind k)
        : kind(k) {}
};

class TurnInfo : public Info {
public:
    int turnIndex;
    std::string algMode;
    size_t inputSize = 0;
    std::vector<StageInfo> stages;

    TurnInfo(const int idx, std::string mode)
        : turnIndex(idx), algMode(std::move(mode)) {}
};

class Debugger {
public:
    static Debugger& getInstance() {
        static Debugger instance;
        return instance;
    }

    Debugger(const Debugger&) = delete;
    Debugger& operator=(const Debugger&) = delete;

    TurnInfo* startTurn(const std::string& mode = "DEFAULT") {
        std::lock_guard<std::mutex> lock(mtx_);
        assert (mode == "DEFAULT" || mode == "FULL" || mode == "INC");
        std::string realMode;
        if (turnCount_ == 0 || mode == "FULL") {
            realMode = "FULL";  // Default mode if not specified
        } else if (mode == "DEFAULT" && turnCount_ > 0) {
            realMode = "INC";
        } else {
            realMode = "INC";
        }
        turns_.emplace_back(++turnCount_, realMode);
        TurnInfo& turn = turns_.back();
        turn.setMemStart(getCurrentMemoryUsage());
        turn.markStartTime();
        currentTurn_ = &turn;
        return &turn;
    }

    void endTurn() {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!currentTurn_) return;
        currentTurn_->setMemEnd(getCurrentMemoryUsage());
        currentTurn_->setMemPeak(getPeakMemoryUsage());
        currentTurn_->markEndTime();
        // std::cout << "Turn " << turnCount_ << " completed: "
        //           << currentTurn_->getDurationSeconds() << "s, PeakMem=" << currentTurn_->getMemPeak() << "KB\n";
        currentTurn_ = nullptr;
    }


    StageInfo* startStage(StageKind kind) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!currentTurn_) return nullptr;
        currentTurn_->stages.emplace_back(kind);
        StageInfo& stage = currentTurn_->stages.back();
        stage.setMemStart(getCurrentMemoryUsage());
        stage.markStartTime();
        currentStage_ = &stage;
        return &stage;
    }

    void endStage() {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!currentStage_ || !currentTurn_) return;
        currentStage_->setMemEnd(getCurrentMemoryUsage());
        currentStage_->setMemPeak(getPeakMemoryUsage());
        currentStage_->markEndTime();
        currentStage_ = nullptr;
    }

    IterationInfo* startIteration() {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!currentStage_) return nullptr;
        size_t idx = currentStage_->iterations.size();
        currentStage_->iterations.emplace_back(static_cast<int>(idx));
        IterationInfo& iter = currentStage_->iterations.back();
        iter.setMemStart(getCurrentMemoryUsage());
        iter.markStartTime();
        currentIteration_ = &iter;
        return &iter;
    }

    void endIteration() {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!currentIteration_) return;
        currentIteration_->setMemEnd(getCurrentMemoryUsage());
        currentIteration_->setMemPeak(getPeakMemoryUsage());
        currentIteration_->markEndTime();
        currentIteration_ = nullptr;
    }

    void addInfo(const std::string& key, const std::string& value) const {
        std::lock_guard<std::mutex> lock(mtx_);
        if (currentIteration_) currentIteration_->setInfo(key, value);
        else if (currentStage_) currentStage_->setInfo(key, value);
        else if (currentTurn_) currentTurn_->setInfo(key, value);
    }

    void logMessage(Level level, const std::string& message) const {
        std::lock_guard<std::mutex> lock(mtx_);
        if (currentIteration_) currentIteration_->logMessage(level, message);
        else if (currentStage_) currentStage_->logMessage(level, message);
        else if (currentTurn_) currentTurn_->logMessage(level, message);
    }

    void printReport(std::ostream& os) {
        std::lock_guard<std::mutex> lock(mtx_);
        for (const auto& turn : turns_) {
            os << "Turn " << turn.turnIndex << " [" << turn.algMode << "]: "
               << "Time=" << turn.getDurationSeconds() << "s, PeakMem=" << turn.getMemPeak() << "KB\n";
            for (const auto& [k, v] : turn.getInfoMap()) {
                os << "    " << k << ": " << v << "\n";
            }
            for (const auto& [lvl, msgs] : turn.getLogs()) {
                for (const auto& msg : msgs) {
                    os << "    [" << levelToString(lvl) << "] " << msg << "\n";
                }
            }
            for (const auto& stage : turn.stages) {
                os << "  Stage " << stageKindToString(stage.kind) << ": Time=" << stage.getDurationSeconds()
                   << "s, PeakMem=" << stage.getMemPeak() << "KB\n";
                for (const auto& [k, v] : stage.getInfoMap()) {
                    os << "    " << k << ": " << v << "\n";
                }
                for (const auto& [lvl, msgs] : stage.getLogs()) {
                    for (const auto& msg : msgs) {
                        os << "    [" << levelToString(lvl) << "] " << msg << "\n";
                    }
                }
                // TODO: temporarily disable detailed iteration logs
                if (false) {
                    for (const auto& iter : stage.iterations) {
                        os << "    Iteration " << iter.iterationIndex << ": Time=" << iter.getDurationSeconds()
                           << "s, MemUsage=" << iter.getMemEnd() << "KB, Peak=" << iter.getMemPeak() << "KB\n";
                        for (const auto& [k, v] : iter.getInfoMap()) {
                            os << "      " << k << ": " << v << "\n";
                        }
                        for (const auto& [lvl, msgs] : iter.getLogs()) {
                            for (const auto& msg : msgs) {
                                os << "      [" << levelToString(lvl) << "] " << msg << "\n";
                            }
                        }
                    }
                }

            }
        }
    }




    void printReportJson(std::ostream& os) {
        using namespace json11;

        Json::array json_turns;

        for (const auto& turn : turns_) {
            Json::object jturn;
            jturn["index"] = Json(static_cast<int>(turn.turnIndex));
            jturn["mode"] = Json(turn.algMode);
            jturn["time_seconds"] = Json(turn.getDurationSeconds());
            // jturn["peak_mem_kb"] = Json(static_cast<long long>(turn.getMemPeak()));

            // Turn-level info map
            Json::object info_map;
            for (const auto& [key, value] : turn.getInfoMap()) {
                info_map[key] = Json(value);
            }
            jturn["info"] = info_map;

            // Turn-level logs
            Json::object logs_obj;
            for (const auto& [lvl, msgs] : turn.getLogs()) {
                Json::array log_array;
                for (const auto& msg : msgs) {
                    log_array.push_back(Json(msg));
                }
                logs_obj[levelToString(lvl)] = log_array;
            }
            jturn["logs"] = logs_obj;

            // Stages
            Json::array stages_array;
            for (const auto& stage : turn.stages) {
                Json::object jstage;
                jstage["name"] = Json(stageKindToString(stage.kind));
                jstage["time_seconds"] = Json(stage.getDurationSeconds());
                jstage["peak_mem_kb"] = Json(static_cast<long long>(stage.getMemPeak()));

                // Stage-level info
                Json::object stage_info;
                for (const auto& [key, value] : stage.getInfoMap()) {
                    stage_info[key] = Json(value);
                }
                jstage["info"] = stage_info;

                // Stage-level logs
                Json::object stage_logs;
                for (const auto& [lvl, msgs] : stage.getLogs()) {
                    Json::array log_array;
                    for (const auto& msg : msgs) {
                        log_array.push_back(Json(msg));
                    }
                    stage_logs[levelToString(lvl)] = log_array;
                }
                jstage["logs"] = stage_logs;

                stages_array.push_back(jstage);
            }

            jturn["stages"] = stages_array;
            json_turns.push_back(jturn);
        }

        Json report_json = Json::object{{"turns", json_turns}};
        os << report_json.dump() << std::endl;
    }

private:
    Debugger() : turnCount_(0), currentTurn_(nullptr), currentStage_(nullptr), currentIteration_(nullptr) {}

    mutable std::mutex mtx_;
    int turnCount_;
    std::vector<TurnInfo> turns_;
    TurnInfo* currentTurn_;
    StageInfo* currentStage_;
    IterationInfo* currentIteration_;

    size_t getCurrentMemoryUsage() const {
        std::ifstream status("/proc/self/status");
        std::string line;
        while (std::getline(status, line)) {
            if (line.rfind("VmRSS:", 0) == 0) {
                std::istringstream iss(line);
                std::string label, value, unit;
                iss >> label >> value >> unit;
                return std::stoul(value);
            }
        }
        return 0;
    }

    size_t getPeakMemoryUsage() const {
        std::ifstream status("/proc/self/status");
        std::string line;
        while (std::getline(status, line)) {
            if (line.rfind("VmHWM:", 0) == 0) {
                std::istringstream iss(line);
                std::string label, value, unit;
                iss >> label >> value >> unit;
                return std::stoul(value);
            }
        }
        return 0;
    }
};

#endif // DEBUGGER_HPP
