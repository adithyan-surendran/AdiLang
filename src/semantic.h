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
    int loopDepth = 0; // Tracks whether we are inside a loop

    void error(const std::string& message) {
        std::cerr << "Semantic Error: " << message << "\n";
        hasError = true;
    }

public:
    bool analyze(const Program& program) {
        hasError = false;
        loopDepth = 0;
        for (const auto& stmt : program.statements) {
            analyzeStmt(stmt.get());
        }
        return !hasError;
    }

private:
    void analyzeStmt(const Stmt* stmt) {
        if (!stmt) return;

        // 1. Block Scope
        if (auto block = dynamic_cast<const BlockStatement*>(stmt)) {
            symbolTable.enterScope();
            for (const auto& innerStmt : block->statements) {
                analyzeStmt(innerStmt.get());
            }
            symbolTable.exitScope();
        }
        // 2. Variable Declaration
        else if (auto varDecl = dynamic_cast<const VariableDeclaration*>(stmt)) {
            analyzeExpr(varDecl->initializer.get());
            if (!symbolTable.declare(varDecl->name)) {
                error("Variable '" + varDecl->name + "' is already declared in this scope.");
            }
        }
        // 3. Print Statement
        else if (auto printStmt = dynamic_cast<const PrintStatement*>(stmt)) {
            analyzeExpr(printStmt->expression.get());
        }
        // 4. Expression Statement
        else if (auto exprStmt = dynamic_cast<const ExpressionStatement*>(stmt)) {
            analyzeExpr(exprStmt->expression.get());
        }
        // 5. If Statement
        else if (auto ifStmt = dynamic_cast<const IfStatement*>(stmt)) {
            analyzeExpr(ifStmt->condition.get());
            analyzeStmt(ifStmt->thenBranch.get());
            if (ifStmt->elseBranch) {
                analyzeStmt(ifStmt->elseBranch.get());
            }
        }
        // 6. While Statement
        else if (auto whileStmt = dynamic_cast<const WhileStatement*>(stmt)) {
            analyzeExpr(whileStmt->condition.get());
            loopDepth++;
            analyzeStmt(whileStmt->body.get());
            loopDepth--;
        }
        // 7. Break Statement
        else if (dynamic_cast<const BreakStatement*>(stmt)) {
            if (loopDepth == 0) {
                error("Cannot use 'break' outside of a loop.");
            }
        }
        // 8. Continue Statement
        else if (dynamic_cast<const ContinueStatement*>(stmt)) {
            if (loopDepth == 0) {
                error("Cannot use 'continue' outside of a loop.");
            }
        }
    }

    void analyzeExpr(const Expr* expr) {
        if (!expr) return;

        if (auto assignExpr = dynamic_cast<const AssignExpr*>(expr)) {
            if (!symbolTable.isDeclared(assignExpr->name)) {
                error("Cannot assign to undeclared variable '" + assignExpr->name + "'.");
            }
            analyzeExpr(assignExpr->value.get());
        }
        else if (auto varExpr = dynamic_cast<const VariableExpr*>(expr)) {
            if (varExpr->name != "true" && varExpr->name != "false") {
                if (!symbolTable.isDeclared(varExpr->name)) {
                    error("Cannot use undeclared variable '" + varExpr->name + "'.");
                }
            }
        }
        else if (auto unExpr = dynamic_cast<const UnaryExpr*>(expr)) {
            analyzeExpr(unExpr->right.get());
        }
        else if (auto logExpr = dynamic_cast<const LogicalExpr*>(expr)) {
            analyzeExpr(logExpr->left.get());
            analyzeExpr(logExpr->right.get());
        }
        else if (auto binExpr = dynamic_cast<const BinaryExpr*>(expr)) {
            analyzeExpr(binExpr->left.get());
            analyzeExpr(binExpr->right.get());
        }
    }
};


#endif // ADILANG_SEMANTIC_H