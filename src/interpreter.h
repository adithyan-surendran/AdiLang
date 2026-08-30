#ifndef ADILANG_INTERPRETER_H
#define ADILANG_INTERPRETER_H

#include "ast.h"
#include "environment.h"
#include <iostream>
#include <stdexcept>
#include <memory>
#include <vector>
#include <cmath>

// Forward declaration
class Interpreter;

// Runtime representation of an AdiLang user-defined function
struct AdiFunction {
    const FunctionStatement* declaration;
    std::shared_ptr<Environment> closure;

    AdiFunction(const FunctionStatement* declaration, std::shared_ptr<Environment> closure)
        : declaration(declaration), closure(closure) {}

    Value call(Interpreter& interpreter, const std::vector<Value>& arguments);
};

// Sentinel jump signals for interpreter unwinding
struct BreakJump {};
struct ContinueJump {};
struct ReturnJump {
    Value value;
    explicit ReturnJump(Value val) : value(val) {}
};

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
            std::cerr << "CRITICAL RUNTIME ERROR: " << error.what() << "\n";
        }
    }

    // Public helper so AdiFunction can execute blocks in its own scope environment
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
        // 6. While Statement: while (<cond>) <body>
        else if (auto whileStmt = dynamic_cast<const WhileStatement*>(stmt)) {
            while (isTruthy(evaluate(whileStmt->condition.get()))) {
                try {
                    execute(whileStmt->body.get());
                } catch (const ContinueJump&) {
                    continue; // Jump to next loop iteration
                } catch (const BreakJump&) {
                    break;    // Exit loop completely
                }
            }
        }
        // 7. Break Statement
        else if (dynamic_cast<const BreakStatement*>(stmt)) {
            throw BreakJump{};
        }
        // 8. Continue Statement
        else if (dynamic_cast<const ContinueStatement*>(stmt)) {
            throw ContinueJump{};
        }
        // 9. Function Declaration: fn name(...) { ... }
        else if (auto funcStmt = dynamic_cast<const FunctionStatement*>(stmt)) {
            auto function = std::make_shared<AdiFunction>(funcStmt, environment);
            environment->define(funcStmt->name, function);
        }
        // 10. Return Statement: return <expr>;
        else if (auto returnStmt = dynamic_cast<const ReturnStatement*>(stmt)) {
            Value val = false; // Default return if empty
            if (returnStmt->value != nullptr) {
                val = evaluate(returnStmt->value.get());
            }
            throw ReturnJump(val);
        }
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

        // 4. Function Call Expression: callee(args...)
        if (auto callExpr = dynamic_cast<const CallExpr*>(expr)) {
            Value callee = evaluate(callExpr->callee.get());

            std::vector<Value> arguments;
            for (const auto& arg : callExpr->arguments) {
                arguments.push_back(evaluate(arg.get()));
            }

            if (!std::holds_alternative<std::shared_ptr<AdiFunction>>(callee)) {
                throw std::runtime_error("Can only call functions.");
            }

            auto function = std::get<std::shared_ptr<AdiFunction>>(callee);
            
            if (arguments.size() != function->declaration->params.size()) {
                throw std::runtime_error("Expected " + std::to_string(function->declaration->params.size()) +
                                            " arguments but got " + std::to_string(arguments.size()) + ".");
            }

            return function->call(*this, arguments);
        }

        // 5. Unary Operations: !a, -a
        if (auto un = dynamic_cast<const UnaryExpr*>(expr)) {
            Value right = evaluate(un->right.get());

            if (un->op == TokenType::BANG) {
                return !isTruthy(right);
            }
            if (un->op == TokenType::MINUS) {
                if (std::holds_alternative<double>(right)) {
                    return -std::get<double>(right);
                }
                throw std::runtime_error("Unary '-' operand must be a number.");
            }
        }

        // 6. Logical Operations with Short-Circuiting: &&, ||
        if (auto log = dynamic_cast<const LogicalExpr*>(expr)) {
            Value left = evaluate(log->left.get());

            if (log->op == TokenType::OR_OR) {
                if (isTruthy(left)) return true;
            } else if (log->op == TokenType::AND_AND) {
                if (!isTruthy(left)) return false;
            }

            return isTruthy(evaluate(log->right.get()));
        }

        // 7. Binary Operations
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
                    case TokenType::PERCENT:
                        if (r == 0.0) throw std::runtime_error("Modulo by zero.");
                        return std::fmod(l, r);
                    case TokenType::GREATER: return l > r;
                    case TokenType::GREATER_EQUAL: return l >= r;
                    case TokenType::LESS: return l < r;
                    case TokenType::LESS_EQUAL: return l <= r;
                    case TokenType::EQUAL_EQUAL: return l == r;
                    case TokenType::BANG_EQUAL: return l != r;
                    default:
                        throw std::runtime_error("Unsupported operator on numbers.");
                }
            }

            // String Concatenation & Comparison
            if (std::holds_alternative<std::string>(left) && std::holds_alternative<std::string>(right)) {
                if (bin->op == TokenType::PLUS) {
                    return std::get<std::string>(left) + std::get<std::string>(right);
                }
                if (bin->op == TokenType::EQUAL_EQUAL) {
                    return std::get<std::string>(left) == std::get<std::string>(right);
                }
                if (bin->op == TokenType::BANG_EQUAL) {
                    return std::get<std::string>(left) != std::get<std::string>(right);
                }
                throw std::runtime_error("Unsupported operator for strings.");
            }

            // Boolean Comparisons
            if (std::holds_alternative<bool>(left) && std::holds_alternative<bool>(right)) {
                if (bin->op == TokenType::EQUAL_EQUAL) {
                    return std::get<bool>(left) == std::get<bool>(right);
                }
                if (bin->op == TokenType::BANG_EQUAL) {
                    return std::get<bool>(left) != std::get<bool>(right);
                }
                throw std::runtime_error("Unsupported operator for booleans.");
            }

            throw std::runtime_error("Type error in binary operation.");
        }

        throw std::runtime_error("Unknown expression node.");
    }
};

// Implement AdiFunction::call outside Interpreter class definition
inline Value AdiFunction::call(Interpreter& interpreter, const std::vector<Value>& arguments) {
    auto funcEnv = std::make_shared<Environment>(closure);

    for (size_t i = 0; i < declaration->params.size(); i++) {
        funcEnv->define(declaration->params[i], arguments[i]);
    }

    try {
        interpreter.executeBlock(declaration->body->statements, funcEnv);
    } catch (const ReturnJump& ret) {
        return ret.value;
    }

    return false; // Implicit return
}

#endif // ADILANG_INTERPRETER_H