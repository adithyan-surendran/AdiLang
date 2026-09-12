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

    error(message);
    return peek();
}

void Parser::errorAt(const Token& token, const std::string& message) {
    if (panicMode) return;
    panicMode = true;

    std::cerr << "Parser Error at line " << token.line << ", column " << token.column;
    if (token.type == TokenType::END_OF_FILE) {
        std::cerr << " at end";
    } else {
        std::cerr << " at '" << token.lexeme << "'";
    }
    std::cerr << ": " << message << "\n";
    hadError = true;
}

void Parser::error(const std::string& message) {
    errorAt(previous(), message);
}

void Parser::synchronize() {
    panicMode = false;

    while (peek().type != TokenType::END_OF_FILE) {
        if (previous().type == TokenType::SEMICOLON) return;

        switch (peek().type) {
            case TokenType::CLASS:
            case TokenType::STRUCT:
            case TokenType::FN:
            case TokenType::LET:
            case TokenType::FOR:
            case TokenType::WHILE:
            case TokenType::IF:
            case TokenType::PRINT:
            case TokenType::RETURN:
                return;
            default:
                break;
        }

        advance();
    }
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
    if (panicMode) {
        synchronize();
    }

    if (match(TokenType::AT)) {
        int line = previous().line;
        consume(TokenType::IMPORT, "Expect 'import' after '@'.");
        Token moduleNameToken = consume(TokenType::IDENTIFIER, "Expect module name after '@import'.");
        consume(TokenType::SEMICOLON, "Expect ';' after import statement.");
        auto stmt = std::make_unique<ImportStmt>(moduleNameToken.lexeme);
        stmt->line = line;
        return stmt;
    }
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
        int line = previous().line;
        consume(TokenType::SEMICOLON, "Expected ';' after 'break'.");
        auto stmt = std::make_unique<BreakStatement>();
        stmt->line = line;
        return stmt;
    }   
    if (match(TokenType::CONTINUE))
    {
        int line = previous().line;
        consume(TokenType::SEMICOLON, "Expected ';' after 'continue'.");
        auto stmt = std::make_unique<ContinueStatement>();
        stmt->line = line;
        return stmt;
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

    if (match(TokenType::STRUCT) || match(TokenType::CLASS)) {
        return structDeclaration();
    }

    return expressionStatement();
}

std::unique_ptr<Stmt> Parser::ifStatement()
{
    int line = previous().line;
    consume(TokenType::LEFT_PAREN, "Expected '(' after 'if'.");
    auto condition = expression();
    consume(TokenType::RIGHT_PAREN, "Expected ')' after if condition.");

    auto thenBranch = statement();
    std::unique_ptr<Stmt> elseBranch = nullptr;

    if (match(TokenType::ELSE))
    {
        elseBranch = statement();
    }

    auto stmt = std::make_unique<IfStatement>(
        std::move(condition),
        std::move(thenBranch),
        std::move(elseBranch)
    );
    stmt->line = line;
    return stmt;
}

std::unique_ptr<Stmt> Parser::block()
{
    int line = previous().line;
    std::vector<std::unique_ptr<Stmt>> statements;

    while (!check(TokenType::RIGHT_BRACE) && !check(TokenType::END_OF_FILE))
    {
        statements.push_back(statement());
    }

    consume(TokenType::RIGHT_BRACE, "Expected '}' after block.");
    auto stmt = std::make_unique<BlockStatement>(std::move(statements));
    stmt->line = line;
    return stmt;
}

