#include "vm.h"

#include <iostream>
#include <memory>
#include <string>
#include <variant>

// ============================================================
// RUNTIME ERROR
// ============================================================

void VM::runtimeError(const std::string& message) {
    std::cerr << "Runtime Error: " << message << "\n";

    for (int i = frameCount - 1; i >= 0; i--) {
        CallFrame* frame = &frames[i];

        if (
            frame->closure == nullptr ||
            frame->closure->function == nullptr ||
            frame->closure->function->chunk == nullptr
        ) {
            continue;
        }

        Chunk* targetChunk =
            frame->closure->function->chunk.get();

        size_t instruction =
            frame->ip > 0 ? frame->ip - 1 : 0;

        int line =
            instruction < targetChunk->lines.size()
                ? targetChunk->lines[instruction]
                : 0;

        std::string name =
            frame->closure->function->name;

        if (name.empty() || name == "script") {
            std::cerr
                << "  at [line "
                << line
                << "] in script\n";
        } else {
            std::cerr
                << "  at [line "
                << line
                << "] in "
                << name
                << "()\n";
        }
    }

    resetStack();
    frameCount = 0;
    openUpvalues = nullptr;
}


// ============================================================
// BYTECODE SAFETY HELPERS
// ============================================================

bool VM::checkInstructionBytes(
    size_t count,
    const CallFrame* frame
) const {
    if (
        frame == nullptr ||
        frame->closure == nullptr ||
        frame->closure->function == nullptr ||
        frame->closure->function->chunk == nullptr
    ) {
        return false;
    }

    const Chunk* targetChunk =
        frame->closure->function->chunk.get();

    size_t frameIP = frame->ip;

    return frameIP <= targetChunk->code.size() &&
           count <= targetChunk->code.size() - frameIP;
}


bool VM::checkConstantIndex(
    size_t index,
    const CallFrame* frame
) const {
    if (
        frame == nullptr ||
        frame->closure == nullptr ||
        frame->closure->function == nullptr ||
        frame->closure->function->chunk == nullptr
    ) {
        return false;
    }

    const Chunk* targetChunk =
        frame->closure->function->chunk.get();

    return index < targetChunk->constants.size();
}


bool VM::checkLocalSlot(
    size_t slot,
    const CallFrame* frame
) const {
    if (frame == nullptr) {
        return false;
    }

    if (slot >= static_cast<size_t>(STACK_MAX)) {
        return false;
    }

    size_t targetIndex =
        frame->slots + slot;

    return targetIndex < stack.size();
}


bool VM::checkUpvalueSlot(
    size_t slot,
    const CallFrame* frame
) const {
    if (
        frame == nullptr ||
        frame->closure == nullptr
    ) {
        return false;
    }

    return slot <
           frame->closure->upvalues.size();
}


bool VM::checkJumpBytes(
    size_t count,
    const CallFrame* frame
) const {
    return checkInstructionBytes(count, frame);
}


// ============================================================
// STACK
// ============================================================

void VM::resetStack() {
    stack.clear();

    if (stack.capacity() < STACK_MAX) {
        stack.reserve(STACK_MAX);
    }
}


void VM::push(Value value) {
    if (stack.size() >= STACK_MAX) {
        runtimeError("Stack overflow.");
        return;
    }

    stack.push_back(std::move(value));
}


Value VM::pop() {
    if (stack.empty()) {
        runtimeError("Stack underflow.");
        return Value{};
    }

    Value value =
        std::move(stack.back());

    stack.pop_back();

    return value;
}


Value VM::peek(int distance) {
    if (
        distance < 0 ||
        stack.empty() ||
        static_cast<size_t>(distance) >= stack.size()
    ) {
        runtimeError("Stack peek out of bounds.");
        return Value{};
    }

    return stack[
        stack.size() -
        1 -
        static_cast<size_t>(distance)
    ];
}


// ============================================================
// UPVALUES
// ============================================================

AdiUpvalue* VM::captureUpvalue(Value* local) {
    AdiUpvalue* previous = nullptr;
    AdiUpvalue* current = openUpvalues;

    while (
        current != nullptr &&
        current->location > local
    ) {
        previous = current;
        current = current->next;
    }

    if (
        current != nullptr &&
        current->location == local
    ) {
        return current;
    }

    AdiUpvalue* created =
        allocateObject<AdiUpvalue>();

    created->location = local;
    created->next = current;

    if (previous == nullptr) {
        openUpvalues = created;
    } else {
        previous->next = created;
    }

    return created;
}


void VM::closeUpvalues(Value* last) {
    while (
        openUpvalues != nullptr &&
        openUpvalues->location >= last
    ) {
        AdiUpvalue* upvalue = openUpvalues;

        if (
            upvalue->location !=
            &upvalue->closed
        ) {
            upvalue->closed =
                *upvalue->location;

            upvalue->location =
                &upvalue->closed;
        }

        openUpvalues =
            upvalue->next;
    }
}


// ============================================================
// PRINTING
// ============================================================

void VM::printValue(const Value& value) {
    if (
        std::holds_alternative<double>(value)
    ) {
        std::cout
            << std::get<double>(value);
    }

    else if (
        std::holds_alternative<bool>(value)
    ) {
        std::cout
            << (
                std::get<bool>(value)
                    ? "true"
                    : "false"
            );
    }

    else if (
        std::holds_alternative<std::string>(value)
    ) {
        std::cout
            << std::get<std::string>(value);
    }

    else if (
        std::holds_alternative<AdiMap*>(value)
    ) {
        std::cout << "{";

        AdiMap* map =
            std::get<AdiMap*>(value);

        bool first = true;

        for (
            const auto& [key, val] :
            map->entries
        ) {
            if (!first) {
                std::cout << ", ";
            }

            std::cout
                << "\""
                << key
                << "\": ";

            printValue(val);

            first = false;
        }

        std::cout << "}";
    }

    else if (
        std::holds_alternative<AdiFunction*>(value)
    ) {
        AdiFunction* function =
            std::get<AdiFunction*>(value);

        if (function != nullptr) {
            std::cout
                << "<fn "
                << function->name
                << ">";
        } else {
            std::cout << "<fn>";
        }
    }

    else if (
        std::holds_alternative<AdiClosure*>(value)
    ) {
        AdiClosure* closure =
            std::get<AdiClosure*>(value);

        if (
            closure != nullptr &&
            closure->function != nullptr
        ) {
            std::cout
                << "<fn "
                << closure->function->name
                << ">";
        } else {
            std::cout << "<fn>";
        }
    }

    else {
        std::cout << "nil";
    }
}


// ============================================================
// GARBAGE COLLECTOR - MARK VALUE
// ============================================================

void VM::markValue(Value value) {
    if (
        std::holds_alternative<AdiArray*>(value)
    ) {
        markObject(
            std::get<AdiArray*>(value)
        );
    }

    else if (
        std::holds_alternative<AdiMap*>(value)
    ) {
        markObject(
            std::get<AdiMap*>(value)
        );
    }

    else if (
        std::holds_alternative<AdiClosure*>(value)
    ) {
        markObject(
            std::get<AdiClosure*>(value)
        );
    }

    else if (
        std::holds_alternative<AdiFunction*>(value)
    ) {
        markObject(
            std::get<AdiFunction*>(value)
        );
    }

    else if (
        std::holds_alternative<AdiInstance*>(value)
    ) {
        markObject(
            std::get<AdiInstance*>(value)
        );
    }

    else if (
        std::holds_alternative<AdiStructDef*>(value)
    ) {
        markObject(
            std::get<AdiStructDef*>(value)
        );
    }

    else if (
        std::holds_alternative<AdiBoundMethod*>(value)
    ) {
        markObject(
            std::get<AdiBoundMethod*>(value)
        );
    }

    else if (
        std::holds_alternative<AdiNativeMethod*>(value)
    ) {
        markObject(
            std::get<AdiNativeMethod*>(value)
        );
    }
}


