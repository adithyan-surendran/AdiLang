#ifndef ADILANG_COMPILER_H
#define ADILANG_COMPILER_H

#include "chunk.h"
#include "object.h"
#include "ast.h"
#include "vm.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <cstdint>
#include <utility>

enum class FunctionType {
    TYPE_FUNCTION,
    TYPE_SCRIPT,
    TYPE_METHOD,
    TYPE_INITIALIZER
};

struct Local {
    std::string name;
    int depth;
    bool isCaptured;
};

struct CompilerUpvalue {
    uint8_t index;
    bool isLocal;
};

struct FunctionCompiler {
    FunctionCompiler* enclosing = nullptr;
    AdiFunction* function = nullptr;
    FunctionType type = FunctionType::TYPE_SCRIPT;
    int arity = 0;

    std::vector<Local> locals;
    std::vector<CompilerUpvalue> upvalues;

    int scopeDepth = 0;
};

struct ClassCompiler {
    ClassCompiler* enclosing = nullptr;
    bool hasSuperclass = false;
    std::string superclassName;
};

class Compiler {
private:
    int currentLine = 0;

    VM* vm = nullptr;

    FunctionCompiler* current = nullptr;
    ClassCompiler* currentClass = nullptr;

    std::unordered_map<std::string, AdiStructDef*> definedStructs;

    Value loadNativeModule(const std::string& name);

    void compileNode(const Stmt* stmt);
    void compileExpression(const Expr* expr);

    // ============================================================
    // Function compiler
    // ============================================================

    void initFunction(
        AdiFunction* function,
        FunctionType type
    ) {
        if (function == nullptr) {
            throw std::runtime_error(
                "Compiler Error: Cannot initialize a null function."
            );
        }

        current = new FunctionCompiler{
            current,
            function,
            type,
            0,
            {},
            {},
            0
        };

        /*
         * Slot 0 is reserved for the callee for normal
         * functions.
         *
         * For methods and initializers, slot 0 is `this`.
         */
        if (type != FunctionType::TYPE_SCRIPT) {

            Local local;

            if (
                type == FunctionType::TYPE_METHOD ||
                type == FunctionType::TYPE_INITIALIZER
            ) {
                local.name = "this";
            }
            else {
                /*
                 * Normal functions reserve slot 0 for
                 * the callee.
                 */
                local.name = "";
            }

            local.depth = 0;
            local.isCaptured = false;

            current->locals.push_back(
                std::move(local)
            );
        }
    }

    // ============================================================
    // Current chunk
    // ============================================================

    Chunk* currentChunk() {
        if (
            current == nullptr ||
            current->function == nullptr
        ) {
            throw std::runtime_error(
                "Compiler Error: No active function compiler."
            );
        }

        if (!current->function->chunk) {
            throw std::runtime_error(
                "Compiler Error: Function has no bytecode chunk."
            );
        }

        return current->function->chunk.get();
    }

    // ============================================================
    // Bytecode
    // ============================================================

    void emitByte(
        uint8_t byte,
        int line = 0
    ) {
        currentChunk()->write(
            byte,
            line
        );
    }

    void emitBytes(
        uint8_t byte1,
        uint8_t byte2,
        int line = 0
    ) {
        emitByte(
            byte1,
            line
        );

        emitByte(
            byte2,
            line
        );
    }

    uint8_t makeConstant(
        Value value
    ) {
        int constant =
            currentChunk()->addConstant(
                value
            );

        if (constant > UINT8_MAX) {
            throw std::runtime_error(
                "Too many constants in one chunk."
            );
        }

        return static_cast<uint8_t>(
            constant
        );
    }

    void emitConstant(
        Value value,
        int line = 0
    ) {
        emitBytes(
            static_cast<uint8_t>(
                OpCode::OP_CONSTANT
            ),
            makeConstant(
                std::move(value)
            ),
            line
        );
    }

    void emitReturn(
        int line = 0
    ) {
        emitByte(
            static_cast<uint8_t>(
                OpCode::OP_NIL
            ),
            line
        );

        emitByte(
            static_cast<uint8_t>(
                OpCode::OP_RETURN
            ),
            line
        );
    }

    // ============================================================
    // End compiler
    // ============================================================

