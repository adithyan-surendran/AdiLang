#ifndef ADILANG_PARSER_H
#define ADILANG_PARSER_H

#include "ast.h"
#include "lexer.h"
#include <vector> 
#include <memory>
#include <string>

class Parser {
private:
    std::vector<Token> tokens;
    int current = 0;

    // Statements
    std::unique_ptr<Stmt> statement();
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

    // Expressions (Precedence Hierarchy)
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

    // Helper Token Methods
    Token advance();
    Token peek();
    Token previous();

    bool check(TokenType type);
    bool match(TokenType type);

    Token consume(TokenType type, const std::string& message);

public:
    explicit Parser(const std::vector<Token>& tokens);

    std::unique_ptr<Program> parse();
};

#endif // ADILANG_PARSER_H