// ============================================================
// GARBAGE COLLECTOR - MARK OBJECT
// ============================================================

void VM::markObject(AdiObject* object) {
    if (
        object == nullptr ||
        object->isMarked
    ) {
        return;
    }

    object->isMarked = true;


    // --------------------------------------------------------
    // Function
    // --------------------------------------------------------

    if (
        auto function =
            dynamic_cast<AdiFunction*>(object)
    ) {
        if (function->chunk != nullptr) {
            for (
                const auto& constant :
                function->chunk->constants
            ) {
                markValue(constant);
            }
        }
    }


    // --------------------------------------------------------
    // Closure
    // --------------------------------------------------------

    else if (
        auto closure =
            dynamic_cast<AdiClosure*>(object)
    ) {
        markObject(closure->function);

        for (
            AdiUpvalue* upvalue :
            closure->upvalues
        ) {
            markObject(upvalue);
        }
    }


    // --------------------------------------------------------
    // Instance
    // --------------------------------------------------------

    else if (
        auto instance =
            dynamic_cast<AdiInstance*>(object)
    ) {
        markObject(instance->blueprint);

        for (
            const auto& [name, value] :
            instance->fields
        ) {
            markValue(value);
        }
    }


    // --------------------------------------------------------
    // Array
    // --------------------------------------------------------

    else if (
        auto array =
            dynamic_cast<AdiArray*>(object)
    ) {
        for (
            const auto& value :
            array->elements
        ) {
            markValue(value);
        }
    }


    // --------------------------------------------------------
    // Map
    // --------------------------------------------------------

    else if (
        auto map =
            dynamic_cast<AdiMap*>(object)
    ) {
        for (
            const auto& [key, value] :
            map->entries
        ) {
            markValue(value);
        }
    }


    // --------------------------------------------------------
    // Upvalue
    // --------------------------------------------------------

    else if (
        auto upvalue =
            dynamic_cast<AdiUpvalue*>(object)
    ) {
        if (
            upvalue->location ==
            &upvalue->closed
        ) {
            markValue(upvalue->closed);
        }
    }


    // --------------------------------------------------------
    // Struct
    // --------------------------------------------------------

    else if (
        auto structDef =
            dynamic_cast<AdiStructDef*>(object)
    ) {
        markObject(structDef->superclass);

        for (
            const auto& [name, method] :
            structDef->methods
        ) {
            markObject(method);
        }
    }


    // --------------------------------------------------------
    // Bound method
    // --------------------------------------------------------

    else if (
        auto bound =
            dynamic_cast<AdiBoundMethod*>(object)
    ) {
        markObject(bound->receiver);
        markObject(bound->method);
    }


    // --------------------------------------------------------
    // Native method
    // --------------------------------------------------------

    else if (
        auto native =
            dynamic_cast<AdiNativeMethod*>(object)
    ) {
        markObject(native->self);
    }
}


// ============================================================
// GARBAGE COLLECTOR - ROOTS
// ============================================================

void VM::markRoots() {

    // Stack
    for (
        const auto& value :
        stack
    ) {
        markValue(value);
    }


    // Globals
    for (
        const auto& [name, value] :
        globals
    ) {
        markValue(value);
    }


    // Active call frames
    for (
        int i = 0;
        i < frameCount;
        i++
    ) {
        if (
            frames[i].closure != nullptr
        ) {
            markObject(
                frames[i].closure
            );
        }
    }


    // Open upvalues
    for (
        AdiUpvalue* upvalue = openUpvalues;
        upvalue != nullptr;
        upvalue = upvalue->next
    ) {
        markObject(upvalue);
    }
}


// ============================================================
// GARBAGE COLLECTOR - SWEEP
// ============================================================

void VM::sweep() {
    AdiObject* previous = nullptr;
    AdiObject* current = objects;

    while (current != nullptr) {

        if (current->isMarked) {
            current->isMarked = false;

            previous = current;
            current = current->next;
        }

        else {
            AdiObject* unreached = current;

            current = current->next;

            if (previous == nullptr) {
                objects = current;
            } else {
                previous->next = current;
            }

            /*
             * Preserve the current accounting behavior.
             * A proper per-object allocation size can be
             * added later.
             */
            if (
                bytesAllocated >=
                sizeof(*unreached)
            ) {
                bytesAllocated -=
                    sizeof(*unreached);
            } else {
                bytesAllocated = 0;
            }

            delete unreached;
        }
    }
}


// ============================================================
// GARBAGE COLLECTOR
// ============================================================

void VM::collectGarbage() {
    markRoots();
    sweep();
}


// ============================================================
// INTERPRETER
// ============================================================

