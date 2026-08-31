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

struct Local {
    std::string name;
    int depth;
};

class Compiler {
private:
    struct ClassCompiler {
        ClassCompiler* enclosing = nullptr;
    };

    struct FunctionCompiler {
        std::shared_ptr<AdiFunction> function;
        int arity = 0;
        std::vector<Local> locals;
        int scopeDepth = 0;
    };

    FunctionCompiler* current = nullptr;
    ClassCompiler* currentClass = nullptr;

    void initFunction(std::shared_ptr<AdiFunction> function) {
        current = new FunctionCompiler{function, 0, {}, 0};
        Local local;
        local.name = "";
        local.depth = 0;
        current->locals.push_back(local);
    }

    Chunk* currentChunk() {
        return &current->function->chunk;
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

    std::shared_ptr<AdiFunction> endCompiler(int line = 0) {
        emitReturn(line);
        std::shared_ptr<AdiFunction> function = current->function;
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

    void compileNode(const Stmt* stmt) {
        if (auto varDecl = dynamic_cast<const VariableDeclaration*>(stmt)) {
            compileExpression(varDecl->initializer.get());
            uint8_t global = parseVariable(varDecl->name);
            defineVariable(global);
        }
        else if (auto printStmt = dynamic_cast<const PrintStatement*>(stmt)) {
            compileExpression(printStmt->expression.get());
            emitByte(static_cast<uint8_t>(OpCode::OP_PRINT));
        }
        else if (auto exprStmt = dynamic_cast<const ExpressionStatement*>(stmt)) {
            compileExpression(exprStmt->expression.get());
            emitByte(static_cast<uint8_t>(OpCode::OP_POP));
        }
        else if (auto blockStmt = dynamic_cast<const BlockStatement*>(stmt)) {
            beginScope();
            for (const auto& s : blockStmt->statements) {
                compileNode(s.get());
            }
            endScope();
        }
        else if (auto ifStmt = dynamic_cast<const IfStatement*>(stmt)) {
            compileExpression(ifStmt->condition.get());
            int thenJump = emitJump(static_cast<uint8_t>(OpCode::OP_JUMP_IF_FALSE));
            emitByte(static_cast<uint8_t>(OpCode::OP_POP));

            compileNode(ifStmt->thenBranch.get());
            int elseJump = emitJump(static_cast<uint8_t>(OpCode::OP_JUMP));

            patchJump(thenJump);
            emitByte(static_cast<uint8_t>(OpCode::OP_POP));

            if (ifStmt->elseBranch != nullptr) {
                compileNode(ifStmt->elseBranch.get());
            }
            patchJump(elseJump);
        }
        else if (auto whileStmt = dynamic_cast<const WhileStatement*>(stmt)) {
            int loopStart = static_cast<int>(currentChunk()->code.size());
            compileExpression(whileStmt->condition.get());

            int exitJump = emitJump(static_cast<uint8_t>(OpCode::OP_JUMP_IF_FALSE));
            emitByte(static_cast<uint8_t>(OpCode::OP_POP));
            compileNode(whileStmt->body.get());
            emitLoop(loopStart);

            patchJump(exitJump);
            emitByte(static_cast<uint8_t>(OpCode::OP_POP));
        }
        else if (auto funcStmt = dynamic_cast<const FunctionStatement*>(stmt)) {
            auto function = std::make_shared<AdiFunction>(funcStmt->name, funcStmt);
            function->arity = static_cast<int>(funcStmt->params.size());
            
            FunctionCompiler* enclosing = current;
            initFunction(function);
            
            beginScope();
            for (const auto& param : funcStmt->params) {
                declareVariable(param);
                markInitialized();
            }
            
            for (const auto& s : funcStmt->body->statements) {
                compileNode(s.get());
            }
            
            auto compiledFunction = endCompiler();
            current = enclosing;

            uint8_t global = parseVariable(funcStmt->name);
            emitConstant(compiledFunction);
            defineVariable(global);
        }
        else if (auto returnStmt = dynamic_cast<const ReturnStatement*>(stmt)) {
            if (returnStmt->value != nullptr) {
                compileExpression(returnStmt->value.get());
            } else {
                emitByte(static_cast<uint8_t>(OpCode::OP_NIL));
            }
            emitByte(static_cast<uint8_t>(OpCode::OP_RETURN));
        }
        else if (auto structStmt = dynamic_cast<const StructStmt*>(stmt)) {
            uint8_t nameConst = makeConstant(structStmt->name);
            auto structDef = std::make_shared<AdiStructDef>(structStmt->name, structStmt->fields);
            emitConstant(structDef);
            defineVariable(nameConst);
        }
    }

    void compileExpression(const Expr* expr) {
        if (auto num = dynamic_cast<const NumberExpr*>(expr)) {
            emitConstant(num->value);
        }
        else if (auto str = dynamic_cast<const StringExpr*>(expr)) {
            emitConstant(str->value);
        }
        else if (auto var = dynamic_cast<const VariableExpr*>(expr)) {
            if (var->name == "true") {
                emitByte(static_cast<uint8_t>(OpCode::OP_TRUE));
            } else if (var->name == "false") {
                emitByte(static_cast<uint8_t>(OpCode::OP_FALSE));
            } else {
                int arg = resolveLocal(current, var->name);
                if (arg != -1) {
                    emitBytes(static_cast<uint8_t>(OpCode::OP_GET_LOCAL), static_cast<uint8_t>(arg));
                } else {
                    emitBytes(static_cast<uint8_t>(OpCode::OP_GET_GLOBAL), makeConstant(var->name));
                }
            }
        }
        else if (auto assign = dynamic_cast<const AssignExpr*>(expr)) {
            compileExpression(assign->value.get());
            int arg = resolveLocal(current, assign->name);
            if (arg != -1) {
                emitBytes(static_cast<uint8_t>(OpCode::OP_SET_LOCAL), static_cast<uint8_t>(arg));
            } else {
                emitBytes(static_cast<uint8_t>(OpCode::OP_SET_GLOBAL), makeConstant(assign->name));
            }
        }
        else if (auto bin = dynamic_cast<const BinaryExpr*>(expr)) {
            compileExpression(bin->left.get());
            compileExpression(bin->right.get());

            switch (bin->op) {
                case TokenType::PLUS: emitByte(static_cast<uint8_t>(OpCode::OP_ADD)); break;
                case TokenType::MINUS: emitByte(static_cast<uint8_t>(OpCode::OP_SUBTRACT)); break;
                case TokenType::STAR: emitByte(static_cast<uint8_t>(OpCode::OP_MULTIPLY)); break;
                case TokenType::SLASH: emitByte(static_cast<uint8_t>(OpCode::OP_DIVIDE)); break;
                case TokenType::EQUAL_EQUAL: emitByte(static_cast<uint8_t>(OpCode::OP_EQUAL)); break;
                case TokenType::GREATER: emitByte(static_cast<uint8_t>(OpCode::OP_GREATER)); break;
                case TokenType::LESS: emitByte(static_cast<uint8_t>(OpCode::OP_LESS)); break;
                default: break;
            }
        }
        else if (auto callExpr = dynamic_cast<const CallExpr*>(expr)) {
            compileExpression(callExpr->callee.get());
            for (const auto& arg : callExpr->arguments) {
                compileExpression(arg.get());
            }
            emitBytes(static_cast<uint8_t>(OpCode::OP_CALL), static_cast<uint8_t>(callExpr->arguments.size()));
        }
        else if (auto structInst = dynamic_cast<const StructInstanceExpr*>(expr)) {
            int arg = resolveLocal(current, structInst->name);
            if (arg != -1) {
                emitBytes(static_cast<uint8_t>(OpCode::OP_GET_LOCAL), static_cast<uint8_t>(arg));
            } else {
                emitBytes(static_cast<uint8_t>(OpCode::OP_GET_GLOBAL), makeConstant(structInst->name));
            }
            for (const auto& argExpr : structInst->arguments) {
                compileExpression(argExpr.get());
            }
            emitBytes(static_cast<uint8_t>(OpCode::OP_STRUCT_INSTANCE), static_cast<uint8_t>(structInst->arguments.size()));
        }
        else if (auto getExpr = dynamic_cast<const GetExpr*>(expr)) {
            compileExpression(getExpr->object.get());
            emitBytes(static_cast<uint8_t>(OpCode::OP_GET), makeConstant(getExpr->name));
        }
        else if (auto setExpr = dynamic_cast<const SetExpr*>(expr)) {
            compileExpression(setExpr->value.get());
            compileExpression(setExpr->object.get());
            emitBytes(static_cast<uint8_t>(OpCode::OP_SET), makeConstant(setExpr->name));
        }
        else if (auto arrExpr = dynamic_cast<const ArrayExpr*>(expr)) {
            for (const auto& element : arrExpr->elements) {
                compileExpression(element.get());
            }
            emitBytes(static_cast<uint8_t>(OpCode::OP_ARRAY), static_cast<uint8_t>(arrExpr->elements.size()));
        }
        else if (auto indexGet = dynamic_cast<const IndexGetExpr*>(expr)) {
            compileExpression(indexGet->target.get());
            compileExpression(indexGet->index.get());
            emitByte(static_cast<uint8_t>(OpCode::OP_INDEX_GET));
        }
        else if (auto indexSet = dynamic_cast<const IndexSetExpr*>(expr)) {
            compileExpression(indexSet->value.get());   // Pushed 1st
            compileExpression(indexSet->target.get());  // Pushed 2nd
            compileExpression(indexSet->index.get());   // Pushed 3rd
            emitByte(static_cast<uint8_t>(OpCode::OP_INDEX_SET));
        }
    }

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
    Compiler() = default;

    bool compile(const Program* program, Chunk* chunk) {
        auto scriptFunction = std::make_shared<AdiFunction>("script");
        scriptFunction->chunk = *chunk;
        initFunction(scriptFunction);

        try {
            for (const auto& stmt : program->statements) {
                compileNode(stmt.get());
            }
            auto compiledFunction = endCompiler();
            *chunk = compiledFunction->chunk;
            return true;
        } catch (const std::runtime_error& e) {
            std::cerr << "Compiler Error: " << e.what() << "\n";
            if (current) {
                delete current;
                current = nullptr;
            }
            return false;
        }
    }
};

#endif // ADILANG_COMPILER_H