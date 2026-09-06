#ifndef ADILANG_VM_H
#define ADILANG_VM_H

#include "chunk.h"
#include "compiler.h"
#include <iostream>
#include <vector>
#include <variant>
#include <unordered_map>
#include <memory>

struct CallFrame {
    AdiClosure* closure = nullptr;
    size_t ip = 0;
    size_t slots = 0; // Stack offset for this function's locals
};

enum class InterpretResult {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
};

class VM {
public:
    template <typename T, typename... Args>
    T* allocateObject(Args&&... args) {
        T* object = new T(std::forward<Args>(args)...);
        
        // Explicitly cast to AdiObject* for the GC linked list tracking
        AdiObject* gcObject = static_cast<AdiObject*>(object);
        gcObject->next = objects;
        objects = gcObject;
        
        return object;
    }

private:
    static constexpr int FRAMES_MAX = 64;
    static constexpr int STACK_MAX = FRAMES_MAX * 256;

    // Master GC linked list anchor and base object definition
    AdiObject* objects = nullptr;

    CallFrame frames[FRAMES_MAX];
    int frameCount = 0;

    Chunk* chunk;
    size_t ip; // Instruction Pointer
    std::vector<Value> stack;
    std::unordered_map<std::string, Value> globals;
    AdiUpvalue* openUpvalues = nullptr;

    AdiUpvalue* captureUpvalue(Value* local) {
        AdiUpvalue* prevUpvalue = nullptr;
        AdiUpvalue* currentUpvalue = openUpvalues;

        while (currentUpvalue != nullptr && currentUpvalue->location > local) {
            prevUpvalue = currentUpvalue;
            currentUpvalue = currentUpvalue->next;
        }

        if (currentUpvalue != nullptr && currentUpvalue->location == local) {
            return currentUpvalue;
        }

        // Allocate raw pointer via GC manager instead of std::make_shared
        AdiUpvalue* createdUpvalue = allocateObject<AdiUpvalue>();
        createdUpvalue->location = local;
        createdUpvalue->next = currentUpvalue;

        if (prevUpvalue == nullptr) {
            openUpvalues = createdUpvalue;
        } else {
            prevUpvalue->next = createdUpvalue;
        }

        return createdUpvalue;
    }

    void closeUpvalues(Value* last) {
        while (openUpvalues != nullptr && openUpvalues->location >= last) {
            AdiUpvalue* upvalue = openUpvalues;
            upvalue->closed = *upvalue->location;
            upvalue->location = &upvalue->closed;
            openUpvalues = upvalue->next;
        }
    }

    void resetStack() {
        stack.clear();
        stack.reserve(STACK_MAX);
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
        } else if (std::holds_alternative<AdiFunction*>(value)) {
            std::cout << "<fn " << std::get<AdiFunction*>(value)->name << ">";
        } else if (std::holds_alternative<AdiClosure*>(value)) {
            std::cout << "<fn " << std::get<AdiClosure*>(value)->function->name << ">";
        } else {
            std::cout << "nil";
        }
    }
    void markValue(Value value) {
        if (std::holds_alternative<AdiArray*>(value)) {
            markObject(std::get<AdiArray*>(value));
        } else if (std::holds_alternative<AdiClosure*>(value)) {
            markObject(std::get<AdiClosure*>(value));
        } else if (std::holds_alternative<AdiFunction*>(value)) {
            markObject(std::get<AdiFunction*>(value));
        } else if (std::holds_alternative<AdiInstance*>(value)) {
            markObject(std::get<AdiInstance*>(value));
        } else if (std::holds_alternative<AdiStructDef*>(value)) {
            markObject(std::get<AdiStructDef*>(value));
        } else if (std::holds_alternative<AdiBoundMethod*>(value)) {
            markObject(std::get<AdiBoundMethod*>(value));
        } else if (std::holds_alternative<AdiNativeMethod*>(value)) {
            markObject(std::get<AdiNativeMethod*>(value));
        }
    }

    void markObject(AdiObject* object) {
        if (object == nullptr || object->isMarked) return;
        object->isMarked = true;

        if (auto closure = dynamic_cast<AdiClosure*>(object)) {
            markObject(closure->function);
            for (auto upvalue : closure->upvalues) {
                markObject(upvalue);
            }
        } else if (auto instance = dynamic_cast<AdiInstance*>(object)) {
            markObject(instance->blueprint);
            for (const auto& [name, val] : instance->fields) {
                markValue(val);
            }
        } else if (auto array = dynamic_cast<AdiArray*>(object)) {
            for (const auto& val : array->elements) {
                markValue(val);
            }
        } else if (auto upvalue = dynamic_cast<AdiUpvalue*>(object)) {
            if (upvalue->location == &upvalue->closed) {
                markValue(upvalue->closed);
            }
        } else if (auto structDef = dynamic_cast<AdiStructDef*>(object)) {
            markObject(structDef->superclass);
            for (const auto& [name, method] : structDef->methods) {
                markObject(method);
            }
        } else if (auto bound = dynamic_cast<AdiBoundMethod*>(object)) {
            markObject(bound->receiver);
            markObject(bound->method);
        } else if (auto native = dynamic_cast<AdiNativeMethod*>(object)) {
            markObject(native->self);
        }
    }

    void markRoots() {
        for (const auto& val : stack) {
            markValue(val);
        }
        for (const auto& [name, val] : globals) {
            markValue(val);
        }
        for (AdiUpvalue* upvalue = openUpvalues; upvalue != nullptr; upvalue = upvalue->next) {
            markObject(upvalue);
        }
    }

    void sweep() {
        AdiObject* previous = nullptr;
        AdiObject* current = objects;

        while (current != nullptr) {
            if (current->isMarked) {
                current->isMarked = false;
                previous = current;
                current = current->next;
            } else {
                AdiObject* unreached = current;
                current = current->next;

                if (previous == nullptr) {
                    objects = current;
                } else {
                    previous->next = current;
                }

                delete unreached;
            }
        }
    }
