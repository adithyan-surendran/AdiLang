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
struct CompilerUpvalue {
    uint8_t index;
    bool isLocal;
};
class Compiler {
private:
    std::unordered_map<std::string, std::shared_ptr<AdiStructDef>> definedStructs;
    
    struct ClassCompiler {
        ClassCompiler* enclosing = nullptr;
        bool hasSuperclass = false; 
        std::string superclassName; // Stores the parent struct name
    };

    struct FunctionCompiler {
        FunctionCompiler* enclosing = nullptr;
        std::shared_ptr<AdiFunction> function;
        int arity = 0;
        std::vector<Local> locals;
        std::vector<CompilerUpvalue> upvalues;
        int scopeDepth = 0;
    };

    FunctionCompiler* current = nullptr;
    ClassCompiler* currentClass = nullptr;

    void initFunction(std::shared_ptr<AdiFunction> function) {
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
            
            std::vector<CompilerUpvalue> upvalues = current->upvalues;
            function->upvalueCount = static_cast<int>(upvalues.size()); // <--- Ensure this is set!
            
            auto compiledFunction = endCompiler();
            current = enclosing;

            uint8_t nameConst = parseVariable(funcStmt->name);
            emitBytes(static_cast<uint8_t>(OpCode::OP_CLOSURE), makeConstant(compiledFunction));

            for (const auto& upvalue : upvalues) {
                emitByte(upvalue.isLocal ? 1 : 0);
                emitByte(upvalue.index);
            }

            defineVariable(nameConst);
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
            
            // 1. Handle Superclass Lookup & Inheritance
            std::shared_ptr<AdiStructDef> parentDef = nullptr;
            if (structStmt->superclass.has_value()) {
                std::string superclassName = structStmt->superclass.value();
                if (definedStructs.find(superclassName) == definedStructs.end()) {
                    throw std::runtime_error("Compiler Error: Undefined superclass '" + superclassName + "'.");
                }
                parentDef = definedStructs[superclassName];
            }

            // 2. Combine inherited fields with child fields
            std::vector<std::string> combinedFields;
            if (parentDef != nullptr) {
                combinedFields = parentDef->fields;
            }
            for (const auto& field : structStmt->fields) {
                combinedFields.push_back(field);
            }

            // 3. Create the struct definition object, linking the superclass
            auto structDef = std::make_shared<AdiStructDef>(structStmt->name, combinedFields, parentDef);

            // 4. Inherit parent methods as a baseline
            if (parentDef != nullptr) {
                structDef->methods = parentDef->methods;
            }

            // 5. Setup ClassCompiler context for methods (enabling super support)
            ClassCompiler classCompiler;
            classCompiler.enclosing = currentClass;
            classCompiler.hasSuperclass = structStmt->superclass.has_value();
            if (structStmt->superclass.has_value()) {
                classCompiler.superclassName = structStmt->superclass.value();
            }
            currentClass = &classCompiler;

            // 6. Compile each method defined inside the child struct (overriding parent methods if names match)
            for (const auto& methodStmt : structStmt->methods) {
                auto function = std::make_shared<AdiFunction>(methodStmt->name, methodStmt.get());
                function->arity = static_cast<int>(methodStmt->params.size());
                
                FunctionCompiler* enclosing = current;
                initFunction(function);
                
                // Bind slot 0 explicitly to "this" for methods
                current->locals[0].name = "this";
                current->locals[0].depth = 0;
                
                beginScope();
                for (const auto& param : methodStmt->params) {
                    declareVariable(param);
                    markInitialized();
                }
                
                for (const auto& s : methodStmt->body->statements) {
                    compileNode(s.get());
                }
                
                auto compiledFunction = endCompiler();
                current = enclosing;

                // Store/Override compiled method in the struct definition's method map
                structDef->methods[methodStmt->name] = compiledFunction;
            }

            currentClass = currentClass->enclosing;

            // 7. Register the struct in the compiler's defined structs map
            definedStructs[structStmt->name] = structDef;

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
                } else if ((arg = resolveUpvalue(current, var->name)) != -1) {
                    emitBytes(static_cast<uint8_t>(OpCode::OP_GET_UPVALUE), static_cast<uint8_t>(arg));
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
            } else if ((arg = resolveUpvalue(current, assign->name)) != -1) {
                emitBytes(static_cast<uint8_t>(OpCode::OP_SET_UPVALUE), static_cast<uint8_t>(arg));
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
            // 1. Get the struct blueprint
            int arg = resolveLocal(current, structInst->name);
            if (arg != -1) {
                emitBytes(static_cast<uint8_t>(OpCode::OP_GET_LOCAL), static_cast<uint8_t>(arg));
            } else {
                emitBytes(static_cast<uint8_t>(OpCode::OP_GET_GLOBAL), makeConstant(structInst->name));
            }
            // 2. Compile arguments
            for (const auto& argExpr : structInst->arguments) {
                compileExpression(argExpr.get());
            }
            // 3. Emit instantiation opcode
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
        else if (auto superExpr = dynamic_cast<const SuperExpr*>(expr)) {
            if (currentClass == nullptr || !currentClass->hasSuperclass) {
                throw std::runtime_error("Compiler Error: Can't use 'super' outside of a subclass method.");
            }

            // 1. Load 'this' (slot 0) as receiver instance
            int thisSlot = resolveLocal(current, "this");
            if (thisSlot != -1) {
                emitBytes(static_cast<uint8_t>(OpCode::OP_GET_LOCAL), static_cast<uint8_t>(thisSlot));
            } else {
                throw std::runtime_error("Compiler Error: Internal error - 'this' not found.");
            }

            // 2. Load the superclass variable (local or global)
            int superArg = resolveLocal(current, currentClass->superclassName);
            if (superArg != -1) {
                emitBytes(static_cast<uint8_t>(OpCode::OP_GET_LOCAL), static_cast<uint8_t>(superArg));
            } else {
                emitBytes(static_cast<uint8_t>(OpCode::OP_GET_GLOBAL), makeConstant(currentClass->superclassName));
            }

            // 3. Compile arguments for super.init(...)
            for (const auto& arg : superExpr->arguments) {
                compileExpression(arg.get());
            }

            // 4. Emit OP_SUPER instruction (Method name constant + argument count)
            emitBytes(static_cast<uint8_t>(OpCode::OP_SUPER), makeConstant(superExpr->method));
            emitByte(static_cast<uint8_t>(superExpr->arguments.size()));
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
        scriptFunction->chunk = std::make_shared<Chunk>(*chunk);
        initFunction(scriptFunction);

        try {
            for (const auto& stmt : program->statements) {
                compileNode(stmt.get());
            }
            // End the script compiler normally, which appends an OP_RETURN
            auto compiledFunction = endCompiler();
            
            // Copy back the fully compiled chunk containing all bytecodes, 
            // including OP_CLOSURE and OP_DEFINE_GLOBAL instructions for top-level functions.
            *chunk = *compiledFunction->chunk;
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

        // Try to resolve as a local in the immediately enclosing function
        int local = resolveLocal(compiler->enclosing, name);
        if (local != -1) {
            return addUpvalue(compiler, static_cast<uint8_t>(local), true);
        }

        // Recursively try to resolve as an upvalue in the enclosing function
        int upvalue = resolveUpvalue(compiler->enclosing, name);
        if (upvalue != -1) {
            return addUpvalue(compiler, static_cast<uint8_t>(upvalue), false);
        }

        return -1;
    }
};

#endif // ADILANG_COMPILER_H