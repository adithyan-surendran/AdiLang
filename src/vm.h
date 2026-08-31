#ifndef ADILANG_VM_H
#define ADILANG_VM_H

#include "chunk.h"
#include <iostream>
#include <vector>
#include <variant>
#include <unordered_map>

enum class InterpretResult {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
};

class VM {
private:
    Chunk* chunk;
    size_t ip; // Instruction Pointer
    std::vector<Value> stack;
    std::unordered_map<std::string, Value> globals;

    void resetStack() {
        stack.clear();
    }

    void push(Value value) {
        stack.push_back(value);
    }

    Value pop() {
        Value val = stack.back();
        stack.pop_back();
        return val;
    }

    void printValue(const Value& value) {
        if (std::holds_alternative<double>(value)) {
            std::cout << std::get<double>(value);
        } else if (std::holds_alternative<bool>(value)) {
            std::cout << (std::get<bool>(value) ? "true" : "false");
        } else if (std::holds_alternative<std::string>(value)) {
            std::cout << std::get<std::string>(value);
        } else {
            std::cout << "nil";
        }
    }

public:
    InterpretResult interpret(Chunk* targetChunk) {
        chunk = targetChunk;
        ip = 0;
        resetStack();

        while (ip < chunk->code.size()) {
            uint8_t instruction = chunk->code[ip++];
            switch (static_cast<OpCode>(instruction)) {
                case OpCode::OP_CONSTANT: {
                    uint8_t constantIndex = chunk->code[ip++];
                    push(chunk->constants[constantIndex]);
                    break;
                }
                case OpCode::OP_NIL: push(false); break;
                case OpCode::OP_TRUE: push(true); break;
                case OpCode::OP_FALSE: push(false); break;
                case OpCode::OP_POP: pop(); break;

                case OpCode::OP_DEFINE_GLOBAL: {
                    uint8_t nameIndex = chunk->code[ip++];
                    std::string name = std::get<std::string>(chunk->constants[nameIndex]);
                    globals[name] = pop();
                    break;
                }
                case OpCode::OP_GET_GLOBAL: {
                    uint8_t nameIndex = chunk->code[ip++];
                    std::string name = std::get<std::string>(chunk->constants[nameIndex]);
                    if (globals.find(name) == globals.end()) {
                        std::cerr << "Runtime Error: Undefined variable '" << name << "'.\n";
                        return InterpretResult::INTERPRET_RUNTIME_ERROR;
                    }
                    push(globals[name]);
                    break;
                }
                case OpCode::OP_SET_GLOBAL: {
                    uint8_t nameIndex = chunk->code[ip++];
                    std::string name = std::get<std::string>(chunk->constants[nameIndex]);
                    if (globals.find(name) == globals.end()) {
                        std::cerr << "Runtime Error: Undefined variable '" << name << "'.\n";
                        return InterpretResult::INTERPRET_RUNTIME_ERROR;
                    }
                    globals[name] = stack.back();
                    break;
                }

                case OpCode::OP_ADD: {
                    Value b = pop();
                    Value a = pop();
                    if (std::holds_alternative<double>(a) && std::holds_alternative<double>(b)) {
                        push(std::get<double>(a) + std::get<double>(b));
                    } else if (std::holds_alternative<std::string>(a) && std::holds_alternative<std::string>(b)) {
                        push(std::get<std::string>(a) + std::get<std::string>(b));
                    } else {
                        std::cerr << "Runtime Error: Operands must be two numbers or two strings.\n";
                        return InterpretResult::INTERPRET_RUNTIME_ERROR;
                    }
                    break;
                }
                case OpCode::OP_SUBTRACT: {
                    Value b = pop();
                    Value a = pop();
                    if (std::holds_alternative<double>(a) && std::holds_alternative<double>(b)) {
                        push(std::get<double>(a) - std::get<double>(b));
                    } else {
                        std::cerr << "Runtime Error: Operands must be numbers.\n";
                        return InterpretResult::INTERPRET_RUNTIME_ERROR;
                    }
                    break;
                }
                case OpCode::OP_MULTIPLY: {
                    Value b = pop();
                    Value a = pop();
                    if (std::holds_alternative<double>(a) && std::holds_alternative<double>(b)) {
                        push(std::get<double>(a) * std::get<double>(b));
                    } else {
                        std::cerr << "Runtime Error: Operands must be numbers.\n";
                        return InterpretResult::INTERPRET_RUNTIME_ERROR;
                    }
                    break;
                }
                case OpCode::OP_DIVIDE: {
                    Value b = pop();
                    Value a = pop();
                    if (std::holds_alternative<double>(a) && std::holds_alternative<double>(b)) {
                        double divisor = std::get<double>(b);
                        if (divisor == 0.0) {
                            std::cerr << "Runtime Error: Division by zero.\n";
                            return InterpretResult::INTERPRET_RUNTIME_ERROR;
                        }
                        push(std::get<double>(a) / divisor);
                    } else {
                        std::cerr << "Runtime Error: Operands must be numbers.\n";
                        return InterpretResult::INTERPRET_RUNTIME_ERROR;
                    }
                    break;
                }
                case OpCode::OP_NEGATE: {
                    Value val = pop();
                    if (std::holds_alternative<double>(val)) {
                        push(-std::get<double>(val));
                    } else {
                        std::cerr << "Runtime Error: Operand must be a number.\n";
                        return InterpretResult::INTERPRET_RUNTIME_ERROR;
                    }
                    break;
                }
                case OpCode::OP_PRINT: {
                    printValue(pop());
                    std::cout << "\n";
                    break;
                }
                case OpCode::OP_RETURN: {
                    return InterpretResult::INTERPRET_OK;
                }
                default:
                    std::cerr << "Unknown opcode execution error.\n";
                    return InterpretResult::INTERPRET_RUNTIME_ERROR;
            }
        }

        return InterpretResult::INTERPRET_OK;
    }
};

#endif