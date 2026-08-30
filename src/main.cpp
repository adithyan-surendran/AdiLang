#include <iostream>
#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include "interpreter.h"

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
        case TokenType::PERCENT: return "PERCENT";

        case TokenType::EQUAL: return "EQUAL";
        case TokenType::PLUS_EQUAL: return "PLUS_EQUAL";
        case TokenType::MINUS_EQUAL: return "MINUS_EQUAL";
        case TokenType::STAR_EQUAL: return "STAR_EQUAL";
        case TokenType::SLASH_EQUAL: return "SLASH_EQUAL";
        case TokenType::PERCENT_EQUAL: return "PERCENT_EQUAL";
        case TokenType::EQUAL_EQUAL: return "EQUAL_EQUAL";
        case TokenType::BANG: return "BANG";
        case TokenType::BANG_EQUAL: return "BANG_EQUAL";
        case TokenType::AND_AND: return "AND_AND";
        case TokenType::OR_OR: return "OR_OR";

        case TokenType::GREATER: return "GREATER";
        case TokenType::LESS: return "LESS";
        case TokenType::GREATER_EQUAL: return "GREATER_EQUAL";
        case TokenType::LESS_EQUAL: return "LESS_EQUAL";

        case TokenType::PRINT: return "PRINT";
        case TokenType::IF: return "IF";
        case TokenType::ELSE: return "ELSE";
        case TokenType::WHILE: return "WHILE";
        case TokenType::FOR: return "FOR";
        case TokenType::BREAK: return "BREAK";
        case TokenType::CONTINUE: return "CONTINUE";
        case TokenType::FN: return "FN";
        case TokenType::RETURN: return "RETURN";

        case TokenType::TRUE: return "TRUE";
        case TokenType::FALSE: return "FALSE";

        case TokenType::LEFT_PAREN: return "LEFT_PAREN";
        case TokenType::RIGHT_PAREN: return "RIGHT_PAREN";
        case TokenType::LEFT_BRACE: return "LEFT_BRACE";
        case TokenType::RIGHT_BRACE: return "RIGHT_BRACE";
        case TokenType::COMMA: return "COMMA";
        case TokenType::SEMICOLON: return "SEMICOLON";

        case TokenType::END_OF_FILE: return "EOF";
    }

    return "UNKNOWN";
}

int main() {
    std::string source =
        "print(\"=== v0.7.0 User-Defined Functions Test ===\");\n"
        "\n"
        "fn add(a, b) {\n"
        "    return a + b;\n"
        "}\n"
        "\n"
        "fn factorial(n) {\n"
        "    if (n <= 1) {\n"
        "        return 1;\n"
        "    }\n"
        "    return n * factorial(n - 1);\n"
        "}\n"
        "\n"
        "print(\"Result of add(10, 32):\");\n"
        "print(add(10, 32));\n"
        "\n"
        "print(\"Result of factorial(5) (expect 120):\");\n"
        "print(factorial(5));\n";

    Lexer lexer(source);
    auto tokens = lexer.scanTokens();

    Parser parser(tokens);
    auto program = parser.parse();

    SemanticAnalyzer analyzer;
    if (!analyzer.analyze(*program)) {
        std::cerr << "Semantic check failed.\n";
        return 1;
    }

    std::cout << "==============================\n";
    std::cout << "       ADILANG v0.7.0 RUN     \n";
    std::cout << "==============================\n";
    Interpreter interpreter;
    interpreter.interpret(*program);
    std::cout << "==============================\n";

    return 0;
}