#ifndef ADILANG_VM_H
#define ADILANG_VM_H

#include "chunk.h"
#include "compiler.h"
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
        scriptFunction->chunk = std::make_shared<Chunk>(*targetChunk);

        resetStack();
        frameCount = 0;

        CallFrame* frame = &frames[frameCount++];
        frame->function = scriptFunction;
        frame->ip = 0;
        frame->slots = 0;

        chunk = scriptFunction->chunk.get();
        ip = 0;

        while (true) {
            CallFrame* currentFrame = &frames[frameCount - 1];
            chunk = currentFrame->function->chunk.get();
            ip = currentFrame->ip;

            if (ip >= chunk->code.size()) {
                break;
            }

            uint8_t instruction = chunk->code[ip++];
            currentFrame->ip = ip;

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
                    std::shared_ptr<AdiFunction> function;

                    if (std::holds_alternative<std::shared_ptr<AdiBoundMethod>>(callee)) {
                        auto bound = std::get<std::shared_ptr<AdiBoundMethod>>(callee);
                        // Replace the bound method on the stack with the receiver instance ('this')
                        stack[stack.size() - argCount - 1] = bound->receiver;
                        function = bound->method;
                    } else if (std::holds_alternative<std::shared_ptr<AdiFunction>>(callee)) {
                        function = std::get<std::shared_ptr<AdiFunction>>(callee);
                    } else {
                        std::cerr << "Runtime Error: Can only call functions and methods.\n";
                        return InterpretResult::INTERPRET_RUNTIME_ERROR;
                    }

                    if (function->arity != argCount) {
                        std::cerr << "Runtime Error: Expected " << function->arity << " arguments but got " << argCount << ".\n";
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
                    currentFrame = frame;
                    break;
                }
                case OpCode::OP_RETURN: {
                    Value result = pop();
                    CallFrame* returningFrame = &frames[frameCount - 1];
                    
                    // If this was a constructor ('init'), it must implicitly return 'this' (slot 0)
                    if (returningFrame->function->name == "init") {
                        result = stack[returningFrame->slots];
                    }

                    frameCount--;

                    if (frameCount == 0) {
                        return InterpretResult::INTERPRET_OK;
                    }

                    stack.resize(returningFrame->slots);
                    push(result);
                    break;
                }
                case OpCode::OP_STRUCT_INSTANCE: {
                    uint8_t argCount = chunk->code[ip++];
                    currentFrame->ip = ip;

                    // 1. The blueprint is sitting just below the arguments on the stack
                    Value blueprintVal = peek(argCount);
                    auto blueprint = std::get<std::shared_ptr<AdiStructDef>>(blueprintVal);

                    // 2. Create the new instance
                    auto instance = std::make_shared<AdiInstance>(blueprint);

                    // Replace the blueprint on the stack with the newly created instance
                    // so it acts as 'this' (slot 0) for the constructor
                    stack[stack.size() - argCount - 1] = instance;

                    // 3. Look for an 'init' method
                    auto initIt = blueprint->methods.find("init");
                    if (initIt != blueprint->methods.end()) {
                        auto function = initIt->second;

                        if (function->arity != argCount) {
                            std::cerr << "Runtime Error: Expected " << function->arity << " arguments but got " << argCount << ".\n";
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
                        currentFrame = frame;
                    } else {
                        if (argCount > 0) {
                            std::cerr << "Runtime Error: Struct '" << blueprint->name << "' has no init method but received " << argCount << " arguments.\n";
                            return InterpretResult::INTERPRET_RUNTIME_ERROR;
                        }
                        // If 0 args and no init, the instance is already in place on top of the stack.
                    }
                    break;
                }
                case OpCode::OP_GET: {
                    uint8_t nameIndex = chunk->code[ip++];
                    currentFrame->ip = ip;
                    std::string name = std::get<std::string>(chunk->constants[nameIndex]);
                    Value targetVal = pop();

                    if (!std::holds_alternative<std::shared_ptr<AdiInstance>>(targetVal)) {
                        std::cerr << "Runtime Error: Only instances have properties.\n";
                        return InterpretResult::INTERPRET_RUNTIME_ERROR;
                    }

                    auto instance = std::get<std::shared_ptr<AdiInstance>>(targetVal);

                    if (instance->fields.find(name) != instance->fields.end()) {
                        push(instance->fields[name]);
                    } else if (instance->blueprint->methods.find(name) != instance->blueprint->methods.end()) {
                        auto method = instance->blueprint->methods[name];
                        auto bound = std::make_shared<AdiBoundMethod>(instance, method);
                        push(bound);
                    } else {
                        std::cerr << "Runtime Error: Undefined property '" << name << "'.\n";
                        return InterpretResult::INTERPRET_RUNTIME_ERROR;
                    }
                    break;
                }
                case OpCode::OP_SET: {
                    uint8_t nameIndex = chunk->code[ip++];
                    currentFrame->ip = ip;
                    std::string name = std::get<std::string>(chunk->constants[nameIndex]);
                    Value targetVal = pop();
                    Value value = pop();

                    if (!std::holds_alternative<std::shared_ptr<AdiInstance>>(targetVal)) {
                        std::cerr << "Runtime Error: Only instances have fields.\n";
                        return InterpretResult::INTERPRET_RUNTIME_ERROR;
                    }

                    auto instance = std::get<std::shared_ptr<AdiInstance>>(targetVal);
                    instance->fields[name] = value;
                    push(value);
                    break;
                }
                case OpCode::OP_STRUCT: {
                    // Handled via constant table / globals during compilation passes
                    break;
                }
                case OpCode::OP_ARRAY: {
                    uint8_t elementCount = chunk->code[ip++];
                    currentFrame->ip = ip;
                    std::vector<Value> elements;
                    // Elements are on the stack in order, pop them
                    elements.resize(elementCount);
                    for (int i = elementCount - 1; i >= 0; i--) {
                        elements[i] = pop();
                    }
                    push(std::make_shared<AdiArray>(std::move(elements)));
                    break;
                }
                case OpCode::OP_INDEX_GET: {
                    Value indexVal = pop();
                    Value targetVal = pop();

                    if (!std::holds_alternative<std::shared_ptr<AdiArray>>(targetVal)) {
                        std::cerr << "Runtime Error: Only arrays can be indexed.\n";
                        return InterpretResult::INTERPRET_RUNTIME_ERROR;
                    }
                    if (!std::holds_alternative<double>(indexVal)) {
                        std::cerr << "Runtime Error: Array index must be a number.\n";
                        return InterpretResult::INTERPRET_RUNTIME_ERROR;
                    }

                    auto arr = std::get<std::shared_ptr<AdiArray>>(targetVal);
                    int idx = static_cast<int>(std::get<double>(indexVal));

                    if (idx < 0 || static_cast<size_t>(idx) >= arr->elements.size()) {
                        std::cerr << "Runtime Error: Array index out of bounds: " << idx << "\n";
                        return InterpretResult::INTERPRET_RUNTIME_ERROR;
                    }

                    push(arr->elements[idx]);
                    break;
                }
                case OpCode::OP_INDEX_SET: {
                    Value indexVal = pop();
                    Value targetVal = pop();
                    Value val = pop();

                    if (!std::holds_alternative<std::shared_ptr<AdiArray>>(targetVal)) {
                        std::cerr << "Runtime Error: Only arrays can be assigned by index.\n";
                        return InterpretResult::INTERPRET_RUNTIME_ERROR;
                    }
                    if (!std::holds_alternative<double>(indexVal)) {
                        std::cerr << "Runtime Error: Array index must be a number.\n";
                        return InterpretResult::INTERPRET_RUNTIME_ERROR;
                    }

                    auto arr = std::get<std::shared_ptr<AdiArray>>(targetVal);
                    int idx = static_cast<int>(std::get<double>(indexVal));

                    if (idx < 0 || static_cast<size_t>(idx) >= arr->elements.size()) {
                        std::cerr << "Runtime Error: Array assignment index out of bounds: " << idx << "\n";
                        return InterpretResult::INTERPRET_RUNTIME_ERROR;
                    }

                    arr->elements[idx] = val;
                    push(val);
                    break;
                }
                default:
                    std::cerr << "Unknown opcode execution error: " << static_cast<int>(instruction) 
                              << " at IP: " << (ip - 1) << "\n";
                    return InterpretResult::INTERPRET_RUNTIME_ERROR;
            }
        }

        return InterpretResult::INTERPRET_OK;
    }
};

#endif // ADILANG_VM_H