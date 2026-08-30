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

        std::unique_ptr<Stmt> statement();
        std::unique_ptr<Stmt> variableDeclaration();
        std::unique_ptr<Stmt> printStatement();
        std::unique_ptr<Stmt> expressionStatement();

        std::unique_ptr<Expr> expression();
        std::unique_ptr<Expr> addition();
        std::unique_ptr<Expr> multiplication();
        std::unique_ptr<Expr> primary(); 

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

    #endif