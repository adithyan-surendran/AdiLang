#ifndef ADILANG_PARSER_H
#define ADILANG_PARSER_H

#include "ast.h"
#include "lexer.h"

#include <vector>
#include <memory>
#include <string>
#include <unordered_set>
#include <iostream>

class Parser {
private:
    std::vector<Token> tokens;
    int current = 0;

    std::unordered_set<std::string> structNames;

    // Error recovery flags
    bool hadError = false;
    bool panicMode = false;

    // Error handling
    void errorAt(const Token& token, const std::string& message);
    void error(const std::string& message);
    void synchronize();

    // Statements
    std::unique_ptr<Stmt> statement();
    std::unique_ptr<Stmt> structDeclaration();
    std::unique_ptr<Stmt> ifStatement();
    std::unique_ptr<Stmt> whileStatement();
    std::unique_ptr<Stmt> forStatement();
    std::unique_ptr<Stmt> breakStatement();
    std::unique_ptr<Stmt> continueStatement();
    std::unique_ptr<Stmt> functionDeclaration();
    std::unique_ptr<Stmt> returnStatement();
    std::unique_ptr<Stmt> block();
    std::unique_ptr<Stmt> variableDeclaration();
    std::unique_ptr<Stmt> printStatement();
    std::unique_ptr<Stmt> expressionStatement();

    // Expressions
    std::unique_ptr<Expr> expression();
    std::unique_ptr<Expr> assignment();
    std::unique_ptr<Expr> orExpression();
    std::unique_ptr<Expr> andExpression();
    std::unique_ptr<Expr> equality();
    std::unique_ptr<Expr> comparison();
    std::unique_ptr<Expr> addition();
    std::unique_ptr<Expr> multiplication();
    std::unique_ptr<Expr> unary();
    std::unique_ptr<Expr> primary();
    std::unique_ptr<Expr> call();
    std::unique_ptr<Expr> finishCall(std::unique_ptr<Expr> callee);

    // Token helpers
    Token advance();
    Token peek();
    Token previous();

    bool check(TokenType type);
    bool match(TokenType type);

    Token consume(
        TokenType type,
        const std::string& message
    );

public:
    explicit Parser(const std::vector<Token>& tokens);

    std::unique_ptr<Program> parse();

    // Returns true if any parser error occurred.
    bool hasError() const;
};

#endif // ADILANG_PARSER_H
