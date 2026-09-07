#include "compiler.h"
#include "vm.h"
#include <stdexcept>
#include <iostream>

Compiler::Compiler(VM* vm) : vm(vm) {}

bool Compiler::compile(const Program* program, Chunk* chunk) {
    AdiFunction* scriptFunction = vm->allocateObject<AdiFunction>("script");
    scriptFunction->chunk = std::make_shared<Chunk>(*chunk);
    initFunction(scriptFunction);

    try {
        for (const auto& stmt : program->statements) {
            compileNode(stmt.get());
        }
        AdiFunction* compiledFunction = endCompiler();
        
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

void Compiler::compileNode(const Stmt* stmt) {
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
        AdiFunction* function = vm->allocateObject<AdiFunction>(funcStmt->name, funcStmt);
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
        function->upvalueCount = static_cast<int>(upvalues.size());
        
        AdiFunction* compiledFunction = endCompiler();
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
        
        AdiStructDef* parentDef = nullptr;
        if (structStmt->superclass.has_value()) {
            std::string superclassName = structStmt->superclass.value();
            if (definedStructs.find(superclassName) == definedStructs.end()) {
                throw std::runtime_error("Compiler Error: Undefined superclass '" + superclassName + "'.");
            }
            parentDef = definedStructs[superclassName];
        }

        std::vector<std::string> combinedFields;
        if (parentDef != nullptr) {
            combinedFields = parentDef->fields;
        }
        for (const auto& field : structStmt->fields) {
            combinedFields.push_back(field);
        }

        AdiStructDef* structDef = vm->allocateObject<AdiStructDef>(structStmt->name, combinedFields, parentDef);

        if (parentDef != nullptr) {
            structDef->methods = parentDef->methods;
        }

        ClassCompiler classCompiler;
        classCompiler.enclosing = currentClass;
        classCompiler.hasSuperclass = structStmt->superclass.has_value();
        if (structStmt->superclass.has_value()) {
            classCompiler.superclassName = structStmt->superclass.value();
        }
        currentClass = &classCompiler;

        for (const auto& methodStmt : structStmt->methods) {
            AdiFunction* function = vm->allocateObject<AdiFunction>(methodStmt->name, methodStmt.get());
            function->arity = static_cast<int>(methodStmt->params.size());
            
            FunctionCompiler* enclosing = current;
            initFunction(function);
            
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
            
            AdiFunction* compiledFunction = endCompiler();
            current = enclosing;

            structDef->methods[methodStmt->name] = compiledFunction;
        }

        currentClass = currentClass->enclosing;
        definedStructs[structStmt->name] = structDef;

        emitConstant(structDef);
        defineVariable(nameConst);
    }
   
}
void Compiler::compileExpression(const Expr* expr) {
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
    else if (auto superExpr = dynamic_cast<const SuperExpr*>(expr)) {
        if (currentClass == nullptr || !currentClass->hasSuperclass) {
            throw std::runtime_error("Compiler Error: Can't use 'super' outside of a subclass method.");
        }

        int thisSlot = resolveLocal(current, "this");
        if (thisSlot != -1) {
            emitBytes(static_cast<uint8_t>(OpCode::OP_GET_LOCAL), static_cast<uint8_t>(thisSlot));
        } else {
            throw std::runtime_error("Compiler Error: Internal error - 'this' not found.");
        }

        int superArg = resolveLocal(current, currentClass->superclassName);
        if (superArg != -1) {
            emitBytes(static_cast<uint8_t>(OpCode::OP_GET_LOCAL), static_cast<uint8_t>(superArg));
        } else {
            emitBytes(static_cast<uint8_t>(OpCode::OP_GET_GLOBAL), makeConstant(currentClass->superclassName));
        }

        for (const auto& arg : superExpr->arguments) {
            compileExpression(arg.get());
        }

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
        compileExpression(indexSet->value.get());
        compileExpression(indexSet->target.get());
        compileExpression(indexSet->index.get());
        emitByte(static_cast<uint8_t>(OpCode::OP_INDEX_SET));
    }
    
}
