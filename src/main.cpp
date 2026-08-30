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
        case TokenType::LEFT_BRACKET: return "LEFT_BRACKET";
        case TokenType::RIGHT_BRACKET: return "RIGHT_BRACKET";
        case TokenType::COMMA: return "COMMA";
        case TokenType::SEMICOLON: return "SEMICOLON";

        case TokenType::END_OF_FILE: return "EOF";
    }

    return "UNKNOWN";
}

int main() {
    std::string source =
        "print(\"=== v0.8.0 Arrays & Collections Test ===\");\n"
        "\n"
        "// 1. Define an array literal\n"
        "let numbers = [10, 20, 30, 40, 50];\n"
        "print(\"Initial array:\");\n"
        "print(numbers);\n"
        "\n"
        "// 2. Index access\n"
        "print(\"Element at index 0:\");\n"
        "print(numbers[0]);\n"
        "\n"
        "print(\"Element at index 2:\");\n"
        "print(numbers[2]);\n"
        "\n"
        "// 3. Item mutation (assignment by index)\n"
        "numbers[1] = 99;\n"
        "print(\"Array after modifying index 1 to 99:\");\n"
        "print(numbers);\n"
        "\n"
        "// 4. Using variables for indices and expressions inside arrays\n"
        "let i = 3;\n"
        "print(\n"
        "    numbers[i]\n"
        ");\n"
        "\n"
        "// 5. Nested / Mixed arrays\n"
        "let mixed = [\"hello\", 3.14, true];\n"
        "print(\"Mixed type array:\");\n"
        "print(mixed);\n";

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
    std::cout << "       ADILANG v0.8.0 RUN     \n";
    std::cout << "==============================\n";
    Interpreter interpreter;
    interpreter.interpret(*program);
    std::cout << "==============================\n";

    return 0;
}