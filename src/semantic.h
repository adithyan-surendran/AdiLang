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
        else if (auto ifStmt = dynamic_cast<const IfStatement*>(stmt)) {
        analyzeExpr(ifStmt->condition.get());
        analyzeStmt(ifStmt->thenBranch.get());
        if (ifStmt->elseBranch) {
            analyzeStmt(ifStmt->elseBranch.get());
            }
        }
        else if (auto whileStmt = dynamic_cast<const WhileStatement*>(stmt)) {
            analyzeExpr(whileStmt->condition.get());
            analyzeStmt(whileStmt->body.get());
        }
    }

    void analyzeExpr(const Expr* expr) {
        if (!expr) return;

        // 1. Variable Assignment: x = <expr>
        if (auto assignExpr = dynamic_cast<const AssignExpr*>(expr)) {
            if (!symbolTable.isDeclared(assignExpr->name)) {
                error("Cannot assign to undeclared variable '" + assignExpr->name + "'.");
            }
            analyzeExpr(assignExpr->value.get());
        }
        // 2. Variable Usage: x
        else if (auto varExpr = dynamic_cast<const VariableExpr*>(expr)) {
            if (varExpr->name != "true" && varExpr->name != "false") {
                if (!symbolTable.isDeclared(varExpr->name)) {
                    error("Cannot use undeclared variable '" + varExpr->name + "'.");
                }
            }
        }
        // 3. Binary Expression: left + right
        else if (auto binExpr = dynamic_cast<const BinaryExpr*>(expr)) {
            analyzeExpr(binExpr->left.get());
            analyzeExpr(binExpr->right.get());
        }
        // NumberExpr and StringExpr are base literals, always valid
    }
};


#endif // ADILANG_SEMANTIC_H