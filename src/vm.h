#ifndef ADILANG_VM_H
#define ADILANG_VM_H

#include "chunk.h"
#include <iostream>
#include <vector>
#include <variant>
#include <unordered_map>

struct CallFrame {
    std::shared_ptr<AdiFunction> function;
    size_t ip;
    size_t slots; // Stack offset for this function's locals
};

enum class InterpretResult {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
};

class VM {
private:
    static constexpr int FRAMES_MAX = 64;
    static constexpr int STACK_MAX = FRAMES_MAX * 256;

    CallFrame frames[FRAMES_MAX];
    int frameCount = 0;

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

    Value peek(int distance) {
        return stack[stack.size() - 1 - distance];
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
        auto scriptFunction = std::make_shared<AdiFunction>("script");
        scriptFunction->chunk = *targetChunk;

        resetStack();
        frameCount = 0;

        CallFrame* frame = &frames[frameCount++];
        frame->function = scriptFunction;
        frame->ip = 0;
        frame->slots = 0;

        chunk = &scriptFunction->chunk;
        ip = 0;

        while (true) {
            // Synchronize active chunk and instruction pointer with the current frame
            CallFrame* currentFrame = &frames[frameCount - 1];
            chunk = &currentFrame->function->chunk;
            ip = currentFrame->ip;

            if (ip >= chunk->code.size()) {
                break;
            }

            uint8_t instruction = chunk->code[ip++];
            currentFrame->ip = ip; // Save back advanced IP

            switch (static_cast<OpCode>(instruction)) {
                case OpCode::OP_CONSTANT: {
                    uint8_t constantIndex = chunk->code[ip++];
                    currentFrame->ip = ip;
                    push(chunk->constants[constantIndex]);
                    break;
                }
                case OpCode::OP_NIL: push(false); break;
                case OpCode::OP_TRUE: push(true); break;
                case OpCode::OP_FALSE: push(false); break;
                case OpCode::OP_POP: pop(); break;

                case OpCode::OP_DEFINE_GLOBAL: {
                    uint8_t nameIndex = chunk->code[ip++];
                    currentFrame->ip = ip;
                    std::string name = std::get<std::string>(chunk->constants[nameIndex]);
                    globals[name] = pop();
                    break;
                }
                case OpCode::OP_GET_GLOBAL: {
                    uint8_t nameIndex = chunk->code[ip++];
                    currentFrame->ip = ip;
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
                    currentFrame->ip = ip;
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
                case OpCode::OP_EQUAL: {
                    Value b = pop();
                    Value a = pop();
                    push(a == b);
                    break;
                }
                case OpCode::OP_GREATER: {
                    Value b = pop();
                    Value a = pop();
                    if (std::holds_alternative<double>(a) && std::holds_alternative<double>(b)) {
                        push(std::get<double>(a) > std::get<double>(b));
                    } else {
                        std::cerr << "Runtime Error: Operands must be numbers.\n";
                        return InterpretResult::INTERPRET_RUNTIME_ERROR;
                    }
                    break;
                }
                case OpCode::OP_LESS: {
                    Value b = pop();
                    Value a = pop();
                    if (std::holds_alternative<double>(a) && std::holds_alternative<double>(b)) {
                        push(std::get<double>(a) < std::get<double>(b));
                    } else {
                        std::cerr << "Runtime Error: Operands must be numbers.\n";
                        return InterpretResult::INTERPRET_RUNTIME_ERROR;
                    }
                    break;
                }
                case OpCode::OP_JUMP: {
                    uint16_t offset = (chunk->code[ip] << 8) | chunk->code[ip + 1];
                    ip += 2;
                    currentFrame->ip = ip + offset;
                    break;
                }
                case OpCode::OP_JUMP_IF_FALSE: {
                    uint16_t offset = (chunk->code[ip] << 8) | chunk->code[ip + 1];
                    ip += 2;
                    currentFrame->ip = ip;
                    Value val = stack.back();
                    bool isFalsy = std::holds_alternative<bool>(val) && !std::get<bool>(val);
                    if (isFalsy) {
                        currentFrame->ip += offset;
                    }
                    break;
                }
                case OpCode::OP_LOOP: {
                    uint16_t offset = (chunk->code[ip] << 8) | chunk->code[ip + 1];
                    ip += 2;
                    currentFrame->ip = ip - offset;
                    break;
                }
                case OpCode::OP_GET_LOCAL: {
                    uint8_t slot = chunk->code[ip++];
                    currentFrame->ip = ip;
                    push(stack[currentFrame->slots + slot]);
                    break;
                }
                case OpCode::OP_SET_LOCAL: {
                    uint8_t slot = chunk->code[ip++];
                    currentFrame->ip = ip;
                    stack[currentFrame->slots + slot] = peek(0);
                    break;
                }
                case OpCode::OP_CALL: {
                    uint8_t argCount = chunk->code[ip++];
                    currentFrame->ip = ip;
                    Value callee = peek(argCount);

                    if (!std::holds_alternative<std::shared_ptr<AdiFunction>>(callee)) {
                        std::cerr << "Runtime Error: Can only call functions.\n";
                        return InterpretResult::INTERPRET_RUNTIME_ERROR;
                    }

                    auto function = std::get<std::shared_ptr<AdiFunction>>(callee);
                    if (argCount != function->arity) {
                        std::cerr << "Runtime Error: Expected " << function->arity 
                                  << " arguments but got " << argCount << ".\n";
                        return InterpretResult::INTERPRET_RUNTIME_ERROR;
                    }

                    if (frameCount == FRAMES_MAX) {
                        std::cerr << "Runtime Error: Stack overflow.\n";
                        return InterpretResult::INTERPRET_RUNTIME_ERROR;
                    }

                    CallFrame* frame = &frames[frameCount++];
                    frame->function = function;
                    frame->ip = 0;
                    frame->slots = stack.size() - argCount - 1;
                    break;
                }
                case OpCode::OP_RETURN: {
                    Value result = pop();
                    frameCount--;

                    if (frameCount == 0) {
                        return InterpretResult::INTERPRET_OK;
                    }

                    CallFrame* prevFrame = &frames[frameCount];
                    stack.resize(prevFrame->slots);
                    push(result);
                    break;
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