std::unique_ptr<Stmt> Parser::variableDeclaration()
{
    int line = previous().line;
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

    auto stmt = std::make_unique<VariableDeclaration>(
        nameToken.lexeme,
        std::move(initializer)
    );
    stmt->line = line;
    return stmt;
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
        int line = opToken.line;
        auto value = assignment();

        if (auto varExpr = dynamic_cast<VariableExpr*>(expr.get()))
        {
            std::string name = varExpr->name;

            if (opToken.type == TokenType::EQUAL) {
                auto assignExpr = std::make_unique<AssignExpr>(name, std::move(value));
                assignExpr->line = line;
                return assignExpr;
            }

            TokenType binaryOp;
            if (opToken.type == TokenType::PLUS_EQUAL) binaryOp = TokenType::PLUS;
            else if (opToken.type == TokenType::MINUS_EQUAL) binaryOp = TokenType::MINUS;
            else if (opToken.type == TokenType::STAR_EQUAL) binaryOp = TokenType::STAR;
            else if (opToken.type == TokenType::SLASH_EQUAL) binaryOp = TokenType::SLASH;
            else binaryOp = TokenType::PERCENT;

            auto varNode = std::make_unique<VariableExpr>(name);
            varNode->line = line;
            auto desugaredBinary = std::make_unique<BinaryExpr>(std::move(varNode), binaryOp, std::move(value));
            desugaredBinary->line = line;

            auto assignExpr = std::make_unique<AssignExpr>(name, std::move(desugaredBinary));
            assignExpr->line = line;
            return assignExpr;
        }

        if (auto indexGet = dynamic_cast<IndexGetExpr*>(expr.get()))
        {
            if (opToken.type == TokenType::EQUAL) {
                auto indexSet = std::make_unique<IndexSetExpr>(
                    std::move(indexGet->target),
                    std::move(indexGet->index),
                    std::move(value)
                );
                indexSet->line = line;
                return indexSet;
            }

            throw std::runtime_error("Parser Error: Compound assignment on array elements (like +=) not supported yet.");
        }

        if (auto getExpr = dynamic_cast<GetExpr*>(expr.get()))
        {
            if (opToken.type == TokenType::EQUAL) {
                auto setExpr = std::make_unique<SetExpr>(
                    std::move(getExpr->object),
                    getExpr->name,
                    std::move(value)
                );
                setExpr->line = line;
                return setExpr;
            }
            throw std::runtime_error("Parser Error: Compound assignment on object properties not supported.");
        }

        error("Invalid assignment target.");
    }

    return expr;
}

std::unique_ptr<Expr> Parser::orExpression()
{
    auto expr = andExpression();

    while (match(TokenType::OR_OR))
    {
        Token opToken = previous();
        int line = opToken.line;
        TokenType op = opToken.type;
        auto right = andExpression();
        expr = std::make_unique<LogicalExpr>(std::move(expr), op, std::move(right));
        expr->line = line;
    }

    return expr;
}

std::unique_ptr<Expr> Parser::andExpression()
{
    auto expr = equality();

    while (match(TokenType::AND_AND))
    {
        Token opToken = previous();
        int line = opToken.line;
        TokenType op = opToken.type;
        auto right = equality();
        expr = std::make_unique<LogicalExpr>(std::move(expr), op, std::move(right));
        expr->line = line;
    }

    return expr;
}

std::unique_ptr<Expr> Parser::equality()
{
    auto expr = comparison();

    while (match(TokenType::EQUAL_EQUAL) || match(TokenType::BANG_EQUAL))
    {
        Token opToken = previous();
        int line = opToken.line;
        TokenType op = opToken.type;
        auto right = comparison();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
        expr->line = line;
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
        Token opToken = previous();
        int line = opToken.line;
        TokenType op = opToken.type;
        auto right = addition();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
        expr->line = line;
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
        Token opToken = previous();
        int line = opToken.line;
        TokenType op = opToken.type;
        auto right = multiplication();
        expr = std::make_unique<BinaryExpr>(
            std::move(expr),
            op,
            std::move(right)
        );
        expr->line = line;
    }

    return expr;
}

std::unique_ptr<Expr> Parser::multiplication()
{
    auto expr = unary(); 

    while (match(TokenType::STAR) || match(TokenType::SLASH) || match(TokenType::PERCENT))
    {
        Token opToken = previous();
        int line = opToken.line;
        TokenType op = opToken.type;
        auto right = unary();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
        expr->line = line;
    }

    return expr;
}

std::unique_ptr<Expr> Parser::unary()
{
    if (match(TokenType::BANG) || match(TokenType::MINUS))
    {
        Token opToken = previous();
        int line = opToken.line;
        TokenType op = opToken.type;
        auto right = unary();
        auto expr = std::make_unique<UnaryExpr>(op, std::move(right));
        expr->line = line;
        return expr;
    }

    return call(); 
}

