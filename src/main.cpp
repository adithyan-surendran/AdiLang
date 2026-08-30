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
        case TokenType::GREATER_EQUAL:return "GREATER_EQUAL";
        case TokenType::LESS_EQUAL:return "LESS_EQUAL";

        case TokenType::PRINT: return "PRINT";
        case TokenType::IF: return "IF";
        case TokenType::ELSE: return "ELSE";
        case TokenType::WHILE: return "WHILE";

        case TokenType::TRUE: return "TRUE";
        case TokenType::FALSE: return "FALSE";

        case TokenType::LEFT_PAREN:return "LEFT_PAREN";
        case TokenType::RIGHT_PAREN:return "RIGHT_PAREN";
        case TokenType::LEFT_BRACE: return "LEFT_BRACE";
        case TokenType::RIGHT_BRACE:return "RIGHT_BRACE";
        case TokenType::SEMICOLON:return "SEMICOLON";

        case TokenType::END_OF_FILE:return "EOF";
    }

    return "UNKNOWN";
}

int main() {
    std::string source =
        "// 1. Basic Boolean & Equality Tests\n"
        "let a = true;\n"
        "let b = false;\n"
        "if (a && !b) {\n"
        "    print(\"Test 1 Passed: a && !b is true\");\n"
        "}\n"
        "\n"
        "// 2. Inequality (!=)\n"
        "let x = 10;\n"
        "let y = 20;\n"
        "if (x != y) {\n"
        "    print(\"Test 2 Passed: 10 != 20\");\n"
        "}\n"
        "\n"
        "// 3. Short-Circuit OR Test\n"
        "// If short-circuiting works, 'sideEffect' will NOT increment\n"
        "let sideEffect = 0;\n"
        "if (true || (sideEffect = sideEffect + 1)) {\n"
        "    print(\"Test 3 Passed: Short-circuit OR triggered\");\n"
        "}\n"
        "print(\"Side effect after OR (expect 0):\");\n"
        "print(sideEffect);\n"
        "\n"
        "// 4. Short-Circuit AND Test\n"
        "// If left side is false, the right-hand assignment must NOT execute\n"
        "if (false && (sideEffect = sideEffect + 1)) {\n"
        "    print(\"Should not print\");\n"
        "}\n"
        "print(\"Side effect after AND (expect 0):\");\n"
        "print(sideEffect);\n"
        "\n"
        "// 5. Complex Nested Logic\n"
        "let age = 25;\n"
        "let hasId = true;\n"
        "let isBanned = false;\n"
        "if ((age >= 21 && hasId) && !isBanned) {\n"
        "    print(\"Test 5 Passed: Complex guard condition met\");\n"
        "}\n";

    // 1. Lexing
    Lexer lexer(source);
    auto tokens = lexer.scanTokens();

    // 2. Parsing
    Parser parser(tokens);
    auto program = parser.parse();

    // 3. Semantic Analysis
    SemanticAnalyzer analyzer;
    if (!analyzer.analyze(*program)) {
        std::cerr << "Semantic check failed.\n";
        return 1;
    }

    // 4. Interpretation
    std::cout << "==============================\n";
    std::cout << "       LOGIC TEST OUTPUT      \n";
    std::cout << "==============================\n";
    Interpreter interpreter;
    interpreter.interpret(*program);
    std::cout << "==============================\n";

    return 0;
}