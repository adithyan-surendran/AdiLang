#include <iostream>
#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include "interpreter.h"
#include "compiler.h"
#include "vm.h"

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
        case TokenType::STRUCT: return "STRUCT";

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
        case TokenType::DOT: return "DOT";

        case TokenType::END_OF_FILE: return "EOF";
    }

    return "UNKNOWN";
}

int main() {
    std::string source = 
    "// 1. Variables & Arithmetic\n"
    "let x = 10;\n"
    "let y = 20;\n"
    "print(x + y * 2);\n\n"

    "// 2. Control Flow & Loops\n"
    "let count = 0;\n"
    "while (count < 3) {\n"
    "    print(count);\n"
    "    count = count + 1;\n"
    "}\n\n"

    "// 3. Functions & Parameters\n"
    "fn multiply(a, b) {\n"
    "    return a * b;\n"
    "}\n"
    "print(multiply(6, 7));\n\n"

    "// 4. Arrays & Indexing\n"
    "let arr = [10, 20, 30];\n"
    "print(arr[1]);\n"
    "arr[1] = 99;\n"
    "print(arr[1]);\n\n"

    "// 5. Structs & Properties\n"
    "struct Point { x, y }\n"
    "let p = Point(5, 12);\n"
    "print(p.x);\n"
    "p.x = 100;\n"
    "print(p.x);\n";

    Lexer lexer(source);
    auto tokens = lexer.scanTokens();

    Parser parser(tokens);
    auto program = parser.parse();

    std::cout << "==============================\n";
    std::cout << "       ADILANG v0.10.4 RUN     \n";
    std::cout << "==============================\n";

    Chunk chunk;
    Compiler compiler;
    if (!compiler.compile(program.get(), &chunk)) {
        std::cerr << "Compilation failed during bytecode emission.\n";
        return 1;
    }

    VM vm;
    InterpretResult result = vm.interpret(&chunk);

    if (result != InterpretResult::INTERPRET_OK) {
        std::cerr << "Runtime execution failed.\n";
        return 1;
    }

    std::cout << "==============================\n";

    return 0;
}