std::unique_ptr<Expr> Parser::primary()
{
    if (match(TokenType::NUMBER))
    {
        int line = previous().line;
        double value = std::stod(previous().lexeme);
        auto expr = std::make_unique<NumberExpr>(value);
        expr->line = line;
        return expr;
    }

    if (match(TokenType::STRING))
    {
        int line = previous().line;
        auto expr = std::make_unique<StringExpr>(previous().lexeme);
        expr->line = line;
        return expr;
    }

    if (match(TokenType::IDENTIFIER))
    {
        int line = previous().line;
        auto expr = std::make_unique<VariableExpr>(previous().lexeme);
        expr->line = line;
        return expr;
    }

    if (match(TokenType::TRUE))
    {
        int line = previous().line;
        auto expr = std::make_unique<VariableExpr>("true");
        expr->line = line;
        return expr;
    }

    if (match(TokenType::FALSE))
    {
        int line = previous().line;
        auto expr = std::make_unique<VariableExpr>("false");
        expr->line = line;
        return expr;
    }

    if (match(TokenType::THIS))
    {
        int line = previous().line;
        auto expr = std::make_unique<VariableExpr>("this");
        expr->line = line;
        return expr;
    }

    if (match(TokenType::LEFT_BRACKET)) {
        int line = previous().line;
        std::vector<std::unique_ptr<Expr>> elements;
        if (!check(TokenType::RIGHT_BRACKET)) {
            do {
                if (elements.size() >= 255) {
                    error("Cannot have more than 255 elements in an array literal.");
                }
                elements.push_back(assignment());
            } while (match(TokenType::COMMA));
        }
        consume(TokenType::RIGHT_BRACKET, "Expected ']' after array elements.");
        auto expr = std::make_unique<ArrayExpr>(std::move(elements));
        expr->line = line;
        return expr;
    }

    if (match(TokenType::LEFT_PAREN))
    {
        auto expr = assignment();
        consume(TokenType::RIGHT_PAREN, "Expected ')'");
        return expr;
    }

    if (match(TokenType::SUPER)) {
        int line = previous().line;
        consume(TokenType::DOT, "Expected '.' after 'super'.");
        Token methodToken = consume(TokenType::IDENTIFIER, "Expected superclass method name.");
        std::string methodName = methodToken.lexeme;

        std::vector<std::unique_ptr<Expr>> arguments;
        if (match(TokenType::LEFT_PAREN)) {
            if (!check(TokenType::RIGHT_PAREN)) {
                do {
                    if (arguments.size() >= 255) {
                        error("Cannot have more than 255 arguments.");
                    }
                    arguments.push_back(assignment());
                } while (match(TokenType::COMMA));
            }
            consume(TokenType::RIGHT_PAREN, "Expected ')' after arguments.");
        }

        auto expr = std::make_unique<SuperExpr>(methodName, std::move(arguments));
        expr->line = line;
        return expr;
    }

    error("Expected expression.");
    return nullptr;
}

std::unique_ptr<Stmt> Parser::printStatement()
{
    int line = previous().line;
    bool hasParen = match(TokenType::LEFT_PAREN);
    auto expr = expression();

    if (hasParen) {
        consume(
            TokenType::RIGHT_PAREN,
            "Expected ')' after expression"
        );
    }

    consume(
        TokenType::SEMICOLON,
        "Expected ';' after print statement"
    );

    auto stmt = std::make_unique<PrintStatement>(std::move(expr));
    stmt->line = line;
    return stmt;
}

std::unique_ptr<Stmt> Parser::expressionStatement()
{
    int line = peek().line;
    auto expr = expression();

    consume(
        TokenType::SEMICOLON,
        "Expected ';' after expression"
    );

    auto stmt = std::make_unique<ExpressionStatement>(std::move(expr));
    stmt->line = line;
    return stmt;
}

std::unique_ptr<Stmt> Parser::whileStatement()
{
    int line = previous().line;
    consume(TokenType::LEFT_PAREN, "Expected '(' after 'while'.");
    auto condition = expression();
    consume(TokenType::RIGHT_PAREN, "Expected ')' after while condition.");

    auto body = statement();

    auto stmt = std::make_unique<WhileStatement>(std::move(condition), std::move(body));
    stmt->line = line;
    return stmt;
}

