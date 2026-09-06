#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <memory>

#include "object.h"
#include "lexer.h"
#include "parser.h"
#include "compiler.h"
#include "vm.h"

VM vm;


void runFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Could not open file: " << path << "\n";
        exit(74);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();

    // 1. Lex
    Lexer lexer(source);
    auto tokens = lexer.scanTokens(); // Use tokenize() if that is your lexer's method name

    // 2. Parse
    Parser parser(tokens);
    auto program = parser.parse();

    // 3. Compile
    Chunk chunk;
    Compiler compiler(&vm); 
    if (!compiler.compile(program.get(), &chunk)) {
        std::cerr << "Compilation failed during bytecode emission.\n";
        exit(65);
    }

    // 4. Interpret
    VM vm;
    InterpretResult result = vm.interpret(&chunk); // Use &chunk if your VM signature takes a pointer

    if (result == InterpretResult::INTERPRET_COMPILE_ERROR) exit(65);
    if (result == InterpretResult::INTERPRET_RUNTIME_ERROR) exit(70);
}

void runPrompt() {
    std::string line;
    VM vm;
    std::cout << "AdiLang v0.11.0 REPL\n";
    for (;;) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) {
            std::cout << "\n";
            break;
        }

        Lexer lexer(line);
        auto tokens = lexer.scanTokens();

        Parser parser(tokens);
        auto program = parser.parse();

        Chunk chunk;
        Compiler compiler(&vm); 
        if (!compiler.compile(program.get(), &chunk)) {
            continue;
        }

        vm.interpret(&chunk);
    }
}

int main(int argc, char* argv[]) {
    if (argc == 2) {
        runFile(argv[1]);
    } else {
        runPrompt();
    }
    return 0;
}