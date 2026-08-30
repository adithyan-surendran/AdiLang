#ifndef ADILANG_SEMANTIC_H
#define ADILANG_SEMANTIC_H

#include <iostream>
#include <string>
#include "ast.h"
#include "symbol_table.h"

class SemanticAnalyzer {
private:
    SymbolTable symbolTable;
    bool hasError = false;

    void error(const std::string& message) {
        std::cerr << "Semantic Error: " << message << "\n";
        hasError = true;
    }

public:
    bool analyze(const Program& program) {
        hasError = false;
        for (const auto& stmt : program.statements) {
            analyzeStmt(stmt.get());
        }
        return !hasError;
    }

private:
    void analyzeStmt(const Stmt* stmt) {
        if (!stmt) return;

        // 1. Block Scope { ... }
        if (auto block = dynamic_cast<const BlockStatement*>(stmt)) {
            symbolTable.enterScope();
            for (const auto& innerStmt : block->statements) {
                analyzeStmt(innerStmt.get());
            }
            symbolTable.exitScope();
        }
        // 2. Variable Declaration: let x = <expr>;
        else if (auto varDecl = dynamic_cast<const VariableDeclaration*>(stmt)) {
            // Check the expression on the right first
            analyzeExpr(varDecl->initializer.get());

            // Then try to register the variable
            if (!symbolTable.declare(varDecl->name)) {
                error("Variable '" + varDecl->name + "' is already declared in this scope.");
            }
        }
        // 3. Print Statement: print(<expr>);
        else if (auto printStmt = dynamic_cast<const PrintStatement*>(stmt)) {
            analyzeExpr(printStmt->expression.get());
        }
    }

    void analyzeExpr(const Expr* expr) {
        if (!expr) return;

        // Variable usage: x
        if (auto varExpr = dynamic_cast<const VariableExpr*>(expr)) {
            if (varExpr->name != "true" && varExpr->name != "false") {
                if (!symbolTable.isDeclared(varExpr->name)) {
                    error("Cannot use undeclared variable '" + varExpr->name + "'.");
                }
            }
        }
        // Binary expression: left + right
        else if (auto binExpr = dynamic_cast<const BinaryExpr*>(expr)) {
            analyzeExpr(binExpr->left.get());
            analyzeExpr(binExpr->right.get());
        }
        // NumberExpr and StringExpr are base literals, always valid
    }
};

#endif // ADILANG_SEMANTIC_H