std::unique_ptr<Stmt> Parser::forStatement()
{
    int line = previous().line;
    consume(TokenType::LEFT_PAREN, "Expected '(' after 'for'.");

    std::unique_ptr<Stmt> initializer = nullptr;
    if (match(TokenType::SEMICOLON)) {
        initializer = nullptr;
    } else if (match(TokenType::LET)) {
        initializer = variableDeclaration();
    } else {
        initializer = expressionStatement();
    }

    std::unique_ptr<Expr> condition = nullptr;
    if (!check(TokenType::SEMICOLON)) {
        condition = expression();
    }
    consume(TokenType::SEMICOLON, "Expected ';' after loop condition.");

    std::unique_ptr<Expr> increment = nullptr;
    if (!check(TokenType::RIGHT_PAREN)) {
        increment = expression();
    }
    consume(TokenType::RIGHT_PAREN, "Expected ')' after for clauses.");

    auto body = statement();

    if (increment != nullptr) {
        std::vector<std::unique_ptr<Stmt>> bodyStmts;
        bodyStmts.push_back(std::move(body));
        bodyStmts.push_back(std::make_unique<ExpressionStatement>(std::move(increment)));
        body = std::make_unique<BlockStatement>(std::move(bodyStmts));
    }

    if (condition == nullptr) {
        condition = std::make_unique<VariableExpr>("true");
    }
    body = std::make_unique<WhileStatement>(std::move(condition), std::move(body));

    if (initializer != nullptr) {
        std::vector<std::unique_ptr<Stmt>> blockStmts;
        blockStmts.push_back(std::move(initializer));
        blockStmts.push_back(std::move(body));
        body = std::make_unique<BlockStatement>(std::move(blockStmts));
    }

    body->line = line;
    return body;
}

std::unique_ptr<Stmt> Parser::breakStatement()
{
    int line = previous().line;
    consume(TokenType::SEMICOLON, "Expected ';' after 'break'.");
    auto stmt = std::make_unique<BreakStatement>();
    stmt->line = line;
    return stmt;
}

std::unique_ptr<Stmt> Parser::continueStatement()
{
    int line = previous().line;
    consume(TokenType::SEMICOLON, "Expected ';' after 'continue'.");
    auto stmt = std::make_unique<ContinueStatement>();
    stmt->line = line;
    return stmt;
}

std::unique_ptr<Stmt> Parser::functionDeclaration() {
    int line = previous().line;
    Token nameToken = consume(TokenType::IDENTIFIER, "Expected function name after 'fn'.");
    std::string name = nameToken.lexeme;

    consume(TokenType::LEFT_PAREN, "Expected '(' after function name.");

    std::vector<std::string> parameters;
    if (!check(TokenType::RIGHT_PAREN)) {
        do {
            if (parameters.size() >= 255) {
                error("Cannot have more than 255 parameters.");
            }
            Token paramToken = consume(TokenType::IDENTIFIER, "Expected parameter name.");
            parameters.push_back(paramToken.lexeme);
        } while (match(TokenType::COMMA));
    }

    consume(TokenType::RIGHT_PAREN, "Expected ')' after parameters.");
    consume(TokenType::LEFT_BRACE, "Expected '{' before function body.");

    auto bodyStmt = block(); 
    auto body = std::unique_ptr<BlockStatement>(dynamic_cast<BlockStatement*>(bodyStmt.release()));

    auto stmt = std::make_unique<FunctionStatement>(name, std::vector<std::string>{parameters}, std::move(body));
    stmt->line = line;
    return stmt;
}

std::unique_ptr<Stmt> Parser::returnStatement() {
    int line = previous().line;
    std::unique_ptr<Expr> value = nullptr;
    if (!check(TokenType::SEMICOLON)) {
        value = expression();
    }

    consume(TokenType::SEMICOLON, "Expected ';' after return value.");

    auto stmt = std::make_unique<ReturnStatement>(std::move(value));
    stmt->line = line;
    return stmt;
}

