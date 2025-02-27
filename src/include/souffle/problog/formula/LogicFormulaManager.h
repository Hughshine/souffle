#ifndef LOGICFORMULAMANAGER_H
#define LOGICFORMULAMANAGER_H
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <stdexcept>
#include <iostream>
#include "souffle/problog/formula/FormulaManager.h"

// Forward declaration for the formula node
class LogicNode;

// Reference wrapper for LogicNode, acts as a handle to the internally managed nodes
class LogicNodeRef {
public:
    enum Type {
        VAR,
        AND,
        OR,
        NOT
    };

    // Default constructor for containers
    LogicNodeRef() : node(nullptr) {}

    // Internal constructor, used by LogicFormulaManager
    explicit LogicNodeRef(std::shared_ptr<LogicNode> node) : node(std::move(node)) {}

    std::string toString() const;

    Type getType() const;
    int getVarIndex() const;
    const std::vector<LogicNodeRef>& getOperands() const;
    std::vector<LogicNodeRef> getOperands();

    bool isVar() const;
    bool isNot() const;
    bool isAnd() const;
    bool isOr() const;

    // Helper for equality comparison in hash tables
    bool operator==(const LogicNodeRef& other) const {
        return node == other.node;
    }

    // Get the internal node - for manager use only
    std::shared_ptr<LogicNode> getNode() const {
        return node;
    }

private:
    std::shared_ptr<LogicNode> node;
};

// The actual node implementation, managed by LogicFormulaManager
class LogicNode {
public:
    using Type = LogicNodeRef::Type;

    // Constructor for variable nodes
    explicit LogicNode(int varIndex)
        : type(Type::VAR), varIndex(varIndex) {}

    // Constructor for NOT nodes
    LogicNode(Type type, LogicNodeRef operand)
        : type(type), operands({operand}) {
        if (type != Type::NOT) throw std::invalid_argument("Invalid unary operator");
    }

    // Constructor for AND/OR nodes
    LogicNode(Type type, const std::vector<LogicNodeRef>& ops)
        : type(type), operands(ops) {
        if (type != Type::AND && type != Type::OR)
            throw std::invalid_argument("Invalid binary operator");
    }

    // Getters
    Type getType() const { return type; }
    int getVarIndex() const { return varIndex; }
    const std::vector<LogicNodeRef>& getOperands() const { return operands; }

    bool isVar() const { return type == Type::VAR; }
    bool isNot() const { return type == Type::NOT; }
    bool isAnd() const { return type == Type::AND; }
    bool isOr() const { return type == Type::OR; }

    // String representation for debugging/display
    std::string toString() const {
        std::string result;
        switch (type) {
            case Type::VAR:
                result = "x" + std::to_string(varIndex); break;
            case Type::NOT:
                result = "!(" + operands[0].toString() + ")"; break;
            case Type::AND:
                result = "(";
                for (size_t i = 0; i < operands.size(); i++) {
                    result += operands[i].toString();
                    if (i < operands.size() - 1) result += " /\\ ";
                }
                result += ")"; break;
            case Type::OR:
                result = "(";
                for (size_t i = 0; i < operands.size(); i++) {
                    result += operands[i].toString();
                    if (i < operands.size() - 1) result += " \\/ ";
                }
                result += ")"; break;
        }
        return result;
    }

    // Hash function for node uniqueness
    size_t hash() const {
        size_t h = std::hash<int>{}(static_cast<int>(type));
        if (isVar()) {
            h ^= std::hash<int>{}(varIndex) << 1;
        } else {
            for (const auto& op : operands) {
                h ^= std::hash<void*>{}(op.getNode().get()) << 1;
            }
        }
        return h;
    }

    // Structural equality for node uniqueness
    bool structurallyEqual(const LogicNode& other) const {
        if (type != other.type) return false;
        if (isVar()) return varIndex == other.varIndex;

        if (operands.size() != other.operands.size()) return false;
        for (size_t i = 0; i < operands.size(); i++) {
            // For structural equality, we compare by pointer identity
            // since equivalent formulas will share the same node instances
            if (operands[i].getNode() != other.operands[i].getNode()) return false;
        }
        return true;
    }

private:
    Type type;
    int varIndex = -1;  // Only used for VAR type
    std::vector<LogicNodeRef> operands;  // Used for AND, OR, NOT
};

// Forward definitions for LogicNodeRef methods
inline std::string LogicNodeRef::toString() const {
    return node ? node->toString() : "null";
}

inline LogicNodeRef::Type LogicNodeRef::getType() const {
    return node ? node->getType() : Type::VAR;
}

inline int LogicNodeRef::getVarIndex() const {
    return node ? node->getVarIndex() : -1;
}

