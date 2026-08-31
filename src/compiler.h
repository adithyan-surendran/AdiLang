#ifndef ADILANG_COMPILER_H
#define ADILANG_COMPILER_H

#include "chunk.h"
#include "parser.h"
#include <memory>
#include <stdexcept>

class Compiler {
private:
    Chunk* compilingChunk;

    void emitByte(uint8_t byte, int line) {
        compilingChunk->write(byte, line);
    }

    void emitConstant(Value value, int line) {
        int constantIndex = compilingChunk->addConstant(value);
        emitByte(static_cast<uint8_t>(OpCode::OP_CONSTANT), line);
        emitByte(static_cast<uint8_t>(constantIndex), line);
    }

    size_t emitJump(uint8_t instruction) {
        emitByte(instruction, 1);
        emitByte(0xff, 1);
        emitByte(0xff, 1);
        return compilingChunk->code.size() - 2;
    }

    void patchJump(size_t offset) {
        size_t jump = compilingChunk->code.size() - offset - 2;
        if (jump > UINT16_MAX) {
            throw std::runtime_error("Too much code to jump over.");
        }
        compilingChunk->code[offset] = (jump >> 8) & 0xff;
        compilingChunk->code[offset + 1] = jump & 0xff;
    }

    void emitLoop(size_t loopStart) {
        emitByte(static_cast<uint8_t>(OpCode::OP_LOOP), 1);
        size_t offset = compilingChunk->code.size() - loopStart + 2;
        if (offset > UINT16_MAX) {
            throw std::runtime_error("Loop body too large.");
        }
        emitByte((offset >> 8) & 0xff, 1);
        emitByte(offset & 0xff, 1);
    }

    uint8_t identifierConstant(const std::string& name, [[maybe_unused]] int line) {
        return static_cast<uint8_t>(compilingChunk->addConstant(name));
    }

    void compileExpression(Expr* expr) {
        if (!expr) return;

        if (auto numExpr = dynamic_cast<NumberExpr*>(expr)) {
            emitConstant(numExpr->value, 1);
        } else if (auto strExpr = dynamic_cast<StringExpr*>(expr)) {
            emitConstant(strExpr->value, 1);
        } else if (auto varExpr = dynamic_cast<VariableExpr*>(expr)) {
            uint8_t nameConst = identifierConstant(varExpr->name, 1);
            emitByte(static_cast<uint8_t>(OpCode::OP_GET_GLOBAL), 1);
            emitByte(nameConst, 1);
        } else if (auto binExpr = dynamic_cast<BinaryExpr*>(expr)) {
            compileExpression(binExpr->left.get());
            compileExpression(binExpr->right.get());

            switch (binExpr->op) {
                case TokenType::PLUS: emitByte(static_cast<uint8_t>(OpCode::OP_ADD), 1); break;
                case TokenType::MINUS: emitByte(static_cast<uint8_t>(OpCode::OP_SUBTRACT), 1); break;
                case TokenType::STAR: emitByte(static_cast<uint8_t>(OpCode::OP_MULTIPLY), 1); break;
                case TokenType::SLASH: emitByte(static_cast<uint8_t>(OpCode::OP_DIVIDE), 1); break;
                case TokenType::EQUAL_EQUAL: emitByte(static_cast<uint8_t>(OpCode::OP_EQUAL), 1); break;
                case TokenType::GREATER: emitByte(static_cast<uint8_t>(OpCode::OP_GREATER), 1); break;
                case TokenType::LESS: emitByte(static_cast<uint8_t>(OpCode::OP_LESS), 1); break;
                default:
                    throw std::runtime_error("Compiler Error: Unsupported binary operator.");
            }
        } else if (auto unExpr = dynamic_cast<UnaryExpr*>(expr)) {
            compileExpression(unExpr->right.get());
            if (unExpr->op == TokenType::MINUS) {
                emitByte(static_cast<uint8_t>(OpCode::OP_NEGATE), 1);
            } else {
                throw std::runtime_error("Compiler Error: Unsupported unary operator.");
            }
        }else if (auto assignExpr = dynamic_cast<AssignExpr*>(expr)) {
            compileExpression(assignExpr->value.get());
            uint8_t nameConst = identifierConstant(assignExpr->name, 1);
            emitByte(static_cast<uint8_t>(OpCode::OP_SET_GLOBAL), 1);
            emitByte(nameConst, 1);
        } else {
            throw std::runtime_error("Compiler Error: Unhandled expression type in compiler.");
        }
    }

void compileStatement(Stmt* stmt) {
        if (!stmt) return;

        if (auto varStmt = dynamic_cast<VariableDeclaration*>(stmt)) {
            if (varStmt->initializer) {
                compileExpression(varStmt->initializer.get());
            } else {
                emitByte(static_cast<uint8_t>(OpCode::OP_NIL), 1);
            }

            uint8_t nameConst = identifierConstant(varStmt->name, 1);
            emitByte(static_cast<uint8_t>(OpCode::OP_DEFINE_GLOBAL), 1);
            emitByte(nameConst, 1);
        } 
        else if (auto ifStmt = dynamic_cast<IfStatement*>(stmt)) {
            compileExpression(ifStmt->condition.get());

            size_t thenJump = emitJump(static_cast<uint8_t>(OpCode::OP_JUMP_IF_FALSE));
            emitByte(static_cast<uint8_t>(OpCode::OP_POP), 1);

            compileStatement(ifStmt->thenBranch.get());
            size_t elseJump = emitJump(static_cast<uint8_t>(OpCode::OP_JUMP));

            patchJump(thenJump);
            emitByte(static_cast<uint8_t>(OpCode::OP_POP), 1);

            if (ifStmt->elseBranch) {
                compileStatement(ifStmt->elseBranch.get());
            }
            patchJump(elseJump);
        }
        else if (auto whileStmt = dynamic_cast<WhileStatement*>(stmt)) {
            size_t loopStart = compilingChunk->code.size();
            compileExpression(whileStmt->condition.get());

            size_t exitJump = emitJump(static_cast<uint8_t>(OpCode::OP_JUMP_IF_FALSE));
            emitByte(static_cast<uint8_t>(OpCode::OP_POP), 1);

            compileStatement(whileStmt->body.get());
            emitLoop(loopStart);

            patchJump(exitJump);
            emitByte(static_cast<uint8_t>(OpCode::OP_POP), 1);
        } 
        else if (auto printStmt = dynamic_cast<PrintStatement*>(stmt)) {
            compileExpression(printStmt->expression.get());
            emitByte(static_cast<uint8_t>(OpCode::OP_PRINT), 1);
        } 
        else if (auto exprStmt = dynamic_cast<ExpressionStatement*>(stmt)) {
            compileExpression(exprStmt->expression.get());
            emitByte(static_cast<uint8_t>(OpCode::OP_POP), 1);
        } 
        else if (auto blockStmt = dynamic_cast<BlockStatement*>(stmt)) {
            for (const auto& innerStmt : blockStmt->statements) {
                compileStatement(innerStmt.get());
            }
        }
        else {
            throw std::runtime_error("Compiler Error: Unhandled statement type in compiler.");
        }
    }

public:
    bool compile(Program* program, Chunk* chunk) {
        compilingChunk = chunk;

        try {
            for (const auto& stmt : program->statements) {
                compileStatement(stmt.get());
            }
            emitByte(static_cast<uint8_t>(OpCode::OP_RETURN), 1);
            return true;
        } catch (const std::exception& e) {
            std::cerr << e.what() << "\n";
            return false;
        }
    }
};

#endif