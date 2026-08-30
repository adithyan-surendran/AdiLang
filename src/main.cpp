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

        case TokenType::EQUAL: return "EQUAL";
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

        case TokenType::TRUE: return "TRUE";
        case TokenType::FALSE: return "FALSE";

        case TokenType::LEFT_PAREN: return "LEFT_PAREN";
        case TokenType::RIGHT_PAREN: return "RIGHT_PAREN";
        case TokenType::LEFT_BRACE: return "LEFT_BRACE";
        case TokenType::RIGHT_BRACE: return "RIGHT_BRACE";
        case TokenType::SEMICOLON: return "SEMICOLON";

        case TokenType::END_OF_FILE: return "EOF";
    }

    return "UNKNOWN";
}

int main() {
    std::string source =
        "// 1. While Loop Break Test\n"
        "print(\"=== 1. While Loop Break (Stop at 3) ===\");\n"
        "let a = 1;\n"
        "while (a <= 10) {\n"
        "    if (a == 4) {\n"
        "        break;\n"
        "    }\n"
        "    print(a);\n"
        "    a = a + 1;\n"
        "}\n"
        "\n"
        "// 2. While Loop Continue Test\n"
        "print(\"=== 2. While Loop Continue (Skip 2) ===\");\n"
        "let b = 0;\n"
        "while (b < 4) {\n"
        "    b = b + 1;\n"
        "    if (b == 2) {\n"
        "        continue;\n"
        "    }\n"
        "    print(b);\n"
        "}\n"
        "\n"
        "// 3. For Loop Break Test\n"
        "print(\"=== 3. For Loop Break (Stop at 2) ===\");\n"
        "for (let i = 1; i <= 5; i = i + 1) {\n"
        "    if (i == 3) {\n"
        "        break;\n"
        "    }\n"
        "    print(i);\n"
        "}\n";

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
    std::cout << "    CONTROL FLOW TEST SUITE   \n";
    std::cout << "==============================\n";
    Interpreter interpreter;
    interpreter.interpret(*program);
    std::cout << "==============================\n";

    return 0;
}