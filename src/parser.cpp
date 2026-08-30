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

    if (match(TokenType::FOR))
    {
        return forStatement();
    }

    if (match(TokenType::BREAK))
    {
        return breakStatement();
    }   
    if (match(TokenType::CONTINUE))
    {
        return continueStatement();
    }
    if (match(TokenType::FN)) {
        return functionDeclaration();
    }
    if (match(TokenType::RETURN)) {
        return returnStatement();
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
    auto expr = orExpression();

    if (match(TokenType::EQUAL) || match(TokenType::PLUS_EQUAL) ||
        match(TokenType::MINUS_EQUAL) || match(TokenType::STAR_EQUAL) ||
        match(TokenType::SLASH_EQUAL) || match(TokenType::PERCENT_EQUAL))
    {
        Token opToken = previous();
        auto value = assignment();

        // 1. Regular variable assignment: x = value
        if (auto varExpr = dynamic_cast<VariableExpr*>(expr.get()))
        {
            std::string name = varExpr->name;

            if (opToken.type == TokenType::EQUAL) {
                return std::make_unique<AssignExpr>(name, std::move(value));
            }

            TokenType binaryOp;
            if (opToken.type == TokenType::PLUS_EQUAL) binaryOp = TokenType::PLUS;
            else if (opToken.type == TokenType::MINUS_EQUAL) binaryOp = TokenType::MINUS;
            else if (opToken.type == TokenType::STAR_EQUAL) binaryOp = TokenType::STAR;
            else if (opToken.type == TokenType::SLASH_EQUAL) binaryOp = TokenType::SLASH;
            else binaryOp = TokenType::PERCENT;

            auto varNode = std::make_unique<VariableExpr>(name);
            auto desugaredBinary = std::make_unique<BinaryExpr>(std::move(varNode), binaryOp, std::move(value));

            return std::make_unique<AssignExpr>(name, std::move(desugaredBinary));
        }

        // 2. Index assignment: arr[i] = value
        if (auto indexGet = dynamic_cast<IndexGetExpr*>(expr.get()))
        {
            if (opToken.type == TokenType::EQUAL) {
                return std::make_unique<IndexSetExpr>(
                    std::move(indexGet->target),
                    std::move(indexGet->index),
                    std::move(value)
                );
            }

            throw std::runtime_error("Parser Error: Compound assignment on array elements (like +=) not supported yet. Use arr[i] = arr[i] + val.");
        }

        std::cerr << "Parser Error: Invalid assignment target at line " << opToken.line << "\n";
    }

    return expr;
}
std::unique_ptr<Expr> Parser::orExpression()
{
    auto expr = andExpression();

    while (match(TokenType::OR_OR))
    {
        TokenType op = previous().type;
        auto right = andExpression();
        expr = std::make_unique<LogicalExpr>(std::move(expr), op, std::move(right));
    }

    return expr;
}

