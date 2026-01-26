#include "souffle/problog/debug/Debugger.h"

#include "souffle/utility/json11.h"

Debugger::Debugger()
        : turnCount_(0), currentTurn_(nullptr), currentStage_(nullptr), currentIteration_(nullptr) {}

TurnInfo* Debugger::startTurn(const std::string& mode) {
    std::lock_guard<std::mutex> lock(mtx_);
    assert(mode == "DEFAULT" || mode == "FULL" || mode == "FULL-HARD" || mode == "FULL-SOFT" ||
            mode == "INC");
    std::string realMode;
    if (mode == "DEFAULT") {
        realMode = (turnCount_ == 0) ? "FULL-HARD" : "INC";
    } else if (mode == "FULL" || mode == "FULL-HARD" || mode == "FULL-SOFT") {
        realMode = mode;
    } else {
        realMode = "INC";
    }
    turns_.emplace_back(++turnCount_, realMode);
    TurnInfo& turn = turns_.back();
//    turn.setMemStart(getCurrentMemoryUsage());
    turn.markStartTime();
    currentTurn_ = &turn;
    return &turn;
}

void Debugger::endTurn() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!currentTurn_) return;
//    currentTurn_->setMemEnd(getCurrentMemoryUsage());
//    currentTurn_->setMemPeak(getPeakMemoryUsage());
    currentTurn_->markEndTime();
    currentTurn_ = nullptr;
}

StageInfo* Debugger::startStage(StageKind kind) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!currentTurn_) return nullptr;
    currentTurn_->stages.emplace_back(kind);
    StageInfo& stage = currentTurn_->stages.back();
//    stage.setMemStart(getCurrentMemoryUsage());
    stage.markStartTime();
    currentStage_ = &stage;
    return &stage;
}

void Debugger::endStage() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!currentStage_ || !currentTurn_) return;
//    currentStage_->setMemEnd(getCurrentMemoryUsage());
//    currentStage_->setMemPeak(getPeakMemoryUsage());
    currentStage_->markEndTime();
    currentStage_ = nullptr;
}

IterationInfo* Debugger::startIteration() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!currentStage_) return nullptr;
    size_t idx = currentStage_->iterations.size();
    currentStage_->iterations.emplace_back(static_cast<int>(idx));
    IterationInfo& iter = currentStage_->iterations.back();
//    iter.setMemStart(getCurrentMemoryUsage());
    iter.markStartTime();
    currentIteration_ = &iter;
    return &iter;
}

void Debugger::endIteration() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!currentIteration_) return;
//    currentIteration_->setMemEnd(getCurrentMemoryUsage());
//    currentIteration_->setMemPeak(getPeakMemoryUsage());
    currentIteration_->markEndTime();
    currentIteration_ = nullptr;
}

void Debugger::addInfo(const std::string& key, const std::string& value) const {
    std::lock_guard<std::mutex> lock(mtx_);
    if (currentIteration_) {
        currentIteration_->setInfo(key, value);
    } else if (currentStage_) {
        currentStage_->setInfo(key, value);
    } else if (currentTurn_) {
        currentTurn_->setInfo(key, value);
    }
}

void Debugger::logMessage(Level level, const std::string& message) const {
    std::lock_guard<std::mutex> lock(mtx_);
    if (currentIteration_) {
        currentIteration_->logMessage(level, message);
    } else if (currentStage_) {
        currentStage_->logMessage(level, message);
    } else if (currentTurn_) {
        currentTurn_->logMessage(level, message);
    }
}

void Debugger::printReport(std::ostream& os) {
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

void Debugger::printReportJson(std::ostream& os) {
    using namespace json11;

    Json::array json_turns;

    for (const auto& turn : turns_) {
        Json::object jturn;
        jturn["index"] = Json(static_cast<int>(turn.turnIndex));
        jturn["mode"] = Json(turn.algMode);
        jturn["time_seconds"] = Json(turn.getDurationSeconds());

        Json::object info_map;
        for (const auto& [key, value] : turn.getInfoMap()) {
            info_map[key] = Json(value);
        }
        jturn["info"] = info_map;

        Json::object logs_obj;
        for (const auto& [lvl, msgs] : turn.getLogs()) {
            Json::array log_array;
            for (const auto& msg : msgs) {
                log_array.push_back(Json(msg));
            }
            logs_obj[levelToString(lvl)] = log_array;
        }
        jturn["logs"] = logs_obj;

        Json::array stages_array;
        for (const auto& stage : turn.stages) {
            Json::object jstage;
            jstage["name"] = Json(stageKindToString(stage.kind));
            jstage["time_seconds"] = Json(stage.getDurationSeconds());
            jstage["peak_mem_kb"] = Json(static_cast<long long>(stage.getMemPeak()));

            Json::object stage_info;
            for (const auto& [key, value] : stage.getInfoMap()) {
                stage_info[key] = Json(value);
            }
            jstage["info"] = stage_info;

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

size_t Debugger::getCurrentMemoryUsage() const {
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

size_t Debugger::getPeakMemoryUsage() const {
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