inline const std::vector<LogicNodeRef>& LogicNodeRef::getOperands() const {
    static const std::vector<LogicNodeRef> empty;
    return node ? node->getOperands() : empty;
}

inline std::vector<LogicNodeRef> LogicNodeRef::getOperands() {
    return node ? std::vector<LogicNodeRef>(node->getOperands()) : std::vector<LogicNodeRef>();
}

inline bool LogicNodeRef::isVar() const {
    return node && node->isVar();
}

inline bool LogicNodeRef::isNot() const {
    return node && node->isNot();
}

inline bool LogicNodeRef::isAnd() const {
    return node && node->isAnd();
}

inline bool LogicNodeRef::isOr() const {
    return node && node->isOr();
}

// Hash function for node uniqueness
struct LogicNodeHash {
    size_t operator()(const std::shared_ptr<LogicNode>& node) const {
        return node->hash();
    }
};

// Equality function for node uniqueness
struct LogicNodeEqual {
    bool operator()(const std::shared_ptr<LogicNode>& a, const std::shared_ptr<LogicNode>& b) const {
        return a->structurallyEqual(*b);
    }
};

// Final implementation of a logic formula manager with RAII style
class LogicFormulaManager final : public FormulaManager<LogicNodeRef> {
public:
    LogicFormulaManager() = default;
    ~LogicFormulaManager() override = default;

    // Create a variable node
    LogicNodeRef createVar(int index) override {
        auto prototype = std::make_shared<LogicNode>(index);
        return LogicNodeRef(getUniqueNode(std::move(prototype)));
    }

    // Create an AND node from two operands
    LogicNodeRef makeAnd(const LogicNodeRef& a, const LogicNodeRef& b) override {
        return makeAnd(std::vector<LogicNodeRef>{a, b});
    }

    // Create an AND node from multiple operands
    LogicNodeRef makeAnd(const std::vector<LogicNodeRef>& nodes) override {
        if (nodes.empty()) {
            // Empty AND is equivalent to TRUE in most logic systems
            // You may want to handle this differently based on your requirements
            throw std::invalid_argument("Empty AND not supported");
        }
        if (nodes.size() == 1) {
            return nodes[0]; // Simplification: AND with one argument is the argument itself
        }
        auto prototype = std::make_shared<LogicNode>(LogicNodeRef::Type::AND, nodes);
        return LogicNodeRef(getUniqueNode(std::move(prototype)));
    }

    // Create an OR node from two operands
    LogicNodeRef makeOr(const LogicNodeRef& a, const LogicNodeRef& b) override {
        return makeOr(std::vector<LogicNodeRef>{a, b});
    }

    // Create an OR node from multiple operands
    LogicNodeRef makeOr(const std::vector<LogicNodeRef>& nodes) override {
        if (nodes.empty()) {
            // Empty OR is equivalent to FALSE in most logic systems
            // You may want to handle this differently based on your requirements
            throw std::invalid_argument("Empty OR not supported");
        }
        if (nodes.size() == 1) {
            return nodes[0]; // Simplification: OR with one argument is the argument itself
        }
        auto prototype = std::make_shared<LogicNode>(LogicNodeRef::Type::OR, nodes);
        return LogicNodeRef(getUniqueNode(std::move(prototype)));
    }

    // Create a NOT node
    LogicNodeRef makeNot(const LogicNodeRef& a) override {
        auto prototype = std::make_shared<LogicNode>(LogicNodeRef::Type::NOT, a);
        return LogicNodeRef(getUniqueNode(std::move(prototype)));
    }

    // Check if two formulas are the same (now uses pointer equality which works
    // because equivalent formulas share the same node instance)
    bool isSame(const LogicNodeRef& a, const LogicNodeRef& b) override {
        return a.getNode() == b.getNode();
    }

    // Print debug information about a node
    void printInfo(const LogicNodeRef& node, const std::string& name) override {
        std::cout << name << " = ";
        std::cout << node.toString();
        std::cout << std::endl;
    }

private:
    // Get a unique node from the cache or insert a new one if not found
    std::shared_ptr<LogicNode> getUniqueNode(std::shared_ptr<LogicNode> prototype) {
        auto it = uniqueNodes.find(prototype);
        if (it != uniqueNodes.end()) {
            return *it; // Return existing node
        }

        // Insert and return the new node
        uniqueNodes.insert(prototype);
        return prototype;
    }

    // Cache of unique nodes - this is the key to maintaining unique formula representations
    std::unordered_set<std::shared_ptr<LogicNode>, LogicNodeHash, LogicNodeEqual> uniqueNodes;
};