std::unique_ptr<Expr> Parser::andExpression()
{
    auto expr = equality();

    while (match(TokenType::AND_AND))
    {
        TokenType op = previous().type;
        auto right = equality();
        expr = std::make_unique<LogicalExpr>(std::move(expr), op, std::move(right));
    }

    return expr;
}
std::unique_ptr<Expr> Parser::equality()
{
    auto expr = comparison();

    while (match(TokenType::EQUAL_EQUAL) || match(TokenType::BANG_EQUAL))
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
    auto expr = unary(); 

    while (match(TokenType::STAR) || match(TokenType::SLASH) || match(TokenType::PERCENT))
    {
        TokenType op = previous().type;
        auto right = unary();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
    }

    return expr;
}
std::unique_ptr<Expr> Parser::unary()
{
    if (match(TokenType::BANG) || match(TokenType::MINUS))
    {
        TokenType op = previous().type;
        auto right = unary();
        return std::make_unique<UnaryExpr>(op, std::move(right));
    }

    return call(); 
}
std::unique_ptr<Expr> Parser::primary()
{
    if (match(TokenType::NUMBER))
    {
        double value = std::stod(previous().lexeme);
        return std::make_unique<NumberExpr>(value);
    }

    if (match(TokenType::STRING))
    {
        return std::make_unique<StringExpr>(previous().lexeme);
    }

    if (match(TokenType::IDENTIFIER))
    {
        return std::make_unique<VariableExpr>(previous().lexeme);
    }

    if (match(TokenType::TRUE))
    {
        return std::make_unique<VariableExpr>("true");
    }

    if (match(TokenType::FALSE))
    {
        return std::make_unique<VariableExpr>("false");
    }

    // --- Array Literal: [elem1, elem2, ...] ---
    if (match(TokenType::LEFT_BRACKET)) {
        std::vector<std::unique_ptr<Expr>> elements;
        if (!check(TokenType::RIGHT_BRACKET)) {
            do {
                if (elements.size() >= 255) {
                    throw std::runtime_error("Parser Error: Cannot have more than 255 elements in an array literal.");
                }
                elements.push_back(expression());
            } while (match(TokenType::COMMA));
        }
        consume(TokenType::RIGHT_BRACKET, "Expected ']' after array elements.");
        return std::make_unique<ArrayExpr>(std::move(elements));
    }

    if (match(TokenType::LEFT_PAREN))
    {
        auto expr = expression();
        consume(TokenType::RIGHT_PAREN, "Expected ')'");
        return expr;
    }

    std::cerr << "Parser Error: Expected expression\n";
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
std::unique_ptr<Stmt> Parser::forStatement()
{
    consume(TokenType::LEFT_PAREN, "Expected '(' after 'for'.");

    // 1. Initializer: 'let i = 0;' OR 'i = 0;' OR ';'
    std::unique_ptr<Stmt> initializer = nullptr;
    if (match(TokenType::SEMICOLON)) {
        initializer = nullptr;
    } else if (match(TokenType::LET)) {
        initializer = variableDeclaration();
    } else {
        initializer = expressionStatement();
    }

    // 2. Condition: 'i < 10;' (defaults to 'true' if omitted)
    std::unique_ptr<Expr> condition = nullptr;
    if (!check(TokenType::SEMICOLON)) {
        condition = expression();
    }
    consume(TokenType::SEMICOLON, "Expected ';' after loop condition.");

    // 3. Increment: 'i = i + 1)' (optional)
    std::unique_ptr<Expr> increment = nullptr;
    if (!check(TokenType::RIGHT_PAREN)) {
        increment = expression();
    }
    consume(TokenType::RIGHT_PAREN, "Expected ')' after for clauses.");

    // 4. Body Statement
    auto body = statement();

    // --- Syntactic Desugaring to While Loop AST ---

    // A. If increment exists, attach it to the end of the body in a block
    if (increment != nullptr) {
        std::vector<std::unique_ptr<Stmt>> bodyStmts;
        bodyStmts.push_back(std::move(body));
        bodyStmts.push_back(std::make_unique<ExpressionStatement>(std::move(increment)));
        body = std::make_unique<BlockStatement>(std::move(bodyStmts));
    }

    // B. If condition is omitted (e.g. for (;;)), treat condition as true
    if (condition == nullptr) {
        condition = std::make_unique<VariableExpr>("true");
    }
    body = std::make_unique<WhileStatement>(std::move(condition), std::move(body));

    // C. If initializer exists, wrap both initializer and while loop in an outer scope block
    if (initializer != nullptr) {
        std::vector<std::unique_ptr<Stmt>> blockStmts;
        blockStmts.push_back(std::move(initializer));
        blockStmts.push_back(std::move(body));
        body = std::make_unique<BlockStatement>(std::move(blockStmts));
    }

    return body;
}
std::unique_ptr<Stmt> Parser::breakStatement()
{
    consume(TokenType::SEMICOLON, "Expected ';' after 'break'.");
    return std::make_unique<BreakStatement>();
}

std::unique_ptr<Stmt> Parser::continueStatement()
{
    consume(TokenType::SEMICOLON, "Expected ';' after 'continue'.");
    return std::make_unique<ContinueStatement>();
}
std::unique_ptr<Stmt> Parser::functionDeclaration() {
    if (!match(TokenType::IDENTIFIER)) {
        throw std::runtime_error("Parser Error: Expected function name after 'fn'.");
    }
    std::string name = previous().lexeme;

    if (!match(TokenType::LEFT_PAREN)) {
        throw std::runtime_error("Parser Error: Expected '(' after function name.");
    }

    std::vector<std::string> parameters;
    if (!check(TokenType::RIGHT_PAREN)) {
        do {
            if (parameters.size() >= 255) {
                throw std::runtime_error("Parser Error: Cannot have more than 255 parameters.");
            }
            if (!match(TokenType::IDENTIFIER)) {
                throw std::runtime_error("Parser Error: Expected parameter name.");
            }
            parameters.push_back(previous().lexeme);
        } while (match(TokenType::COMMA));
    }

    if (!match(TokenType::RIGHT_PAREN)) {
        throw std::runtime_error("Parser Error: Expected ')' after parameters.");
    }

    if (!match(TokenType::LEFT_BRACE)) {
        throw std::runtime_error("Parser Error: Expected '{' before function body.");
    }

    // Change blockStatement() to block() (or whatever your block parsing method is named)
    auto bodyStmt = block(); 
    auto body = std::unique_ptr<BlockStatement>(dynamic_cast<BlockStatement*>(bodyStmt.release()));

    return std::make_unique<FunctionStatement>(name, std::move(parameters), std::move(body));
}

std::unique_ptr<Stmt> Parser::returnStatement() {
    Token keyword = previous();
    std::unique_ptr<Expr> value = nullptr;
    if (!check(TokenType::SEMICOLON)) {
        value = expression();
    }

    if (!match(TokenType::SEMICOLON)) {
        throw std::runtime_error("Parser Error: Expected ';' after return value.");
    }

    return std::make_unique<ReturnStatement>(std::move(value));
}
std::unique_ptr<Expr> Parser::call() {
    auto expr = primary();

    while (true) {
        if (match(TokenType::LEFT_PAREN)) {
            expr = finishCall(std::move(expr));
        } 
        else if (match(TokenType::LEFT_BRACKET)) {
            auto index = expression();
            consume(TokenType::RIGHT_BRACKET, "Expected ']' after array index.");
            expr = std::make_unique<IndexGetExpr>(std::move(expr), std::move(index));
        } 
        else {
            break;
        }
    }

    return expr;
}

std::unique_ptr<Expr> Parser::finishCall(std::unique_ptr<Expr> callee) {
    std::vector<std::unique_ptr<Expr>> arguments;
    if (!check(TokenType::RIGHT_PAREN)) {
        do {
            if (arguments.size() >= 255) {
                throw std::runtime_error("Parser Error: Cannot have more than 255 arguments.");
            }
            arguments.push_back(expression());
        } while (match(TokenType::COMMA));
    }

    if (!match(TokenType::RIGHT_PAREN)) {
        throw std::runtime_error("Parser Error: Expected ')' after arguments.");
    }

    return std::make_unique<CallExpr>(std::move(callee), std::move(arguments));
}