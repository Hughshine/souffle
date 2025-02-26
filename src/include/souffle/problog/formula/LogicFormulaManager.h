#ifndef LOGICFORMULAMANAGER_H
#define LOGICFORMULAMANAGER_H
#include <memory>
#include <string>
#include <vector>
#include "souffle/problog/formula/FormulaManager.h"

class LogicNodeRef {
public:
    enum Type {
        VAR,
        AND,
        OR,
        NOT
    };

    // Constructor for variables
    LogicNodeRef(const int varIndex)
        : type(VAR), varIndex(varIndex) {}

    // Constructor for unary operations (NOT)
    LogicNodeRef(const Type type, LogicNodeRef operand)
        : type(type), operands({std::move(operand)}) {
        if (type != NOT) throw std::invalid_argument("Invalid unary operator");
    }

    // Constructor for binary operations (AND, OR)
    LogicNodeRef(const Type type, const std::vector<LogicNodeRef>& nodes)
        : type(type), operands(std::move(nodes)) {
        if (type != AND && type != OR) throw std::invalid_argument("Invalid binary operator");
    }

    // Copy constructor
    LogicNodeRef(const LogicNodeRef& other)
        : type(other.type), varIndex(other.varIndex), operands(other.operands) {}

    // Move constructor
    LogicNodeRef(LogicNodeRef&& other) noexcept
        : type(other.type), varIndex(other.varIndex), operands(std::move(other.operands)) {}

    // Copy assignment
    LogicNodeRef& operator=(const LogicNodeRef& other) {
        if (this != &other) {
            type = other.type;
            varIndex = other.varIndex;
            operands = other.operands;
        }
        return *this;
    }

    // Move assignment
    LogicNodeRef& operator=(LogicNodeRef&& other) noexcept {
        if (this != &other) {
            type = other.type;
            varIndex = other.varIndex;
            operands = std::move(other.operands);
        }
        return *this;
    }

    std::string toString() const {
        std::string result;
        switch (type) {
            case VAR:
                result = "x" + std::to_string(varIndex); break;
            case NOT:
                result = "!(" + operands[0].toString() + ")"; break;
            case AND:
                result = "(";
                for (size_t i = 0; i < operands.size(); i++) {
                    result += operands[i].toString();
                    if (i < operands.size() - 1) result += " /\\ ";
                }
                result += ")"; break;
            case OR:
                result = "(";
                for (size_t i = 0; i < operands.size(); i++) {
                    result += operands[i].toString();
                    if (i < operands.size() - 1) result += " \\/ ";
                }
                result += ")"; break;
        }
        return result;
    }



    Type getType() const { return type; }
    int getVarIndex() const { return varIndex; }
    const std::vector<LogicNodeRef>& getOperands() const { return operands; }

    bool isVar() const { return type == VAR; }
    bool isNot() const { return type == NOT; }
    bool isAnd() const { return type == AND; }
    bool isOr() const { return type == OR; }
    std::vector<LogicNodeRef> getOperands() { return operands; }

private:
    Type type;
    int varIndex = -1;  // Only used for VAR type
    std::vector<LogicNodeRef> operands;  // Used for AND, OR, NOT
};

// Final implementation of a logic formula manager
class LogicFormulaManager final : public FormulaManager<LogicNodeRef> {
public:
    LogicFormulaManager() = default;
    ~LogicFormulaManager() override = default;

    LogicNodeRef createVar(int index) override {
        return LogicNodeRef(index);
    }

    LogicNodeRef makeAnd(const LogicNodeRef& a, const LogicNodeRef& b) override {
        return LogicNodeRef(LogicNodeRef::AND, {a, b});
    }

    LogicNodeRef makeAnd(const std::vector<LogicNodeRef>& nodes) override {
        return LogicNodeRef(LogicNodeRef::AND, std::move(nodes));
    }

    LogicNodeRef makeOr(const LogicNodeRef& a, const LogicNodeRef& b) override {
        return LogicNodeRef(LogicNodeRef::OR, {a, b});
    }

    LogicNodeRef makeOr(const std::vector<LogicNodeRef>& nodes) override {
        return LogicNodeRef(LogicNodeRef::OR, std::move(nodes));
    }

    LogicNodeRef makeNot(const LogicNodeRef& a) override {
        return LogicNodeRef(LogicNodeRef::NOT, a);
    }

    void printInfo(const LogicNodeRef& node, const std::string& name) override {
        std::cout << name << " = ";
        std::cout << node.toString();
        std::cout << std::endl;
    }
};
#endif //LOGICFORMULAMANAGER_H