std::unique_ptr<Expr> Parser::call() {
    auto expr = primary();

    while (true) {
        if (match(TokenType::LEFT_PAREN)) {
            int line = previous().line;
            if (auto varExpr = dynamic_cast<VariableExpr*>(expr.get())) {
                std::string structName = varExpr->name;
                if (structNames.find(structName) != structNames.end()) {
                    std::vector<std::unique_ptr<Expr>> arguments;
                    if (!check(TokenType::RIGHT_PAREN)) {
                        do {
                            arguments.push_back(assignment());
                        } while (match(TokenType::COMMA));
                    }
                    consume(TokenType::RIGHT_PAREN, "Expected ')' after arguments.");
                    auto structInst = std::make_unique<StructInstanceExpr>(structName, std::move(arguments));
                    structInst->line = line;
                    expr = std::move(structInst);
                } else {
                    expr = finishCall(std::move(expr));
                    expr->line = line;
                }
            } else {
                expr = finishCall(std::move(expr));
                expr->line = line;
            }
        } 
        else if (match(TokenType::LEFT_BRACKET)) {
            int line = previous().line;
            auto index = assignment();
            consume(TokenType::RIGHT_BRACKET, "Expected ']' after array index.");
            auto indexGet = std::make_unique<IndexGetExpr>(std::move(expr), std::move(index));
            indexGet->line = line;
            expr = std::move(indexGet);
        }
        else if (match(TokenType::DOT)) {
            int line = previous().line;
            Token name = consume(TokenType::IDENTIFIER, "Expected property name after '.'.");
            auto getExpr = std::make_unique<GetExpr>(std::move(expr), name.lexeme);
            getExpr->line = line;
            expr = std::move(getExpr);
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
                error("Cannot have more than 255 arguments.");
            }
            arguments.push_back(assignment());
        } while (match(TokenType::COMMA));
    }

    consume(TokenType::RIGHT_PAREN, "Expected ')' after arguments.");

    return std::make_unique<CallExpr>(std::move(callee), std::move(arguments));
}

std::unique_ptr<Stmt> Parser::structDeclaration() {
    int line = previous().line;
    Token nameToken = consume(TokenType::IDENTIFIER, "Expected struct name.");
    std::string name = nameToken.lexeme;
    structNames.insert(name);

    std::optional<std::string> superclass = std::nullopt;
    if (match(TokenType::LESS)) {
        Token superclassToken = consume(TokenType::IDENTIFIER, "Expected superclass name after '<'.");
        superclass = superclassToken.lexeme;
    }

    consume(TokenType::LEFT_BRACE, "Expected '{' before struct body.");

    std::vector<std::string> fields;
    std::vector<std::shared_ptr<FunctionStatement>> methods;

    if (!check(TokenType::RIGHT_BRACE)) {
        do {
            Token identifierToken = consume(TokenType::IDENTIFIER, "Expected field or method name.");
            
            if (match(TokenType::LEFT_PAREN)) {
                std::string methodName = identifierToken.lexeme;
                std::vector<std::string> parameters;
                
                if (!check(TokenType::RIGHT_PAREN)) {
                    do {
                        if (parameters.size() >= 255) {
                            error("Cannot have more than 255 parameters.");
                        }
                        Token paramToken = consume(TokenType::IDENTIFIER, "Expected parameter name.");
                        parameters.push_back(paramToken.lexeme);
                    } while (match(TokenType::COMMA));
                }
                
                consume(TokenType::RIGHT_PAREN, "Expected ')' after parameters.");
                consume(TokenType::LEFT_BRACE, "Expected '{' before method body.");
                
                auto bodyStmt = block(); 
                auto body = std::unique_ptr<BlockStatement>(dynamic_cast<BlockStatement*>(bodyStmt.release()));

                methods.push_back(std::make_shared<FunctionStatement>(methodName, std::move(parameters), std::move(body)));
            } else {
                fields.push_back(identifierToken.lexeme);
            }

            match(TokenType::COMMA);
            match(TokenType::SEMICOLON);

        } while (!check(TokenType::RIGHT_BRACE) && !check(TokenType::END_OF_FILE));
    }

    consume(TokenType::RIGHT_BRACE, "Expected '}' after struct body.");
    match(TokenType::SEMICOLON);

    auto stmt = std::make_unique<StructStmt>(name, std::move(superclass), std::move(fields), std::move(methods));
    stmt->line = line;
    return stmt;
}