    AdiFunction* endCompiler(
        int line = 0
    ) {
        if (current == nullptr) {
            throw std::runtime_error(
                "Compiler Error: No active compiler to end."
            );
        }

        emitReturn(
            line
        );

        AdiFunction* function =
            current->function;

        FunctionCompiler* enclosing =
            current->enclosing;

        delete current;

        current =
            enclosing;

        return function;
    }

    // ============================================================
    // Scope
    // ============================================================

    void beginScope() {
        if (current == nullptr) {
            throw std::runtime_error(
                "Compiler Error: No active compiler."
            );
        }

        current->scopeDepth++;
    }

    void endScope(
        int line = 0
    ) {
        if (current == nullptr) {
            throw std::runtime_error(
                "Compiler Error: No active compiler."
            );
        }

        if (current->scopeDepth <= 0) {
            throw std::runtime_error(
                "Compiler Error: Cannot end global scope."
            );
        }

        current->scopeDepth--;

        while (
            !current->locals.empty() &&
            current->locals.back().depth >
                current->scopeDepth
        ) {

            if (
                current->locals.back().isCaptured
            ) {
                emitByte(
                    static_cast<uint8_t>(
                        OpCode::OP_CLOSE_UPVALUE
                    ),
                    line
                );
            }
            else {
                emitByte(
                    static_cast<uint8_t>(
                        OpCode::OP_POP
                    ),
                    line
                );
            }

            current->locals.pop_back();
        }
    }

    // ============================================================
    // Local resolution
    // ============================================================

    int resolveLocal(
        FunctionCompiler* compiler,
        const std::string& name
    ) {
        if (compiler == nullptr) {
            return -1;
        }

        for (
            int i =
                static_cast<int>(
                    compiler->locals.size()
                ) - 1;
            i >= 0;
            i--
        ) {

            Local& local =
                compiler->locals[
                    static_cast<size_t>(i)
                ];

            if (local.name == name) {

                if (local.depth == -1) {
                    throw std::runtime_error(
                        "Can't read local variable "
                        "in its own initializer."
                    );
                }

                return i;
            }
        }

        return -1;
    }

    // ============================================================
    // Local variables
    // ============================================================

    void addLocal(
        std::string name
    ) {
        if (current == nullptr) {
            throw std::runtime_error(
                "Compiler Error: No active compiler."
            );
        }

        if (current->locals.size() >= 256) {
            throw std::runtime_error(
                "Too many local variables in function."
            );
        }

        Local local;

        local.name =
            std::move(name);

        local.depth = -1;
        local.isCaptured = false;

        current->locals.push_back(
            std::move(local)
        );
    }

    void declareVariable(
        const std::string& name
    ) {
        if (current == nullptr) {
            throw std::runtime_error(
                "Compiler Error: No active compiler."
            );
        }

        /*
         * Only check variables in the current
         * lexical scope.
         */
        for (
            int i =
                static_cast<int>(
                    current->locals.size()
                ) - 1;
            i >= 0;
            i--
        ) {

            Local& local =
                current->locals[
                    static_cast<size_t>(i)
                ];

            /*
             * We reached an outer scope.
             */
            if (
                local.depth != -1 &&
                local.depth <
                    current->scopeDepth
            ) {
                break;
            }

            if (name == local.name) {
                throw std::runtime_error(
                    "Variable with this name already "
                    "declared in this scope."
                );
            }
        }

        addLocal(
            name
        );
    }

    // ============================================================
    // Parse variable declaration
    // ============================================================

    uint8_t parseVariable(
        const std::string& name
    ) {
        if (current == nullptr) {
            throw std::runtime_error(
                "Compiler Error: No active compiler."
            );
        }

        /*
         * Local declarations.
         *
         * NOTE:
         * This function should only be used for a NEW
         * local declaration.
         */
        if (current->scopeDepth > 0) {

            declareVariable(
                name
            );

            return 0;
        }

        /*
         * Global variable.
         */
        return makeConstant(
            name
        );
    }

    // ============================================================
    // Mark initialized
    // ============================================================

    void markInitialized() {
        if (
            current == nullptr ||
            current->locals.empty()
        ) {
            return;
        }

        current->locals.back().depth =
            current->scopeDepth;
    }

    // ============================================================
    // Define variable
    // ============================================================