public:
    void collectGarbage() {
        markRoots();
        sweep();
    }
    InterpretResult interpret(Chunk* targetChunk) {
        AdiFunction* scriptFunction = allocateObject<AdiFunction>("script");
        scriptFunction->chunk = std::make_shared<Chunk>(*targetChunk);

        AdiClosure* scriptClosure = allocateObject<AdiClosure>();
        scriptClosure->function = scriptFunction;

        resetStack();
        frameCount = 0;
        openUpvalues = nullptr;

        CallFrame* frame = &frames[frameCount++];
        frame->closure = scriptClosure;
        frame->ip = 0;
        frame->slots = 0;

        chunk = scriptFunction->chunk.get();
        ip = 0;

        while (true) {
            CallFrame* currentFrame = &frames[frameCount - 1];
            chunk = currentFrame->closure->function->chunk.get();
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
                    currentFrame->ip = ip;
                    currentFrame->ip += offset;
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
                    currentFrame->ip = ip;
                    currentFrame->ip -= offset;
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
                    AdiClosure* closure = nullptr;

                    if (std::holds_alternative<AdiBoundMethod*>(callee)) {
                        auto bound = std::get<AdiBoundMethod*>(callee);
                        stack[stack.size() - argCount - 1] = bound->receiver;
                        closure = allocateObject<AdiClosure>();
                        closure->function = bound->method;
                    } else if (std::holds_alternative<AdiClosure*>(callee)) {
                        closure = std::get<AdiClosure*>(callee);
                    } else {
                        std::cerr << "Runtime Error: Can only call functions and methods.\n";
                        return InterpretResult::INTERPRET_RUNTIME_ERROR;
                    }

                    if (closure->function->arity != argCount) {
                        std::cerr << "Runtime Error: Expected " << closure->function->arity << " arguments but got " << argCount << ".\n";
                        return InterpretResult::INTERPRET_RUNTIME_ERROR;
                    }

                    if (frameCount == FRAMES_MAX) {
                        std::cerr << "Runtime Error: Stack overflow.\n";
                        return InterpretResult::INTERPRET_RUNTIME_ERROR;
                    }

                    CallFrame* frame = &frames[frameCount++];
                    frame->closure = closure;
                    frame->ip = 0;
                    frame->slots = stack.size() - argCount - 1;
                    currentFrame = frame;
                    break;
                }
                case OpCode::OP_RETURN: {
                    Value result = pop();
                    CallFrame* returningFrame = &frames[frameCount - 1];
                    
                    closeUpvalues(&stack[returningFrame->slots]);

                    if (returningFrame->closure->function->name == "init") {
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

                    Value blueprintVal = peek(argCount);
                    AdiStructDef* blueprint = std::get<AdiStructDef*>(blueprintVal);
                    
                    AdiInstance* instance = allocateObject<AdiInstance>(blueprint);

                    stack[stack.size() - argCount - 1] = instance;

                    auto initIt = blueprint->methods.find("init");
                    if (initIt != blueprint->methods.end()) {
                        AdiFunction* function = initIt->second;

                        if (function->arity != argCount) {
                            std::cerr << "Runtime Error: Expected " << function->arity << " arguments but got " << argCount << ".\n";
                            return InterpretResult::INTERPRET_RUNTIME_ERROR;
                        }

                        if (frameCount == FRAMES_MAX) {
                            std::cerr << "Runtime Error: Stack overflow.\n";
                            return InterpretResult::INTERPRET_RUNTIME_ERROR;
                        }

                        AdiClosure* initClosure = allocateObject<AdiClosure>();
                        initClosure->function = function;

                        CallFrame* frame = &frames[frameCount++];
                        frame->closure = initClosure;
                        frame->ip = 0;
                        frame->slots = stack.size() - argCount - 1;
                        currentFrame = frame;
                    } else {
                        if (argCount > 0) {
                            std::cerr << "Runtime Error: Struct '" << blueprint->name << "' has no init method but received " << argCount << " arguments.\n";
                            return InterpretResult::INTERPRET_RUNTIME_ERROR;
                        }
                    }
                    break;
                }
                case OpCode::OP_GET: {
                    uint8_t nameIndex = chunk->code[ip++];
                    currentFrame->ip = ip;
                    std::string name = std::get<std::string>(chunk->constants[nameIndex]);
                    Value targetVal = pop();

                    if (!std::holds_alternative<AdiInstance*>(targetVal)) {
                        std::cerr << "Runtime Error: Only instances have properties.\n";
                        return InterpretResult::INTERPRET_RUNTIME_ERROR;
                    }

                    AdiInstance* instance = std::get<AdiInstance*>(targetVal);

                    if (instance->fields.find(name) != instance->fields.end()) {
                        push(instance->fields[name]);
                    } else if (instance->blueprint->methods.find(name) != instance->blueprint->methods.end()) {
                        AdiFunction* method = instance->blueprint->methods[name];
                        
                        AdiBoundMethod* bound = allocateObject<AdiBoundMethod>(instance, method);
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

                    if (!std::holds_alternative<AdiInstance*>(targetVal)) {
                        std::cerr << "Runtime Error: Only instances have fields.\n";
                        return InterpretResult::INTERPRET_RUNTIME_ERROR;
                    }

                    AdiInstance* instance = std::get<AdiInstance*>(targetVal);
                    instance->fields[name] = value;
                    push(value);
                    break;
                }
                case OpCode::OP_STRUCT: {
                    uint8_t nameIndex = chunk->code[ip++];
                    currentFrame->ip = ip;
                    std::string name = std::get<std::string>(chunk->constants[nameIndex]);

                    Value superclassVal = pop();
                    AdiStructDef* superclass = nullptr;
                    if (std::holds_alternative<AdiStructDef*>(superclassVal)) {
                        superclass = std::get<AdiStructDef*>(superclassVal);
                    }

                    AdiStructDef* structDef = allocateObject<AdiStructDef>(name, std::vector<std::string>{}, superclass);

                    if (superclass != nullptr) {
                        structDef->fields = superclass->fields;
                        structDef->methods = superclass->methods;
                    }

                    globals[name] = structDef;
                    push(structDef);
                    break;
                }
                case OpCode::OP_SUPER: {
                    uint8_t nameIndex = chunk->code[ip++];
                    uint8_t argCount = chunk->code[ip++];
                    currentFrame->ip = ip;

                    std::string methodName = std::get<std::string>(chunk->constants[nameIndex]);

                    Value superclassVal = peek(argCount);
                    AdiStructDef* superclass = std::get<AdiStructDef*>(superclassVal);

                    auto methodIt = superclass->methods.find(methodName);
                    if (methodIt == superclass->methods.end()) {
                        std::cerr << "Runtime Error: Superclass '" << superclass->name << "' has no method '" << methodName << "'.\n";
                        return InterpretResult::INTERPRET_RUNTIME_ERROR;
                    }

                    AdiFunction* function = methodIt->second;
                    if (function->arity != argCount) {
                        std::cerr << "Runtime Error: Expected " << function->arity << " arguments for super." << methodName << " but got " << argCount << ".\n";
                        return InterpretResult::INTERPRET_RUNTIME_ERROR;
                    }

                    size_t receiverSlot = stack.size() - argCount - 1;
                    Value receiver = peek(argCount + 1);
                    stack[receiverSlot] = receiver;
                    stack.erase(stack.begin() + receiverSlot - 1);

                    if (frameCount == FRAMES_MAX) {
                        std::cerr << "Runtime Error: Stack overflow.\n";
                        return InterpretResult::INTERPRET_RUNTIME_ERROR;
                    }

                    AdiClosure* superClosure = allocateObject<AdiClosure>();
                    superClosure->function = function;

                    CallFrame* frame = &frames[frameCount++];
                    frame->closure = superClosure;
                    frame->ip = 0;
                    frame->slots = stack.size() - argCount - 1;
                    currentFrame = frame;
                    break;
                }
                case OpCode::OP_ARRAY: {
                    uint8_t elementCount = chunk->code[ip++];
                    currentFrame->ip = ip;
                    std::vector<Value> elements;
                    elements.resize(elementCount);
                    for (int i = elementCount - 1; i >= 0; i--) {
                        elements[i] = pop();
                    }
                    push(allocateObject<AdiArray>(std::move(elements)));
                    break;
                }
                case OpCode::OP_INDEX_GET: {
                    Value indexVal = pop();
                    Value targetVal = pop();

                    if (!std::holds_alternative<AdiArray*>(targetVal)) {
                        std::cerr << "Runtime Error: Only arrays can be indexed.\n";
                        return InterpretResult::INTERPRET_RUNTIME_ERROR;
                    }
                    if (!std::holds_alternative<double>(indexVal)) {
                        std::cerr << "Runtime Error: Array index must be a number.\n";
                        return InterpretResult::INTERPRET_RUNTIME_ERROR;
                    }

                    AdiArray* arr = std::get<AdiArray*>(targetVal);
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

                    if (!std::holds_alternative<AdiArray*>(targetVal)) {
                        std::cerr << "Runtime Error: Only arrays can be assigned by index.\n";
                        return InterpretResult::INTERPRET_RUNTIME_ERROR;
                    }
                    if (!std::holds_alternative<double>(indexVal)) {
                        std::cerr << "Runtime Error: Array index must be a number.\n";
                        return InterpretResult::INTERPRET_RUNTIME_ERROR;
                    }

                    AdiArray* arr = std::get<AdiArray*>(targetVal);
                    int idx = static_cast<int>(std::get<double>(indexVal));

                    if (idx < 0 || static_cast<size_t>(idx) >= arr->elements.size()) {
                        std::cerr << "Runtime Error: Array assignment index out of bounds: " << idx << "\n";
                        return InterpretResult::INTERPRET_RUNTIME_ERROR;
                    }

                    arr->elements[idx] = val;
                    push(val);
                    break;
                }
                case OpCode::OP_CLOSURE: {
                    uint8_t constantIndex = chunk->code[ip++];
                    currentFrame->ip = ip;
                    Value constant = chunk->constants[constantIndex];
                    
                    AdiFunction* function = std::get<AdiFunction*>(constant);

                    AdiClosure* closure = allocateObject<AdiClosure>();
                    closure->function = function;

                    for (int i = 0; i < function->upvalueCount; i++) {
                        bool isLocal = chunk->code[ip++] == 1;
                        uint8_t index = chunk->code[ip++];
                        currentFrame->ip = ip;

                        if (isLocal) {
                            closure->upvalues.push_back(captureUpvalue(&stack[currentFrame->slots + index]));
                        } else {
                            closure->upvalues.push_back(currentFrame->closure->upvalues[index]);
                        }
                    }

                    push(closure);
                    break;
                }
                case OpCode::OP_GET_UPVALUE: {
                    uint8_t slot = chunk->code[ip++];
                    currentFrame->ip = ip;
                    push(*(currentFrame->closure->upvalues[slot]->location));
                    break;
                }
                case OpCode::OP_SET_UPVALUE: {
                    uint8_t slot = chunk->code[ip++];
                    currentFrame->ip = ip;
                    *(currentFrame->closure->upvalues[slot]->location) = peek(0);
                    break;
                }
                case OpCode::OP_CLOSE_UPVALUE: {
                    closeUpvalues(&stack.back());
                    pop();
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