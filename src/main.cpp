#include <iostream>
#include "lexer.h"
#include "parser.h" 

std::string tokenName(TokenType type) {

    switch (type) {

        case TokenType::LET: return "LET";
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::NUMBER: return "NUMBER";
        case TokenType::STRING: return "STRING";

        case TokenType::PLUS: return "PLUS";
        case TokenType::MINUS: return "MINUS";
        case TokenType::STAR: return "STAR";
        case TokenType::SLASH: return "SLASH";

        case TokenType::EQUAL: return "EQUAL";
        case TokenType::EQUAL_EQUAL: return "EQUAL_EQUAL";

        case TokenType::GREATER: return "GREATER";
        case TokenType::LESS: return "LESS";

        case TokenType::GREATER_EQUAL:
            return "GREATER_EQUAL";

        case TokenType::LESS_EQUAL:
            return "LESS_EQUAL";

        case TokenType::PRINT: return "PRINT";
        case TokenType::IF: return "IF";
        case TokenType::ELSE: return "ELSE";

        case TokenType::TRUE: return "TRUE";
        case TokenType::FALSE: return "FALSE";

        case TokenType::LEFT_PAREN:
            return "LEFT_PAREN";

        case TokenType::RIGHT_PAREN:
            return "RIGHT_PAREN";

        case TokenType::LEFT_BRACE:
            return "LEFT_BRACE";

        case TokenType::RIGHT_BRACE:
            return "RIGHT_BRACE";

        case TokenType::SEMICOLON:
            return "SEMICOLON";

        case TokenType::END_OF_FILE:
            return "EOF";
    }

    return "UNKNOWN";
}

int main()
{
    std::string source =
        "let x = 10 + 20 * 2;"
        "print(x);";

    Lexer lexer(source);

    auto tokens = lexer.scanTokens();

    Parser parser(tokens);

    parser.parse();

    std::cout << "Parsing completed!\n";

    return 0;
}