#ifndef ADILANG_INTERPRETER_H
#define ADILANG_INTERPRETER_H

#include "ast.h"
#include "environment.h"
#include <iostream>
#include <stdexcept>
#include <memory>
#include <vector>

// Helper to determine truthiness in AdiLang (defined BEFORE Interpreter class)
inline bool isTruthy(const Value& val) {
    if (std::holds_alternative<bool>(val)) {
        return std::get<bool>(val);
    }
    if (std::holds_alternative<double>(val)) {
        return std::get<double>(val) != 0.0; // 0 is false, everything else is true
    }
    if (std::holds_alternative<std::string>(val)) {
        return !std::get<std::string>(val).empty(); // Empty string is false
    }
    return false;
}

class Interpreter {
private:
    std::shared_ptr<Environment> environment;

public:
    Interpreter() : environment(std::make_shared<Environment>()) {}

    void interpret(const Program& program) {
        try {
            for (const auto& stmt : program.statements) {
                execute(stmt.get());
            }
        } catch (const std::runtime_error& error) {
            std::cerr << "Runtime Error: " << error.what() << "\n";
        }
    }

private:
    // --- Statement Execution ---
    void execute(const Stmt* stmt) {
        if (!stmt) return;

        // 1. Variable Declaration: let x = <expr>;
        if (auto varDecl = dynamic_cast<const VariableDeclaration*>(stmt)) {
            Value val = evaluate(varDecl->initializer.get());
            environment->define(varDecl->name, val);
        }
        // 2. Print Statement: print(<expr>);
        else if (auto printStmt = dynamic_cast<const PrintStatement*>(stmt)) {
            Value val = evaluate(printStmt->expression.get());
            printValue(val);
            std::cout << "\n";
        }
        // 3. Expression Statement: <expr>;
        else if (auto exprStmt = dynamic_cast<const ExpressionStatement*>(stmt)) {
            evaluate(exprStmt->expression.get());
        }
        // 4. Block Scope: { ... }
        else if (auto block = dynamic_cast<const BlockStatement*>(stmt)) {
            executeBlock(block->statements, std::make_shared<Environment>(environment));
        }
        // 5. If Statement: if (<cond>) <then> else <else>
        else if (auto ifStmt = dynamic_cast<const IfStatement*>(stmt)) {
            Value condVal = evaluate(ifStmt->condition.get());
            if (isTruthy(condVal)) {
                execute(ifStmt->thenBranch.get());
            } else if (ifStmt->elseBranch != nullptr) {
                execute(ifStmt->elseBranch.get());
            }
        }
    }

    void executeBlock(const std::vector<std::unique_ptr<Stmt>>& statements,
                      std::shared_ptr<Environment> blockEnv) {
        std::shared_ptr<Environment> previous = this->environment;
        try {
            this->environment = blockEnv;
            for (const auto& stmt : statements) {
                execute(stmt.get());
            }
        } catch (...) {
            this->environment = previous;
            throw;
        }
        this->environment = previous;
    }

    // --- Expression Evaluation ---
    Value evaluate(const Expr* expr) {
    if (!expr) {
        throw std::runtime_error("Null expression encountered during evaluation.");
    }

    // 1. Variable Assignment: x = <expr>
    if (auto assignExpr = dynamic_cast<const AssignExpr*>(expr)) {
        Value val = evaluate(assignExpr->value.get());
        environment->assign(assignExpr->name, val);
        return val;
    }

    // 2. Literals
    if (auto num = dynamic_cast<const NumberExpr*>(expr)) {
        return num->value;
    }
    if (auto str = dynamic_cast<const StringExpr*>(expr)) {
        return str->value;
    }

    // 3. Variables / Boolean Literals
    if (auto var = dynamic_cast<const VariableExpr*>(expr)) {
        if (var->name == "true") return true;
        if (var->name == "false") return false;
        return environment->get(var->name);
    }

    // 4. Binary Operations
    if (auto bin = dynamic_cast<const BinaryExpr*>(expr)) {
        Value left = evaluate(bin->left.get());
        Value right = evaluate(bin->right.get());

        // Numeric Operations & Comparisons
        if (std::holds_alternative<double>(left) && std::holds_alternative<double>(right)) {
            double l = std::get<double>(left);
            double r = std::get<double>(right);

            switch (bin->op) {
                case TokenType::PLUS: return l + r;
                case TokenType::MINUS: return l - r;
                case TokenType::STAR: return l * r;
                case TokenType::SLASH:
                    if (r == 0.0) throw std::runtime_error("Division by zero.");
                    return l / r;
                case TokenType::GREATER: return l > r;
                case TokenType::GREATER_EQUAL: return l >= r;
                case TokenType::LESS: return l < r;
                case TokenType::LESS_EQUAL: return l <= r;
                case TokenType::EQUAL_EQUAL: return l == r;
                default:
                    throw std::runtime_error("Unsupported operator on numbers.");
            }
        }

        // String Concatenation
        if (std::holds_alternative<std::string>(left) && std::holds_alternative<std::string>(right)) {
            if (bin->op == TokenType::PLUS) {
                return std::get<std::string>(left) + std::get<std::string>(right);
            }
            if (bin->op == TokenType::EQUAL_EQUAL) {
                return std::get<std::string>(left) == std::get<std::string>(right);
            }
            throw std::runtime_error("Only '+' and '==' are supported for strings.");
        }

        // Boolean Comparisons
        if (std::holds_alternative<bool>(left) && std::holds_alternative<bool>(right)) {
            if (bin->op == TokenType::EQUAL_EQUAL) {
                return std::get<bool>(left) == std::get<bool>(right);
            }
            throw std::runtime_error("Only '==' is supported for booleans.");
        }

        throw std::runtime_error("Type error in binary operation.");
    }

    throw std::runtime_error("Unknown expression node.");
}
};

#endif // ADILANG_INTERPRETER_H