#endif //LOGICFORMULAMANAGER_H
//#ifndef LOGICFORMULAMANAGER_H
//#define LOGICFORMULAMANAGER_H
//#include <memory>
//#include <string>
//#include <vector>
//#include "souffle/problog/formula/FormulaManager.h"
//
//class LogicNodeRef {
//public:
//    enum Type {
//        VAR,
//        AND,
//        OR,
//        NOT
//    };
//
//    // Constructor for variables
//    LogicNodeRef(const int varIndex)
//        : type(VAR), varIndex(varIndex) {}
//
//    // Constructor for unary operations (NOT)
//    LogicNodeRef(const Type type, LogicNodeRef operand)
//        : type(type), operands({std::move(operand)}) {
//        if (type != NOT) throw std::invalid_argument("Invalid unary operator");
//    }
//
//    // Constructor for binary operations (AND, OR)
//    LogicNodeRef(const Type type, const std::vector<LogicNodeRef>& nodes)
//        : type(type), operands(std::move(nodes)) {
//        if (type != AND && type != OR) throw std::invalid_argument("Invalid binary operator");
//    }
//
//    // Copy constructor
//    LogicNodeRef(const LogicNodeRef& other)
//        : type(other.type), varIndex(other.varIndex), operands(other.operands) {}
//
//    // Move constructor
//    LogicNodeRef(LogicNodeRef&& other) noexcept
//        : type(other.type), varIndex(other.varIndex), operands(std::move(other.operands)) {}
//
//    // Copy assignment
//    LogicNodeRef& operator=(const LogicNodeRef& other) {
//        if (this != &other) {
//            type = other.type;
//            varIndex = other.varIndex;
//            operands = other.operands;
//        }
//        return *this;
//    }
//
//    // Move assignment
//    LogicNodeRef& operator=(LogicNodeRef&& other) noexcept {
//        if (this != &other) {
//            type = other.type;
//            varIndex = other.varIndex;
//            operands = std::move(other.operands);
//        }
//        return *this;
//    }
//
//    std::string toString() const {
//        std::string result;
//        switch (type) {
//            case VAR:
//                result = "x" + std::to_string(varIndex); break;
//            case NOT:
//                result = "!(" + operands[0].toString() + ")"; break;
//            case AND:
//                result = "(";
//                for (size_t i = 0; i < operands.size(); i++) {
//                    result += operands[i].toString();
//                    if (i < operands.size() - 1) result += " /\\ ";
//                }
//                result += ")"; break;
//            case OR:
//                result = "(";
//                for (size_t i = 0; i < operands.size(); i++) {
//                    result += operands[i].toString();
//                    if (i < operands.size() - 1) result += " \\/ ";
//                }
//                result += ")"; break;
//        }
//        return result;
//    }
//
//
//
//    Type getType() const { return type; }
//    int getVarIndex() const { return varIndex; }
//    const std::vector<LogicNodeRef>& getOperands() const { return operands; }
//
//    bool isVar() const { return type == VAR; }
//    bool isNot() const { return type == NOT; }
//    bool isAnd() const { return type == AND; }
//    bool isOr() const { return type == OR; }
//    std::vector<LogicNodeRef> getOperands() { return operands; }
//
//private:
//    Type type;
//    int varIndex = -1;  // Only used for VAR type
//    std::vector<LogicNodeRef> operands;  // Used for AND, OR, NOT
//};
//
//// Final implementation of a logic formula manager
//class LogicFormulaManager final : public FormulaManager<LogicNodeRef> {
//public:
//    LogicFormulaManager() = default;
//    ~LogicFormulaManager() override = default;
//
//    LogicNodeRef createVar(int index) override {
//        return LogicNodeRef(index);
//    }
//
//    LogicNodeRef makeAnd(const LogicNodeRef& a, const LogicNodeRef& b) override {
//        return LogicNodeRef(LogicNodeRef::AND, {a, b});
//    }
//
//    LogicNodeRef makeAnd(const std::vector<LogicNodeRef>& nodes) override {
//        return LogicNodeRef(LogicNodeRef::AND, std::move(nodes));
//    }
//
//    LogicNodeRef makeOr(const LogicNodeRef& a, const LogicNodeRef& b) override {
//        return LogicNodeRef(LogicNodeRef::OR, {a, b});
//    }
//
//    LogicNodeRef makeOr(const std::vector<LogicNodeRef>& nodes) override {
//        return LogicNodeRef(LogicNodeRef::OR, std::move(nodes));
//    }
//
//    LogicNodeRef makeNot(const LogicNodeRef& a) override {
//        return LogicNodeRef(LogicNodeRef::NOT, a);
//    }
//
//    bool isSame(const LogicNodeRef& a, const LogicNodeRef& b) override {
//    	return a.get() == b.get();
//    }
//
//
//    void printInfo(const LogicNodeRef& node, const std::string& name) override {
//        std::cout << name << " = ";
//        std::cout << node.toString();
//        std::cout << std::endl;
//    }
//};
//#endif //LOGICFORMULAMANAGER_H
