#ifndef ADILANG_PARSER_H
#define ADILANG_PARSER_H

#include "lexer.h"
#include <vector>

class Parser {
private:
    std::vector<Token> tokens;
    int current = 0;

    void statement();
    void variableDeclaration();
    void printStatement();
    void expressionStatement();

    void expression();
    void addition();
    void multiplication();
    void primary();

    Token advance();
    Token peek();
    Token previous();

    bool check(TokenType type);
    bool match(TokenType type);

    Token consume(TokenType type, const std::string& message);

public:
    Parser(const std::vector<Token>& tokens);

    void parse();
};

#endif