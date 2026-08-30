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
std::unique_ptr<Program> Parser::parse()
{
    auto program = std::make_unique<Program>();

    while (!check(TokenType::END_OF_FILE))
    {
        auto stmt = statement();
        if (stmt)
        {
            program->statements.push_back(std::move(stmt));
        }
    }

    return program;
}
std::unique_ptr<Stmt> Parser::statement()
{
    if (match(TokenType::IF))
    {
        return ifStatement();
    }

    if (match(TokenType::WHILE))       
    {                                  
        return whileStatement();       
    }

    if (match(TokenType::LEFT_BRACE))
    {
        return block();
    }

    if (match(TokenType::LET))
    {
        return variableDeclaration();
    }

    if (match(TokenType::PRINT))
    {
        return printStatement();
    }

    return expressionStatement();
}

std::unique_ptr<Stmt> Parser::ifStatement()
{
    consume(TokenType::LEFT_PAREN, "Expected '(' after 'if'.");
    auto condition = expression();
    consume(TokenType::RIGHT_PAREN, "Expected ')' after if condition.");

    auto thenBranch = statement();
    std::unique_ptr<Stmt> elseBranch = nullptr;

    if (match(TokenType::ELSE))
    {
        elseBranch = statement();
    }

    return std::make_unique<IfStatement>(
        std::move(condition),
        std::move(thenBranch),
        std::move(elseBranch)
    );
}

std::unique_ptr<Stmt> Parser::block()
{
    std::vector<std::unique_ptr<Stmt>> statements;

    while (!check(TokenType::RIGHT_BRACE) && !check(TokenType::END_OF_FILE))
    {
        statements.push_back(statement());
    }

    consume(TokenType::RIGHT_BRACE, "Expected '}' after block.");
    return std::make_unique<BlockStatement>(std::move(statements));
}
std::unique_ptr<Stmt> Parser::variableDeclaration()
{
    Token nameToken = consume(
        TokenType::IDENTIFIER,
        "Expected variable name"
    );

    consume(
        TokenType::EQUAL,
        "Expected '=' after variable name"
    );

    auto initializer = expression();

    consume(
        TokenType::SEMICOLON,
        "Expected ';' after variable declaration"
    );

    return std::make_unique<VariableDeclaration>(
        nameToken.lexeme,
        std::move(initializer)
    );
}
std::unique_ptr<Expr> Parser::expression()
{
    return assignment();
}

std::unique_ptr<Expr> Parser::assignment()
{
    auto expr = equality();

    if (match(TokenType::EQUAL))
    {
        Token equals = previous();
        auto value = assignment(); // Right-associative (e.g. a = b = 5)

        // Check if the left-hand side is a valid target (a variable)
        if (auto varExpr = dynamic_cast<VariableExpr*>(expr.get()))
        {
            std::string name = varExpr->name;
            return std::make_unique<AssignExpr>(name, std::move(value));
        }

        std::cerr << "Parser Error: Invalid assignment target at line " 
                  << equals.line << "\n";
    }

    return expr;
}

std::unique_ptr<Expr> Parser::equality()
{
    auto expr = comparison();

    while (match(TokenType::EQUAL_EQUAL))
    {
        TokenType op = previous().type;
        auto right = comparison();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
    }

    return expr;
}

std::unique_ptr<Expr> Parser::comparison()
{
    auto expr = addition();

    while (
        match(TokenType::GREATER) ||
        match(TokenType::GREATER_EQUAL) ||
        match(TokenType::LESS) ||
        match(TokenType::LESS_EQUAL)
    )
    {
        TokenType op = previous().type;
        auto right = addition();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
    }

    return expr;
}
std::unique_ptr<Expr> Parser::addition()
{
    auto expr = multiplication();

    while (
        match(TokenType::PLUS) ||
        match(TokenType::MINUS)
    )
    {
        TokenType op = previous().type;

        auto right = multiplication();

        expr = std::make_unique<BinaryExpr>(
            std::move(expr),
            op,
            std::move(right)
        );
    }

    return expr;
}
std::unique_ptr<Expr> Parser::multiplication()
{
    auto expr = primary();

    while (
        match(TokenType::STAR) ||
        match(TokenType::SLASH)
    )
    {
        TokenType op = previous().type;

        auto right = primary();

        expr = std::make_unique<BinaryExpr>(
            std::move(expr),
            op,
            std::move(right)
        );
    }

    return expr;
}
std::unique_ptr<Expr> Parser::primary()
{
    if (match(TokenType::NUMBER))
    {
        double value =
            std::stod(previous().lexeme);

        return std::make_unique<NumberExpr>(value);
    }

    if (match(TokenType::STRING))
    {
        return std::make_unique<StringExpr>(
            previous().lexeme
        );
    }

    if (match(TokenType::IDENTIFIER))
    {
        return std::make_unique<VariableExpr>(
            previous().lexeme
        );
    }

    if (match(TokenType::TRUE))
    {
        return std::make_unique<VariableExpr>("true");
    }

    if (match(TokenType::FALSE))
    {
        return std::make_unique<VariableExpr>("false");
    }

    if (match(TokenType::LEFT_PAREN))
    {
        auto expr = expression();

        consume(
            TokenType::RIGHT_PAREN,
            "Expected ')'"
        );

        return expr;
    }

    std::cerr
        << "Parser Error: Expected expression\n";

    return nullptr;
}
std::unique_ptr<Stmt> Parser::printStatement()
{
    consume(
        TokenType::LEFT_PAREN,
        "Expected '(' after print"
    );

    auto expr = expression();

    consume(
        TokenType::RIGHT_PAREN,
        "Expected ')' after expression"
    );

    consume(
        TokenType::SEMICOLON,
        "Expected ';' after print statement"
    );

    return std::make_unique<PrintStatement>(std::move(expr));
}
std::unique_ptr<Stmt> Parser::expressionStatement()
{
    auto expr = expression();

    consume(
        TokenType::SEMICOLON,
        "Expected ';' after expression"
    );

    return std::make_unique<ExpressionStatement>(std::move(expr));
}
std::unique_ptr<Stmt> Parser::whileStatement()
{
    consume(TokenType::LEFT_PAREN, "Expected '(' after 'while'.");
    auto condition = expression();
    consume(TokenType::RIGHT_PAREN, "Expected ')' after while condition.");

    auto body = statement();

    return std::make_unique<WhileStatement>(std::move(condition), std::move(body));
}