InterpretResult VM::interpret(
    Chunk* targetChunk
) {
    if (targetChunk == nullptr) {
        std::cerr
            << "Runtime Error: "
               "Cannot interpret a null chunk.\n";

        return
            InterpretResult::
                INTERPRET_RUNTIME_ERROR;
    }


    // --------------------------------------------------------
    // Create script function
    // --------------------------------------------------------

    AdiFunction* scriptFunction =
        allocateObject<AdiFunction>(
            "script"
        );

    scriptFunction->chunk =
        std::make_shared<Chunk>(
            *targetChunk
        );


    resetStack();

    frameCount = 0;
    openUpvalues = nullptr;


    /*
     * Temporarily root the function.
     */
    push(scriptFunction);


    AdiClosure* scriptClosure =
        allocateObject<AdiClosure>();

    scriptClosure->function =
        scriptFunction;


    pop();


    // --------------------------------------------------------
    // Initial frame
    // --------------------------------------------------------

    if (frameCount >= FRAMES_MAX) {
        runtimeError("Stack overflow.");

        return
            InterpretResult::
                INTERPRET_RUNTIME_ERROR;
    }


    CallFrame* frame =
        &frames[frameCount++];


    frame->closure =
        scriptClosure;

    frame->ip = 0;
    frame->slots = 0;


    chunk =
        scriptFunction->chunk.get();

    ip = 0;


    // ========================================================
    // MAIN VM LOOP
    // ========================================================

    while (true) {

        if (frameCount <= 0) {
            return
                InterpretResult::
                    INTERPRET_OK;
        }


        CallFrame* currentFrame =
            &frames[frameCount - 1];


        if (
            currentFrame->closure == nullptr ||
            currentFrame->closure->function == nullptr ||
            currentFrame->closure->function->chunk == nullptr
        ) {
            runtimeError(
                "Invalid call frame."
            );

            return
                InterpretResult::
                    INTERPRET_RUNTIME_ERROR;
        }


        chunk =
            currentFrame
                ->closure
                ->function
                ->chunk
                .get();


        /*
         * Every frame has its own instruction pointer.
         *
         * The VM-level `ip` is only a working copy for
         * the current instruction.
         */
        ip = currentFrame->ip;


        if (ip >= chunk->code.size()) {
            runtimeError(
                "Instruction pointer out of bounds."
            );

            return
                InterpretResult::
                    INTERPRET_RUNTIME_ERROR;
        }


        uint8_t instruction =
            chunk->code[ip++];


        currentFrame->ip =
            ip;


        switch (
            static_cast<OpCode>(instruction)
        ) {


            // =================================================
            // CONSTANT
            // =================================================

            case OpCode::OP_CONSTANT: {

                if (
                    !checkInstructionBytes(
                        1,
                        currentFrame
                    )
                ) {
                    runtimeError(
                        "Missing constant operand."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                uint8_t constantIndex =
                    chunk->code[ip++];


                currentFrame->ip = ip;


                if (
                    !checkConstantIndex(
                        constantIndex,
                        currentFrame
                    )
                ) {
                    runtimeError(
                        "Constant index out of bounds."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                push(
                    chunk->constants[
                        constantIndex
                    ]
                );

                break;
            }


            // =================================================
            // NIL
            // =================================================

            case OpCode::OP_NIL:
                push(Value{});
                break;


            // =================================================
            // TRUE
            // =================================================

            case OpCode::OP_TRUE:
                push(true);
                break;


            // =================================================
            // FALSE
            // =================================================

            case OpCode::OP_FALSE:
                push(false);
                break;


            // =================================================
            // POP
            // =================================================

            case OpCode::OP_POP: {

                if (stack.empty()) {
                    runtimeError(
                        "Stack underflow."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }

                pop();

                break;
            }


            // =================================================
            // DEFINE GLOBAL
            // =================================================

            case OpCode::OP_DEFINE_GLOBAL: {

                if (
                    !checkInstructionBytes(
                        1,
                        currentFrame
                    )
                ) {
                    runtimeError(
                        "Missing global name operand."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                uint8_t nameIndex =
                    chunk->code[ip++];


                currentFrame->ip = ip;


                if (
                    !checkConstantIndex(
                        nameIndex,
                        currentFrame
                    )
                ) {
                    runtimeError(
                        "Global name constant index out of bounds."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                if (stack.empty()) {
                    runtimeError(
                        "Stack underflow while defining global."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                Value nameValue =
                    chunk->constants[
                        nameIndex
                    ];


                if (
                    !std::holds_alternative<
                        std::string
                    >(nameValue)
                ) {
                    runtimeError(
                        "Global name must be a string."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                std::string name =
                    std::get<std::string>(
                        nameValue
                    );


                globals[name] =
                    pop();

                break;
            }


            // =================================================
            // GET GLOBAL
            // =================================================

            case OpCode::OP_GET_GLOBAL: {

                if (
                    !checkInstructionBytes(
                        1,
                        currentFrame
                    )
                ) {
                    runtimeError(
                        "Missing global name operand."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                uint8_t nameIndex =
                    chunk->code[ip++];


                currentFrame->ip = ip;


                if (
                    !checkConstantIndex(
                        nameIndex,
                        currentFrame
                    )
                ) {
                    runtimeError(
                        "Global name constant index out of bounds."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                Value nameValue =
                    chunk->constants[
                        nameIndex
                    ];


                if (
                    !std::holds_alternative<
                        std::string
                    >(nameValue)
                ) {
                    runtimeError(
                        "Global name must be a string."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                std::string name =
                    std::get<std::string>(
                        nameValue
                    );


                auto it =
                    globals.find(name);


                if (it == globals.end()) {
                    runtimeError(
                        "Undefined variable '" +
                        name +
                        "'."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                push(it->second);

                break;
            }


            // =================================================
            // SET GLOBAL
            // =================================================

            case OpCode::OP_SET_GLOBAL: {

                if (
                    !checkInstructionBytes(
                        1,
                        currentFrame
                    )
                ) {
                    runtimeError(
                        "Missing global name operand."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                uint8_t nameIndex =
                    chunk->code[ip++];


                currentFrame->ip = ip;


                if (
                    !checkConstantIndex(
                        nameIndex,
                        currentFrame
                    )
                ) {
                    runtimeError(
                        "Global name constant index out of bounds."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                if (stack.empty()) {
                    runtimeError(
                        "Stack underflow while assigning global."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                Value nameValue =
                    chunk->constants[
                        nameIndex
                    ];


                if (
                    !std::holds_alternative<
                        std::string
                    >(nameValue)
                ) {
                    runtimeError(
                        "Global name must be a string."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                std::string name =
                    std::get<std::string>(
                        nameValue
                    );


                auto it =
                    globals.find(name);


                if (it == globals.end()) {
                    runtimeError(
                        "Undefined variable '" +
                        name +
                        "'."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                it->second =
                    stack.back();

                break;
            }


            // =================================================
            // ADD
            // =================================================

            case OpCode::OP_ADD: {

                if (stack.size() < 2) {
                    runtimeError(
                        "Stack underflow for addition."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                Value b = pop();
                Value a = pop();


                if (
                    std::holds_alternative<double>(a) &&
                    std::holds_alternative<double>(b)
                ) {
                    push(
                        std::get<double>(a) +
                        std::get<double>(b)
                    );
                }

                else if (
                    std::holds_alternative<std::string>(a) &&
                    std::holds_alternative<std::string>(b)
                ) {
                    push(
                        std::get<std::string>(a) +
                        std::get<std::string>(b)
                    );
                }

                else {
                    runtimeError(
                        "Operands must be two numbers or two strings."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }

                break;
            }


            // =================================================
            // SUBTRACT
            // =================================================

            case OpCode::OP_SUBTRACT: {

                if (stack.size() < 2) {
                    runtimeError(
                        "Stack underflow for subtraction."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                Value b = pop();
                Value a = pop();


                if (
                    std::holds_alternative<double>(a) &&
                    std::holds_alternative<double>(b)
                ) {
                    push(
                        std::get<double>(a) -
                        std::get<double>(b)
                    );
                }

                else {
                    runtimeError(
                        "Operands must be numbers."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }

                break;
            }


            // =================================================
            // MULTIPLY
            // =================================================

            case OpCode::OP_MULTIPLY: {

                if (stack.size() < 2) {
                    runtimeError(
                        "Stack underflow for multiplication."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                Value b = pop();
                Value a = pop();


                if (
                    std::holds_alternative<double>(a) &&
                    std::holds_alternative<double>(b)
                ) {
                    push(
                        std::get<double>(a) *
                        std::get<double>(b)
                    );
                }

                else {
                    runtimeError(
                        "Operands must be numbers."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }

                break;
            }


            // =================================================
            // DIVIDE
            // =================================================

            case OpCode::OP_DIVIDE: {

                if (stack.size() < 2) {
                    runtimeError(
                        "Stack underflow for division."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                Value b = pop();
                Value a = pop();


                if (
                    std::holds_alternative<double>(a) &&
                    std::holds_alternative<double>(b)
                ) {
                    double divisor =
                        std::get<double>(b);


                    if (divisor == 0.0) {
                        runtimeError(
                            "Division by zero."
                        );

                        return
                            InterpretResult::
                                INTERPRET_RUNTIME_ERROR;
                    }


                    push(
                        std::get<double>(a) /
                        divisor
                    );
                }

                else {
                    runtimeError(
                        "Operands must be numbers."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }

                break;
            }


            // =================================================
            // NEGATE
            // =================================================

            case OpCode::OP_NEGATE: {

                if (stack.empty()) {
                    runtimeError(
                        "Stack underflow for negation."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                Value value = pop();


                if (
                    std::holds_alternative<double>(
                        value
                    )
                ) {
                    push(
                        -std::get<double>(value)
                    );
                }

                else {
                    runtimeError(
                        "Operand must be a number."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }

                break;
            }


            // =================================================
            // NOT
            // =================================================

            case OpCode::OP_NOT: {

                if (stack.empty()) {
                    runtimeError(
                        "Stack underflow for logical not."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                Value value = pop();


                bool isFalsy =
                    std::holds_alternative<bool>(value) &&
                    !std::get<bool>(value);


                push(isFalsy);

                break;
            }


            // =================================================
            // PRINT
            // =================================================

            case OpCode::OP_PRINT: {

                if (stack.empty()) {
                    runtimeError(
                        "Stack underflow while printing."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                printValue(pop());

                std::cout << "\n";

                break;
            }


            // =================================================
            // EQUAL
            // =================================================

            case OpCode::OP_EQUAL: {

                if (stack.size() < 2) {
                    runtimeError(
                        "Stack underflow for equality."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                Value b = pop();
                Value a = pop();

                push(a == b);

                break;
            }


            // =================================================
            // GREATER
            // =================================================

            case OpCode::OP_GREATER: {

                if (stack.size() < 2) {
                    runtimeError(
                        "Stack underflow for comparison."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                Value b = pop();
                Value a = pop();


                if (
                    std::holds_alternative<double>(a) &&
                    std::holds_alternative<double>(b)
                ) {
                    push(
                        std::get<double>(a) >
                        std::get<double>(b)
                    );
                }

                else {
                    runtimeError(
                        "Operands must be numbers."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }

                break;
            }


            // =================================================
            // LESS
            // =================================================

            case OpCode::OP_LESS: {

                if (stack.size() < 2) {
                    runtimeError(
                        "Stack underflow for comparison."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                Value b = pop();
                Value a = pop();


                if (
                    std::holds_alternative<double>(a) &&
                    std::holds_alternative<double>(b)
                ) {
                    push(
                        std::get<double>(a) <
                        std::get<double>(b)
                    );
                }

                else {
                    runtimeError(
                        "Operands must be numbers."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }

                break;
            }


            // =================================================
            // JUMP
            // =================================================

            case OpCode::OP_JUMP: {

                if (
                    !checkJumpBytes(
                        2,
                        currentFrame
                    )
                ) {
                    runtimeError(
                        "Invalid jump instruction."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                uint16_t offset =
                    (
                        static_cast<uint16_t>(
                            chunk->code[ip]
                        ) << 8
                    ) |
                    chunk->code[ip + 1];


                ip += 2;


                size_t destination =
                    ip + offset;


                if (
                    destination >
                    chunk->code.size()
                ) {
                    runtimeError(
                        "Jump target out of bounds."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                currentFrame->ip =
                    destination;

                break;
            }


            // =================================================
            // JUMP IF FALSE
            // =================================================

            case OpCode::OP_JUMP_IF_FALSE: {

                if (
                    !checkJumpBytes(
                        2,
                        currentFrame
                    )
                ) {
                    runtimeError(
                        "Invalid conditional jump instruction."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                if (stack.empty()) {
                    runtimeError(
                        "Stack underflow for conditional jump."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                uint16_t offset =
                    (
                        static_cast<uint16_t>(
                            chunk->code[ip]
                        ) << 8
                    ) |
                    chunk->code[ip + 1];


                ip += 2;


                size_t destination =
                    ip + offset;


                if (
                    destination >
                    chunk->code.size()
                ) {
                    runtimeError(
                        "Conditional jump target out of bounds."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                currentFrame->ip = ip;


                Value value =
                    stack.back();


                bool isFalsy =
                    std::holds_alternative<bool>(value) &&
                    !std::get<bool>(value);


                if (isFalsy) {
                    currentFrame->ip =
                        destination;
                }

                break;
            }


            // =================================================
            // LOOP
            // =================================================

            case OpCode::OP_LOOP: {

                if (
                    !checkJumpBytes(
                        2,
                        currentFrame
                    )
                ) {
                    runtimeError(
                        "Invalid loop instruction."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                uint16_t offset =
                    (
                        static_cast<uint16_t>(
                            chunk->code[ip]
                        ) << 8
                    ) |
                    chunk->code[ip + 1];


                ip += 2;


                if (offset > ip) {
                    runtimeError(
                        "Loop target out of bounds."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                currentFrame->ip =
                    ip - offset;

                break;
            }


            // =================================================
            // GET LOCAL
            // =================================================

            case OpCode::OP_GET_LOCAL: {

                if (
                    !checkInstructionBytes(
                        1,
                        currentFrame
                    )
                ) {
                    runtimeError(
                        "Missing local slot operand."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                uint8_t slot =
                    chunk->code[ip++];


                currentFrame->ip = ip;


                if (
                    !checkLocalSlot(
                        slot,
                        currentFrame
                    )
                ) {
                    runtimeError(
                        "Local slot out of bounds."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                size_t targetIndex =
                    currentFrame->slots +
                    slot;


                push(
                    stack[targetIndex]
                );

                break;
            }


            // =================================================
            // SET LOCAL
            // =================================================

            case OpCode::OP_SET_LOCAL: {

                if (
                    !checkInstructionBytes(
                        1,
                        currentFrame
                    )
                ) {
                    runtimeError(
                        "Missing local slot operand."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                if (stack.empty()) {
                    runtimeError(
                        "Stack underflow for local assignment."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                uint8_t slot =
                    chunk->code[ip++];


                currentFrame->ip = ip;


                if (
                    !checkLocalSlot(
                        slot,
                        currentFrame
                    )
                ) {
                    runtimeError(
                        "Local slot out of bounds for assignment."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                size_t targetIndex =
                    currentFrame->slots +
                    slot;


                stack[targetIndex] =
                    stack.back();

                break;
            }


            // =================================================
            // CALL
            // =================================================

            case OpCode::OP_CALL: {

                if (
                    !checkInstructionBytes(
                        1,
                        currentFrame
                    )
                ) {
                    runtimeError(
                        "Missing argument count operand."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                uint8_t argCount =
                    chunk->code[ip++];


                currentFrame->ip = ip;


                if (
                    stack.size() <
                    static_cast<size_t>(argCount) + 1
                ) {
                    runtimeError(
                        "Stack underflow during function call."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                size_t calleeIndex =
                    stack.size() -
                    static_cast<size_t>(argCount) -
                    1;


                Value callee =
                    stack[calleeIndex];


                // -------------------------------------------------
                // Native method
                // -------------------------------------------------

                if (
                    std::holds_alternative<
                        AdiNativeMethod*
                    >(callee)
                ) {
                    AdiNativeMethod* native =
                        std::get<AdiNativeMethod*>(
                            callee
                        );


                    if (native == nullptr) {
                        runtimeError(
                            "Invalid native method."
                        );

                        return
                            InterpretResult::
                                INTERPRET_RUNTIME_ERROR;
                    }


                    std::vector<Value> args;
                    args.reserve(argCount);


                    for (
                        size_t i = 0;
                        i < argCount;
                        i++
                    ) {
                        args.push_back(
                            stack[
                                calleeIndex + 1 + i
                            ]
                        );
                    }


                    stack.resize(calleeIndex);


                    Value result =
                        native->function(
                            native->self,
                            args
                        );


                    push(result);

                    break;
                }


                // -------------------------------------------------
                // Resolve closure
                // -------------------------------------------------

                AdiClosure* closure =
                    nullptr;


                // -------------------------------------------------
                // Bound method
                // -------------------------------------------------

                if (
                    std::holds_alternative<
                        AdiBoundMethod*
                    >(callee)
                ) {
                    AdiBoundMethod* bound =
                        std::get<AdiBoundMethod*>(
                            callee
                        );


                    if (
                        bound == nullptr ||
                        bound->method == nullptr
                    ) {
                        runtimeError(
                            "Invalid bound method."
                        );

                        return
                            InterpretResult::
                                INTERPRET_RUNTIME_ERROR;
                    }


                    stack[calleeIndex] =
                        bound->receiver;


                    closure =
                        allocateObject<AdiClosure>();


                    closure->function =
                        bound->method;
                }


                // -------------------------------------------------
                // Normal closure
                // -------------------------------------------------

                else if (
                    std::holds_alternative<
                        AdiClosure*
                    >(callee)
                ) {
                    closure =
                        std::get<AdiClosure*>(
                            callee
                        );
                }


                // -------------------------------------------------
                // Invalid callable
                // -------------------------------------------------

                else {
                    runtimeError(
                        "Can only call functions and methods."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                // -------------------------------------------------
                // Validate closure
                // -------------------------------------------------

                if (
                    closure == nullptr ||
                    closure->function == nullptr
                ) {
                    runtimeError(
                        "Invalid closure."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                // -------------------------------------------------
                // Argument count
                // -------------------------------------------------

                if (
                    closure->function->arity !=
                    argCount
                ) {
                    runtimeError(
                        "Expected " +
                        std::to_string(
                            closure->function->arity
                        ) +
                        " arguments but got " +
                        std::to_string(
                            argCount
                        ) +
                        "."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                // -------------------------------------------------
                // Frame limit
                // -------------------------------------------------

                if (
                    frameCount >= FRAMES_MAX
                ) {
                    runtimeError(
                        "Stack overflow."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                // -------------------------------------------------
                // Create new frame
                // -------------------------------------------------

                CallFrame* newFrame =
                    &frames[frameCount++];


                newFrame->closure =
                    closure;


                newFrame->ip = 0;


                newFrame->slots =
                    calleeIndex;


                currentFrame =
                    newFrame;


                break;
            }


            // =================================================
            // RETURN
            // =================================================

            case OpCode::OP_RETURN: {

                if (stack.empty()) {
                    runtimeError(
                        "Stack underflow on return."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                if (frameCount <= 0) {
                    runtimeError(
                        "Invalid return frame."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                /*
                 * Save the return value before removing
                 * the returning frame's stack slots.
                 */
                Value result =
                    pop();


                CallFrame* returningFrame =
                    &frames[
                        frameCount - 1
                    ];


                size_t returnSlot =
                    returningFrame->slots;


                // -------------------------------------------------
                // Initializer
                // -------------------------------------------------

                if (
                    returningFrame->closure != nullptr &&
                    returningFrame->closure->function != nullptr &&
                    returningFrame
                        ->closure
                        ->function
                        ->name == "init"
                ) {
                    if (
                        returnSlot <
                        stack.size()
                    ) {
                        result =
                            stack[
                                returnSlot
                            ];
                    }
                }


                // -------------------------------------------------
                // Close upvalues
                // -------------------------------------------------

                if (
                    returnSlot <
                    stack.size()
                ) {
                    closeUpvalues(
                        &stack[
                            returnSlot
                        ]
                    );
                }


                // -------------------------------------------------
                // Remove frame
                // -------------------------------------------------

                frameCount--;


                // -------------------------------------------------
                // Script finished
                // -------------------------------------------------

                if (
                    frameCount == 0
                ) {
                    return
                        InterpretResult::
                            INTERPRET_OK;
                }


                // -------------------------------------------------
                // Validate return slot
                // -------------------------------------------------

                if (
                    returnSlot >
                    stack.size()
                ) {
                    runtimeError(
                        "Invalid return stack slot."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                /*
                 * Remove the returning function's stack area,
                 * keeping only the caller's stack.
                 */
                stack.resize(
                    returnSlot
                );


                /*
                 * Put the result onto the caller's stack.
                 */
                push(result);


                break;
            }


            // =================================================
            // STRUCT INSTANCE
            // =================================================

            case OpCode::OP_STRUCT_INSTANCE: {

                if (
                    !checkInstructionBytes(
                        1,
                        currentFrame
                    )
                ) {
                    runtimeError(
                        "Missing struct argument count."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                uint8_t argCount =
                    chunk->code[ip++];


                currentFrame->ip = ip;


                if (
                    stack.size() <
                    static_cast<size_t>(argCount) + 1
                ) {
                    runtimeError(
                        "Stack underflow while creating struct instance."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                Value blueprintValue =
                    peek(argCount);


                if (
                    !std::holds_alternative<
                        AdiStructDef*
                    >(blueprintValue)
                ) {
                    runtimeError(
                        "Expected a struct definition."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                AdiStructDef* blueprint =
                    std::get<AdiStructDef*>(
                        blueprintValue
                    );


                if (blueprint == nullptr) {
                    runtimeError(
                        "Invalid struct definition."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                AdiInstance* instance =
                    allocateObject<AdiInstance>(
                        blueprint
                    );


                size_t instanceSlot =
                    stack.size() -
                    static_cast<size_t>(argCount) -
                    1;


                stack[instanceSlot] =
                    instance;


                auto initIt =
                    blueprint->methods.find("init");


                if (
                    initIt !=
                    blueprint->methods.end()
                ) {
                    AdiFunction* function =
                        initIt->second;


                    if (function == nullptr) {
                        runtimeError(
                            "Invalid struct initializer."
                        );

                        return
                            InterpretResult::
                                INTERPRET_RUNTIME_ERROR;
                    }


                    if (
                        function->arity !=
                        argCount
                    ) {
                        runtimeError(
                            "Expected " +
                            std::to_string(
                                function->arity
                            ) +
                            " arguments but got " +
                            std::to_string(
                                argCount
                            ) +
                            "."
                        );

                        return
                            InterpretResult::
                                INTERPRET_RUNTIME_ERROR;
                    }


                    if (
                        frameCount >=
                        FRAMES_MAX
                    ) {
                        runtimeError(
                            "Stack overflow."
                        );

                        return
                            InterpretResult::
                                INTERPRET_RUNTIME_ERROR;
                    }


                    AdiClosure* initClosure =
                        allocateObject<AdiClosure>();


                    initClosure->function =
                        function;


                    CallFrame* newFrame =
                        &frames[
                            frameCount++
                        ];


                    newFrame->closure =
                        initClosure;

                    newFrame->ip = 0;

                    newFrame->slots =
                        instanceSlot;


                    currentFrame =
                        newFrame;
                }

                else if (
                    argCount > 0
                ) {
                    runtimeError(
                        "Struct '" +
                        blueprint->name +
                        "' has no init method but received arguments."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }

                break;
            }


            // =================================================
            // GET PROPERTY
            // =================================================

            case OpCode::OP_GET: {

                if (
                    !checkInstructionBytes(
                        1,
                        currentFrame
                    )
                ) {
                    runtimeError(
                        "Missing property name operand."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                if (stack.empty()) {
                    runtimeError(
                        "Stack underflow while getting property."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                uint8_t nameIndex =
                    chunk->code[ip++];


                currentFrame->ip = ip;


                if (
                    !checkConstantIndex(
                        nameIndex,
                        currentFrame
                    )
                ) {
                    runtimeError(
                        "Property name constant index out of bounds."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                Value nameValue =
                    chunk->constants[
                        nameIndex
                    ];


                if (
                    !std::holds_alternative<
                        std::string
                    >(nameValue)
                ) {
                    runtimeError(
                        "Property name must be a string."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                std::string name =
                    std::get<std::string>(
                        nameValue
                    );


                Value targetValue =
                    pop();


                if (
                    !std::holds_alternative<
                        AdiInstance*
                    >(targetValue)
                ) {
                    runtimeError(
                        "Only instances have properties."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                AdiInstance* instance =
                    std::get<AdiInstance*>(
                        targetValue
                    );


                if (instance == nullptr) {
                    runtimeError(
                        "Invalid instance."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                auto fieldIt =
                    instance->fields.find(name);


                if (
                    fieldIt !=
                    instance->fields.end()
                ) {
                    push(fieldIt->second);
                    break;
                }


                auto methodIt =
                    instance
                        ->blueprint
                        ->methods
                        .find(name);


                if (
                    methodIt !=
                    instance
                        ->blueprint
                        ->methods
                        .end()
                ) {
                    AdiFunction* method =
                        methodIt->second;


                    if (method == nullptr) {
                        runtimeError(
                            "Invalid method."
                        );

                        return
                            InterpretResult::
                                INTERPRET_RUNTIME_ERROR;
                    }


                    AdiBoundMethod* bound =
                        allocateObject<
                            AdiBoundMethod
                        >(
                            instance,
                            method
                        );


                    push(bound);

                    break;
                }


                runtimeError(
                    "Undefined property '" +
                    name +
                    "'."
                );

                return
                    InterpretResult::
                        INTERPRET_RUNTIME_ERROR;
            }


            // =================================================
            // SET PROPERTY
            // =================================================

            case OpCode::OP_SET: {

                if (
                    !checkInstructionBytes(
                        1,
                        currentFrame
                    )
                ) {
                    runtimeError(
                        "Missing property name operand."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                if (stack.size() < 2) {
                    runtimeError(
                        "Stack underflow while setting property."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                uint8_t nameIndex =
                    chunk->code[ip++];


                currentFrame->ip = ip;


                if (
                    !checkConstantIndex(
                        nameIndex,
                        currentFrame
                    )
                ) {
                    runtimeError(
                        "Property name constant index out of bounds."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                Value nameValue =
                    chunk->constants[
                        nameIndex
                    ];


                if (
                    !std::holds_alternative<
                        std::string
                    >(nameValue)
                ) {
                    runtimeError(
                        "Property name must be a string."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                std::string name =
                    std::get<std::string>(
                        nameValue
                    );


                Value targetValue =
                    pop();

                Value value =
                    pop();


                if (
                    !std::holds_alternative<
                        AdiInstance*
                    >(targetValue)
                ) {
                    runtimeError(
                        "Only instances have fields."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                AdiInstance* instance =
                    std::get<AdiInstance*>(
                        targetValue
                    );


                if (instance == nullptr) {
                    runtimeError(
                        "Invalid instance."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                instance->fields[name] =
                    value;


                push(value);

                break;
            }


            // =================================================
            // STRUCT
            // =================================================

            case OpCode::OP_STRUCT: {

                if (
                    !checkInstructionBytes(
                        1,
                        currentFrame
                    )
                ) {
                    runtimeError(
                        "Missing struct name operand."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                if (stack.empty()) {
                    runtimeError(
                        "Stack underflow while creating struct."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                uint8_t nameIndex =
                    chunk->code[ip++];


                currentFrame->ip = ip;


                if (
                    !checkConstantIndex(
                        nameIndex,
                        currentFrame
                    )
                ) {
                    runtimeError(
                        "Struct name constant index out of bounds."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                Value nameValue =
                    chunk->constants[
                        nameIndex
                    ];


                if (
                    !std::holds_alternative<
                        std::string
                    >(nameValue)
                ) {
                    runtimeError(
                        "Struct name must be a string."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                std::string name =
                    std::get<std::string>(
                        nameValue
                    );


                Value superclassValue =
                    pop();


                AdiStructDef* superclass =
                    nullptr;


                if (
                    std::holds_alternative<
                        AdiStructDef*
                    >(superclassValue)
                ) {
                    superclass =
                        std::get<AdiStructDef*>(
                            superclassValue
                        );
                }


                AdiStructDef* structDef =
                    allocateObject<AdiStructDef>(
                        name,
                        std::vector<std::string>{},
                        superclass
                    );


                if (superclass != nullptr) {
                    structDef->fields =
                        superclass->fields;

                    structDef->methods =
                        superclass->methods;
                }


                globals[name] =
                    structDef;


                push(structDef);

                break;
            }


            // =================================================
            // SUPER
            // =================================================

            case OpCode::OP_SUPER: {

                if (
                    !checkInstructionBytes(
                        2,
                        currentFrame
                    )
                ) {
                    runtimeError(
                        "Invalid super instruction."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                uint8_t nameIndex =
                    chunk->code[ip++];

                uint8_t argCount =
                    chunk->code[ip++];


                currentFrame->ip = ip;


                if (
                    !checkConstantIndex(
                        nameIndex,
                        currentFrame
                    )
                ) {
                    runtimeError(
                        "Super method name constant index out of bounds."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                if (
                    stack.size() <
                    static_cast<size_t>(argCount) + 2
                ) {
                    runtimeError(
                        "Stack underflow for super call."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                Value methodNameValue =
                    chunk->constants[
                        nameIndex
                    ];


                if (
                    !std::holds_alternative<
                        std::string
                    >(methodNameValue)
                ) {
                    runtimeError(
                        "Super method name must be a string."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                std::string methodName =
                    std::get<std::string>(
                        methodNameValue
                    );


                Value superclassValue =
                    peek(argCount);


                if (
                    !std::holds_alternative<
                        AdiStructDef*
                    >(superclassValue)
                ) {
                    runtimeError(
                        "Invalid superclass."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                AdiStructDef* superclass =
                    std::get<AdiStructDef*>(
                        superclassValue
                    );


                if (superclass == nullptr) {
                    runtimeError(
                        "Invalid superclass."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                auto methodIt =
                    superclass->methods.find(
                        methodName
                    );


                if (
                    methodIt ==
                    superclass->methods.end()
                ) {
                    runtimeError(
                        "Superclass '" +
                        superclass->name +
                        "' has no method '" +
                        methodName +
                        "'."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                AdiFunction* function =
                    methodIt->second;


                if (function == nullptr) {
                    runtimeError(
                        "Invalid superclass method."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                if (
                    function->arity !=
                    argCount
                ) {
                    runtimeError(
                        "Expected " +
                        std::to_string(
                            function->arity
                        ) +
                        " arguments for super method."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                size_t receiverSlot =
                    stack.size() -
                    static_cast<size_t>(argCount) -
                    1;


                Value receiver =
                    peek(argCount + 1);


                stack[receiverSlot] =
                    receiver;


                if (receiverSlot == 0) {
                    runtimeError(
                        "Invalid super call stack layout."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                stack.erase(
                    stack.begin() +
                    receiverSlot -
                    1
                );


                if (
                    frameCount >=
                    FRAMES_MAX
                ) {
                    runtimeError(
                        "Stack overflow."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                AdiClosure* superClosure =
                    allocateObject<AdiClosure>();


                superClosure->function =
                    function;


                CallFrame* newFrame =
                    &frames[
                        frameCount++
                    ];


                newFrame->closure =
                    superClosure;

                newFrame->ip = 0;

                newFrame->slots =
                    stack.size() -
                    static_cast<size_t>(argCount) -
                    1;


                currentFrame =
                    newFrame;

                break;
            }


            // =================================================
            // ARRAY
            // =================================================

            case OpCode::OP_ARRAY: {

                if (
                    !checkInstructionBytes(
                        1,
                        currentFrame
                    )
                ) {
                    runtimeError(
                        "Missing array element count."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                uint8_t elementCount =
                    chunk->code[ip++];


                currentFrame->ip = ip;


                if (
                    stack.size() < elementCount
                ) {
                    runtimeError(
                        "Stack underflow while creating array."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                std::vector<Value> elements(
                    elementCount
                );


                for (
                    int i =
                        static_cast<int>(
                            elementCount
                        ) - 1;
                    i >= 0;
                    i--
                ) {
                    elements[i] =
                        pop();
                }


                AdiArray* array =
                    allocateObject<AdiArray>(
                        std::move(elements)
                    );


                push(array);

                break;
            }


            // =================================================
            // MAP
            // =================================================

            case OpCode::OP_MAP: {

                if (
                    !checkInstructionBytes(
                        1,
                        currentFrame
                    )
                ) {
                    runtimeError(
                        "Missing map pair count."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                uint8_t pairCount =
                    chunk->code[ip++];


                currentFrame->ip = ip;


                size_t requiredValues =
                    static_cast<size_t>(pairCount) * 2;


                if (
                    stack.size() <
                    requiredValues
                ) {
                    runtimeError(
                        "Stack underflow while creating map."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                std::unordered_map<
                    std::string,
                    Value
                > entries;


                for (
                    int i = 0;
                    i < pairCount;
                    i++
                ) {
                    Value value =
                        pop();

                    Value keyValue =
                        pop();


                    if (
                        !std::holds_alternative<
                            std::string
                        >(keyValue)
                    ) {
                        runtimeError(
                            "Map key must be a string."
                        );

                        return
                            InterpretResult::
                                INTERPRET_RUNTIME_ERROR;
                    }


                    std::string key =
                        std::get<std::string>(
                            keyValue
                        );


                    entries[key] =
                        value;
                }


                AdiMap* map =
                    allocateObject<AdiMap>(
                        std::move(entries)
                    );


                push(map);

                break;
            }


            // =================================================
            // INDEX GET
            // =================================================

            case OpCode::OP_INDEX_GET: {

                if (stack.size() < 2) {
                    runtimeError(
                        "Stack underflow for index access."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                Value indexValue =
                    pop();

                Value targetValue =
                    pop();


                // -------------------------------------------------
                // Map
                // -------------------------------------------------

                if (
                    std::holds_alternative<
                        AdiMap*
                    >(targetValue)
                ) {
                    if (
                        !std::holds_alternative<
                            std::string
                        >(indexValue)
                    ) {
                        runtimeError(
                            "Map key must be a string."
                        );

                        return
                            InterpretResult::
                                INTERPRET_RUNTIME_ERROR;
                    }


                    AdiMap* map =
                        std::get<AdiMap*>(
                            targetValue
                        );


                    if (map == nullptr) {
                        runtimeError(
                            "Invalid map."
                        );

                        return
                            InterpretResult::
                                INTERPRET_RUNTIME_ERROR;
                    }


                    std::string key =
                        std::get<std::string>(
                            indexValue
                        );


                    auto it =
                        map->entries.find(key);


                    if (
                        it ==
                        map->entries.end()
                    ) {
                        runtimeError(
                            "Key '" +
                            key +
                            "' not found in map."
                        );

                        return
                            InterpretResult::
                                INTERPRET_RUNTIME_ERROR;
                    }


                    push(it->second);

                    break;
                }


                // -------------------------------------------------
                // Array
                // -------------------------------------------------

                if (
                    !std::holds_alternative<
                        AdiArray*
                    >(targetValue)
                ) {
                    runtimeError(
                        "Only arrays and maps can be indexed."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                if (
                    !std::holds_alternative<
                        double
                    >(indexValue)
                ) {
                    runtimeError(
                        "Array index must be a number."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                AdiArray* array =
                    std::get<AdiArray*>(
                        targetValue
                    );


                if (array == nullptr) {
                    runtimeError(
                        "Invalid array."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                int index =
                    static_cast<int>(
                        std::get<double>(
                            indexValue
                        )
                    );


                if (
                    index < 0 ||
                    static_cast<size_t>(index) >=
                        array->elements.size()
                ) {
                    runtimeError(
                        "Array index out of bounds."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                push(
                    array->elements[index]
                );

                break;
            }


            // =================================================
            // INDEX SET
            // =================================================

            case OpCode::OP_INDEX_SET: {

                if (stack.size() < 3) {
                    runtimeError(
                        "Stack underflow for index assignment."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                Value indexValue =
                    pop();

                Value targetValue =
                    pop();

                Value value =
                    pop();


                // -------------------------------------------------
                // Map
                // -------------------------------------------------

                if (
                    std::holds_alternative<
                        AdiMap*
                    >(targetValue)
                ) {
                    if (
                        !std::holds_alternative<
                            std::string
                        >(indexValue)
                    ) {
                        runtimeError(
                            "Map key must be a string."
                        );

                        return
                            InterpretResult::
                                INTERPRET_RUNTIME_ERROR;
                    }


                    AdiMap* map =
                        std::get<AdiMap*>(
                            targetValue
                        );


                    if (map == nullptr) {
                        runtimeError(
                            "Invalid map."
                        );

                        return
                            InterpretResult::
                                INTERPRET_RUNTIME_ERROR;
                    }


                    std::string key =
                        std::get<std::string>(
                            indexValue
                        );


                    map->entries[key] =
                        value;


                    push(value);

                    break;
                }


                // -------------------------------------------------
                // Array
                // -------------------------------------------------

                if (
                    !std::holds_alternative<
                        AdiArray*
                    >(targetValue)
                ) {
                    runtimeError(
                        "Only arrays and maps can be assigned by index."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                if (
                    !std::holds_alternative<
                        double
                    >(indexValue)
                ) {
                    runtimeError(
                        "Array index must be a number."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                AdiArray* array =
                    std::get<AdiArray*>(
                        targetValue
                    );


                if (array == nullptr) {
                    runtimeError(
                        "Invalid array."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                int index =
                    static_cast<int>(
                        std::get<double>(
                            indexValue
                        )
                    );


                if (
                    index < 0 ||
                    static_cast<size_t>(index) >=
                        array->elements.size()
                ) {
                    runtimeError(
                        "Array assignment index out of bounds."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                array->elements[index] =
                    value;


                push(value);

                break;
            }


            // =================================================
            // CLOSURE
            // =================================================

            case OpCode::OP_CLOSURE: {

                /*
                 * IMPORTANT:
                 *
                 * `ip` is the working instruction pointer for
                 * the current frame.
                 *
                 * Do NOT read the operand using
                 * currentFrame->ip and then overwrite it with
                 * the old `ip`.
                 */

                if (
                    !checkInstructionBytes(
                        1,
                        currentFrame
                    )
                ) {
                    runtimeError(
                        "Missing closure function constant."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                uint8_t constantIndex =
                    chunk->code[ip++];


                currentFrame->ip =
                    ip;


                // -------------------------------------------------
                // Validate function constant
                // -------------------------------------------------

                if (
                    !checkConstantIndex(
                        constantIndex,
                        currentFrame
                    )
                ) {
                    runtimeError(
                        "Closure function constant index out of bounds."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                Value constant =
                    chunk->constants[
                        constantIndex
                    ];


                if (
                    !std::holds_alternative<
                        AdiFunction*
                    >(constant)
                ) {
                    runtimeError(
                        "Closure constant is not a function."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                AdiFunction* function =
                    std::get<AdiFunction*>(
                        constant
                    );


                if (function == nullptr) {
                    runtimeError(
                        "Invalid closure function."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                // -------------------------------------------------
                // Create closure
                // -------------------------------------------------

                AdiClosure* closure =
                    allocateObject<AdiClosure>();


                closure->function =
                    function;


                /*
                 * IMPORTANT FOR RECURSIVE LOCAL FUNCTIONS
                 *
                 * The compiler reserves a local slot for a
                 * local function before compiling its body.
                 *
                 * Example:
                 *
                 * fn makeCounter() {
                 *     fn countDown(n) {
                 *         return countDown(n - 1);
                 *     }
                 *
                 *     return countDown;
                 * }
                 *
                 * The compiler has already reserved the
                 * countDown local slot.
                 *
                 * At runtime the closure must occupy that
                 * slot before its recursive upvalue is captured.
                 */

                size_t closureSlot =
                    stack.size();


                /*
                 * Push the closure FIRST.
                 *
                 * This makes the reserved recursive local
                 * slot physically exist on the VM stack.
                 */
                push(closure);


                // -------------------------------------------------
                // Resolve captured upvalues
                // -------------------------------------------------

                for (
                    int i = 0;
                    i < function->upvalueCount;
                    i++
                ) {

                    if (
                        !checkInstructionBytes(
                            2,
                            currentFrame
                        )
                    ) {
                        runtimeError(
                            "Incomplete closure upvalue descriptor."
                        );

                        return
                            InterpretResult::
                                INTERPRET_RUNTIME_ERROR;
                    }


                    /*
                     * Read both descriptor bytes from the
                     * working `ip`.
                     */
                    bool isLocal =
                        chunk->code[ip++] == 1;


                    uint8_t index =
                        chunk->code[ip++];


                    currentFrame->ip =
                        ip;


                    // -------------------------------------------------
                    // Capture local variable
                    // -------------------------------------------------

                    if (isLocal) {

                        size_t localIndex =
                            currentFrame->slots +
                            index;


                        /*
                         * Normal local capture.
                         */
                        if (
                            localIndex <
                            stack.size()
                        ) {

                            AdiUpvalue* upvalue =
                                captureUpvalue(
                                    &stack[
                                        localIndex
                                    ]
                                );


                            closure->upvalues.push_back(
                                upvalue
                            );
                        }


                        /*
                         * Recursive local function.
                         *
                         * The compiler reserved the local slot
                         * and the closure now occupies it.
                         */
                        else if (
                            localIndex ==
                            closureSlot
                        ) {

                            AdiUpvalue* upvalue =
                                captureUpvalue(
                                    &stack[
                                        closureSlot
                                    ]
                                );


                            closure->upvalues.push_back(
                                upvalue
                            );
                        }


                        else {

                            runtimeError(
                                "Upvalue local index out of bounds."
                            );

                            return
                                InterpretResult::
                                    INTERPRET_RUNTIME_ERROR;
                        }
                    }


                    // -------------------------------------------------
                    // Capture enclosing upvalue
                    // -------------------------------------------------

                    else {

                        if (
                            currentFrame->closure ==
                            nullptr
                        ) {
                            runtimeError(
                                "Invalid enclosing closure."
                            );

                            return
                                InterpretResult::
                                    INTERPRET_RUNTIME_ERROR;
                        }


                        if (
                            index >=
                            currentFrame
                                ->closure
                                ->upvalues
                                .size()
                        ) {
                            runtimeError(
                                "Upvalue index out of bounds."
                            );

                            return
                                InterpretResult::
                                    INTERPRET_RUNTIME_ERROR;
                        }


                        closure->upvalues.push_back(
                            currentFrame
                                ->closure
                                ->upvalues[
                                    index
                                ]
                        );
                    }
                }


                /*
                 * The closure is already on the stack.
                 *
                 * Do NOT push it again.
                 */

                break;
            }

            // =================================================
            // GET UPVALUE
            // =================================================

            case OpCode::OP_GET_UPVALUE: {

                if (
                    !checkInstructionBytes(
                        1,
                        currentFrame
                    )
                ) {
                    runtimeError(
                        "Missing upvalue slot operand."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }

                uint8_t slot =
                    chunk->code[ip++];

                currentFrame->ip = ip;

                if (
                    !checkUpvalueSlot(
                        slot,
                        currentFrame
                    )
                ) {
                    runtimeError(
                        "Upvalue slot out of bounds."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }

                AdiUpvalue* upvalue =
                    currentFrame
                        ->closure
                        ->upvalues[
                            slot
                        ];

                if (
                    upvalue == nullptr ||
                    upvalue->location == nullptr
                ) {
                    runtimeError(
                        "Invalid upvalue."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }

                push(
                    *upvalue->location
                );

                break;
            }



            // =================================================
            // SET UPVALUE
            // =================================================

            case OpCode::OP_SET_UPVALUE: {

                if (
                    !checkInstructionBytes(
                        1,
                        currentFrame
                    )
                ) {
                    runtimeError(
                        "Missing upvalue slot operand."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                if (stack.empty()) {
                    runtimeError(
                        "Stack underflow for upvalue assignment."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                uint8_t slot =
                    chunk->code[ip++];


                currentFrame->ip = ip;


                if (
                    !checkUpvalueSlot(
                        slot,
                        currentFrame
                    )
                ) {
                    runtimeError(
                        "Upvalue slot out of bounds."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                AdiUpvalue* upvalue =
                    currentFrame
                        ->closure
                        ->upvalues[
                            slot
                        ];


                if (
                    upvalue == nullptr ||
                    upvalue->location == nullptr
                ) {
                    runtimeError(
                        "Invalid upvalue."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                *upvalue->location =
                    stack.back();

                break;
            }


            // =================================================
            // CLOSE UPVALUE
            // =================================================

            case OpCode::OP_CLOSE_UPVALUE: {

                if (stack.empty()) {
                    runtimeError(
                        "Stack underflow while closing upvalue."
                    );

                    return
                        InterpretResult::
                            INTERPRET_RUNTIME_ERROR;
                }


                closeUpvalues(
                    &stack.back()
                );


                pop();

                break;
            }

            


            // =================================================
            // UNKNOWN OPCODE
            // =================================================

            default: {

                runtimeError(
                    "Unknown opcode execution error."
                );

                return
                    InterpretResult::
                        INTERPRET_RUNTIME_ERROR;
            }
        }
    }


    return
        InterpretResult::
            INTERPRET_OK;
}