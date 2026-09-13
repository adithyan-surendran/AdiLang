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


// ============================================================
// Run File
// ============================================================

void runFile(const std::string& path)
{
    // --------------------------------------------------------
    // Open file
    // --------------------------------------------------------

    std::ifstream file(path);

    if (!file.is_open()) {
        std::cerr << "Could not open file: " << path << "\n";
        exit(74);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    std::string source = buffer.str();


    // --------------------------------------------------------
    // 1. Lex
    // --------------------------------------------------------

    Lexer lexer(source);
    auto tokens = lexer.scanTokens();


    // --------------------------------------------------------
    // 2. Parse
    // --------------------------------------------------------

    Parser parser(tokens);
    auto program = parser.parse();


    // --------------------------------------------------------
    // Stop if parser errors occurred
    // --------------------------------------------------------

    if (parser.hasError()) {
        std::cerr << "Parsing failed.\n";
        exit(65);
    }


    // --------------------------------------------------------
    // 3. Compile
    // --------------------------------------------------------

    Chunk chunk;

    Compiler compiler(&vm);

    if (!compiler.compile(program.get(), &chunk)) {
        std::cerr << "Compilation failed during bytecode emission.\n";
        exit(65);
    }


    // --------------------------------------------------------
    // 4. Interpret
    // --------------------------------------------------------

    InterpretResult result = vm.interpret(&chunk);


    // --------------------------------------------------------
    // Handle VM result
    // --------------------------------------------------------

    if (result == InterpretResult::INTERPRET_COMPILE_ERROR) {
        exit(65);
    }

    if (result == InterpretResult::INTERPRET_RUNTIME_ERROR) {
        exit(70);
    }
}


// ============================================================
// REPL
// ============================================================

void runPrompt()
{
    std::string line;

    std::cout << "AdiLang v0.18.0 REPL\n";

    for (;;) {

        std::cout << "> ";

        if (!std::getline(std::cin, line)) {
            std::cout << "\n";
            break;
        }


        // ----------------------------------------------------
        // Lex
        // ----------------------------------------------------

        Lexer lexer(line);
        auto tokens = lexer.scanTokens();


        // ----------------------------------------------------
        // Parse
        // ----------------------------------------------------

        Parser parser(tokens);
        auto program = parser.parse();


        // ----------------------------------------------------
        // Do not compile invalid input
        // ----------------------------------------------------

        if (parser.hasError()) {
            continue;
        }


        // ----------------------------------------------------
        // Compile
        // ----------------------------------------------------

        Chunk chunk;

        Compiler compiler(&vm);

        if (!compiler.compile(program.get(), &chunk)) {
            continue;
        }


        // ----------------------------------------------------
        // Interpret
        // ----------------------------------------------------

        vm.interpret(&chunk);
    }
}


// ============================================================
// Main
// ============================================================

int main(int argc, char* argv[])
{
    if (argc == 2) {
        runFile(argv[1]);
    }
    else {
        runPrompt();
    }

    return 0;
}