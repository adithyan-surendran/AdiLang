#ifndef ADILANG_COMPILER_H
#define ADILANG_COMPILER_H

#include "ast.h"
#include "chunk.h"
#include "object.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <stdexcept>

class VM;

struct Local {
    std::string name;
    int depth;
};
struct CompilerUpvalue {
    uint8_t index;
    bool isLocal;
};

class Compiler {
private:
    VM* vm;
    std::unordered_map<std::string, AdiStructDef*> definedStructs;
    
    struct ClassCompiler {
        ClassCompiler* enclosing = nullptr;
        bool hasSuperclass = false; 
        std::string superclassName;
    };

    struct FunctionCompiler {
        FunctionCompiler* enclosing = nullptr;
        AdiFunction* function;
        int arity = 0;
        std::vector<Local> locals;
        std::vector<CompilerUpvalue> upvalues;
        int scopeDepth = 0;
    };

    FunctionCompiler* current = nullptr;
    ClassCompiler* currentClass = nullptr;

    void initFunction(AdiFunction* function) {
        current = new FunctionCompiler{current, function, 0, {}, {}, 0};
        Local local;
        local.name = "";
        local.depth = 0;
        current->locals.push_back(local);
    }

    Chunk* currentChunk() {
        return current->function->chunk.get();
    }

    void emitByte(uint8_t byte, int line = 0) {
        currentChunk()->write(byte, line);
    }

    void emitBytes(uint8_t byte1, uint8_t byte2, int line = 0) {
        emitByte(byte1, line);
        emitByte(byte2, line);
    }

    uint8_t makeConstant(Value value) {
        int constant = currentChunk()->addConstant(value);
        if (constant > UINT8_MAX) {
            throw std::runtime_error("Too many constants in one chunk.");
        }
        return static_cast<uint8_t>(constant);
    }

    void emitConstant(Value value, int line = 0) {
        emitBytes(static_cast<uint8_t>(OpCode::OP_CONSTANT), makeConstant(value), line);
    }

    void emitReturn(int line = 0) {
        emitByte(static_cast<uint8_t>(OpCode::OP_NIL), line);
        emitByte(static_cast<uint8_t>(OpCode::OP_RETURN), line);
    }

    AdiFunction* endCompiler(int line = 0) {
        emitReturn(line);
        AdiFunction* function = current->function;
        delete current;
        current = nullptr;
        return function;
    }

    void beginScope() {
        current->scopeDepth++;
    }

    void endScope(int line = 0) {
        current->scopeDepth--;
        while (!current->locals.empty() && current->locals.back().depth > current->scopeDepth) {
            emitByte(static_cast<uint8_t>(OpCode::OP_POP), line);
            current->locals.pop_back();
        }
    }

    int resolveLocal(FunctionCompiler* compiler, const std::string& name) {
        for (int i = static_cast<int>(compiler->locals.size()) - 1; i >= 0; i--) {
            Local* local = &compiler->locals[i];
            if (local->name == name) {
                if (local->depth == -1) {
                    throw std::runtime_error("Can't read local variable in its own initializer.");
                }
                return i;
            }
        }
        return -1;
    }

    void addLocal(std::string name) {
        if (current->locals.size() >= 256) {
            throw std::runtime_error("Too many local variables in function.");
        }
        Local local;
        local.name = name;
        local.depth = -1;
        current->locals.push_back(local);
    }

    void declareVariable(const std::string& name) {
        if (current->scopeDepth == 0) return;
        for (int i = static_cast<int>(current->locals.size()) - 1; i >= 0; i--) {
            Local* local = &current->locals[i];
            if (local->depth != -1 && local->depth < current->scopeDepth) {
                break;
            }
            if (name == local->name) {
                throw std::runtime_error("Variable with this name already declared in this scope.");
            }
        }
        addLocal(name);
    }

    uint8_t parseVariable(const std::string& name) {
        if (current->scopeDepth > 0) {
            declareVariable(name);
            return 0;
        }
        return makeConstant(name);
    }

    void markInitialized() {
        if (current->scopeDepth == 0) return;
        current->locals.back().depth = current->scopeDepth;
    }

    void defineVariable(uint8_t global, int line = 0) {
        if (current->scopeDepth > 0) {
            markInitialized();
            return;
        }
        emitBytes(static_cast<uint8_t>(OpCode::OP_DEFINE_GLOBAL), global, line);
    }

    void compileNode(const Stmt* stmt);
    void compileExpression(const Expr* expr); // defined inline or in cpp

    int emitJump(uint8_t instruction) {
        emitByte(instruction);
        emitByte(0xff);
        emitByte(0xff);
        return static_cast<int>(currentChunk()->code.size()) - 2;
    }

    void patchJump(int offset) {
        int jump = static_cast<int>(currentChunk()->code.size()) - offset - 2;
        if (jump > UINT16_MAX) {
            throw std::runtime_error("Too much code to jump over.");
        }
        currentChunk()->code[offset] = (jump >> 8) & 0xff;
        currentChunk()->code[offset + 1] = jump & 0xff;
    }

    void emitLoop(int loopStart) {
        emitByte(static_cast<uint8_t>(OpCode::OP_LOOP));
        int offset = static_cast<int>(currentChunk()->code.size()) - loopStart + 2;
        if (offset > UINT16_MAX) {
            throw std::runtime_error("Loop body too large.");
        }
        emitByte((offset >> 8) & 0xff);
        emitByte(offset & 0xff);
    }

public:
    explicit Compiler(VM* vm);
    bool compile(const Program* program, Chunk* chunk);

    int addUpvalue(FunctionCompiler* compiler, uint8_t index, bool isLocal) {
        int count = static_cast<int>(compiler->upvalues.size());
        for (int i = 0; i < count; i++) {
            CompilerUpvalue* upvalue = &compiler->upvalues[i];
            if (upvalue->index == index && upvalue->isLocal == isLocal) {
                return i;
            }
        }

        if (count >= 256) {
            throw std::runtime_error("Too many closure variables in function.");
        }

        compiler->upvalues.push_back(CompilerUpvalue{index, isLocal});
        return count;
    }

    int resolveUpvalue(FunctionCompiler* compiler, const std::string& name) {
        if (compiler->enclosing == nullptr) return -1;

        int local = resolveLocal(compiler->enclosing, name);
        if (local != -1) {
            return addUpvalue(compiler, static_cast<uint8_t>(local), true);
        }

        int upvalue = resolveUpvalue(compiler->enclosing, name);
        if (upvalue != -1) {
            return addUpvalue(compiler, static_cast<uint8_t>(upvalue), false);
        }

        return -1;
    }
};

#endif // ADILANG_COMPILER_H