    void defineVariable(
        uint8_t global,
        int line = 0
    ) {
        if (current == nullptr) {
            throw std::runtime_error(
                "Compiler Error: No active compiler."
            );
        }

        /*
         * Local value is already on the VM stack.
         */
        if (current->scopeDepth > 0) {

            markInitialized();

            return;
        }

        /*
         * Global variable.
         */
        emitBytes(
            static_cast<uint8_t>(
                OpCode::OP_DEFINE_GLOBAL
            ),
            global,
            line
        );
    }

    // ============================================================
    // Jumps
    // ============================================================

    int emitJump(
        uint8_t instruction
    ) {
        emitByte(
            instruction
        );

        emitByte(
            0xff
        );

        emitByte(
            0xff
        );

        return static_cast<int>(
            currentChunk()->code.size()
        ) - 2;
    }

    void patchJump(
        int offset
    ) {
        if (offset < 0) {
            throw std::runtime_error(
                "Compiler Error: Invalid jump offset."
            );
        }

        int jump =
            static_cast<int>(
                currentChunk()->code.size()
            ) - offset - 2;

        if (jump > UINT16_MAX) {
            throw std::runtime_error(
                "Too much code to jump over."
            );
        }

        currentChunk()->code[
            static_cast<size_t>(offset)
        ] =
            static_cast<uint8_t>(
                (jump >> 8) & 0xff
            );

        currentChunk()->code[
            static_cast<size_t>(offset + 1)
        ] =
            static_cast<uint8_t>(
                jump & 0xff
            );
    }

    // ============================================================
    // Loops
    // ============================================================

    void emitLoop(
        int loopStart
    ) {
        if (loopStart < 0) {
            throw std::runtime_error(
                "Compiler Error: Invalid loop start."
            );
        }

        emitByte(
            static_cast<uint8_t>(
                OpCode::OP_LOOP
            )
        );

        int offset =
            static_cast<int>(
                currentChunk()->code.size()
            ) - loopStart + 2;

        if (offset > UINT16_MAX) {
            throw std::runtime_error(
                "Loop body too large."
            );
        }

        emitByte(
            static_cast<uint8_t>(
                (offset >> 8) & 0xff
            )
        );

        emitByte(
            static_cast<uint8_t>(
                offset & 0xff
            )
        );
    }

public:

    explicit Compiler(
        VM* vm
    );

    bool compile(
        const Program* program,
        Chunk* chunk
    );

    // ============================================================
    // Upvalues
    // ============================================================

    int addUpvalue(
        FunctionCompiler* compiler,
        uint8_t index,
        bool isLocal
    ) {
        if (compiler == nullptr) {
            throw std::runtime_error(
                "Compiler Error: Cannot add upvalue "
                "to null compiler."
            );
        }

        int count =
            static_cast<int>(
                compiler->upvalues.size()
            );

        /*
         * Reuse an existing upvalue.
         */
        for (
            int i = 0;
            i < count;
            i++
        ) {

            CompilerUpvalue& upvalue =
                compiler->upvalues[
                    static_cast<size_t>(i)
                ];

            if (
                upvalue.index == index &&
                upvalue.isLocal == isLocal
            ) {
                return i;
            }
        }

        if (count >= 256) {
            throw std::runtime_error(
                "Too many closure variables in function."
            );
        }

        compiler->upvalues.push_back(
            CompilerUpvalue{
                index,
                isLocal
            }
        );

        return count;
    }

    int resolveUpvalue(
        FunctionCompiler* compiler,
        const std::string& name
    ) {
        if (
            compiler == nullptr ||
            compiler->enclosing == nullptr
        ) {
            return -1;
        }

        /*
         * First search the immediately enclosing
         * compiler for a local.
         */
        int local =
            resolveLocal(
                compiler->enclosing,
                name
            );

        if (local != -1) {

            compiler->enclosing
                ->locals[
                    static_cast<size_t>(local)
                ]
                .isCaptured = true;

            return addUpvalue(
                compiler,
                static_cast<uint8_t>(local),
                true
            );
        }

        /*
         * Search further up the compiler chain.
         */
        int upvalue =
            resolveUpvalue(
                compiler->enclosing,
                name
            );

        if (upvalue != -1) {

            return addUpvalue(
                compiler,
                static_cast<uint8_t>(upvalue),
                false
            );
        }

        return -1;
    }
};

#endif // ADILANG_COMPILER_H