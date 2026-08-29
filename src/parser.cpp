#include "parser.h"
#include <iostream>

Parser::Parser(const std::vector<Token>& tokens)
    : tokens(tokens)
{
} 
Token Parser::peek()
{
    return tokens[current];
}
Token Parser::previous()
{
    return tokens[current - 1];
}
Token Parser::advance()
{
    if (!check(TokenType::END_OF_FILE))
        current++;

    return previous();
}
bool Parser::check(TokenType type)
{
    if (peek().type == TokenType::END_OF_FILE)
        return type == TokenType::END_OF_FILE;

    return peek().type == type;
}
bool Parser::match(TokenType type)
{
    if (!check(type))
        return false;

    advance();
    return true;
}
Token Parser::consume(
    TokenType type,
    const std::string& message)
{
    if (check(type))
        return advance();

    std::cerr
        << "Parser Error: "
        << message
        << " at line "
        << peek().line
        << '\n';

    return peek();
}
void Parser::parse()
{
    while (!check(TokenType::END_OF_FILE))
    {
        statement();
    }
}
void Parser::statement()
{
    if (match(TokenType::LET))
    {
        variableDeclaration();
        return;
    }

    if (match(TokenType::PRINT))
    {
        printStatement();
        return;
    }

    expressionStatement();
}
void Parser::variableDeclaration()
{
    consume(
        TokenType::IDENTIFIER,
        "Expected variable name"
    );

    consume(
        TokenType::EQUAL,
        "Expected '=' after variable name"
    );

    expression();

    consume(
        TokenType::SEMICOLON,
        "Expected ';' after variable declaration"
    );
}
void Parser::expression()
{
    addition();
}
void Parser::addition()
{
    multiplication();

    while (
        match(TokenType::PLUS) ||
        match(TokenType::MINUS)
    )
    {
        multiplication();
    }
}
void Parser::multiplication()
{
    primary();

    while (
        match(TokenType::STAR) ||
        match(TokenType::SLASH)
    )
    {
        primary();
    }
}
void Parser::primary()
{
    if (match(TokenType::NUMBER))
        return;

    if (match(TokenType::STRING))
        return;

    if (match(TokenType::IDENTIFIER))
        return;

    if (match(TokenType::TRUE))
        return;

    if (match(TokenType::FALSE))
        return;

    if (match(TokenType::LEFT_PAREN))
    {
        expression();

        consume(
            TokenType::RIGHT_PAREN,
            "Expected ')'"
        );

        return;
    }

    std::cerr
        << "Parser Error: Expected expression\n";
}
void Parser::printStatement()
{
    consume(
        TokenType::LEFT_PAREN,
        "Expected '(' after print"
    );

    expression();

    consume(
        TokenType::RIGHT_PAREN,
        "Expected ')' after expression"
    );

    consume(
        TokenType::SEMICOLON,
        "Expected ';' after print statement"
    );
}
void Parser::expressionStatement()
{
    expression();

    consume(
        TokenType::SEMICOLON,
        "Expected ';' after expression"
    );
}   