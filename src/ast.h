#ifndef ADILANG_AST_H
#define ADILANG_AST_H

#include "lexer.h"
#include <memory>
#include <string>
#include <vector>

// ==========================================
// 1. Base Expression Node & Derived Classes
// ==========================================
class Expr
{
public:
    virtual ~Expr() = default;
};

class NumberExpr : public Expr
{
public:
    double value;

    explicit NumberExpr(double value)
        : value(value)
    {
    }
};

class StringExpr : public Expr
{
public:
    std::string value;

    explicit StringExpr(const std::string& value)
        : value(value)
    {
    }
};

class VariableExpr : public Expr
{
public:
    std::string name;

    explicit VariableExpr(const std::string& name)
        : name(name)
    {
    }
};

class AssignExpr : public Expr
{
public:
    std::string name;
    std::unique_ptr<Expr> value;

    AssignExpr(const std::string& name, std::unique_ptr<Expr> value)
        : name(name), value(std::move(value))
    {
    }
};

class UnaryExpr : public Expr
{
public:
    TokenType op;
    std::unique_ptr<Expr> right;

    UnaryExpr(TokenType op, std::unique_ptr<Expr> right)
        : op(op), right(std::move(right))
    {
    }
};

class LogicalExpr : public Expr
{
public:
    std::unique_ptr<Expr> left;
    TokenType op;
    std::unique_ptr<Expr> right;

    LogicalExpr(std::unique_ptr<Expr> left, TokenType op, std::unique_ptr<Expr> right)
        : left(std::move(left)), op(op), right(std::move(right))
    {
    }
};

class BinaryExpr : public Expr
{
public:
    std::unique_ptr<Expr> left;
    TokenType op;
    std::unique_ptr<Expr> right;

    BinaryExpr(
        std::unique_ptr<Expr> left,
        TokenType op,
        std::unique_ptr<Expr> right
    )
        : left(std::move(left)),
          op(op),
          right(std::move(right))
    {
    }
};

// ==========================================
// 2. Base Statement Node & Derived Classes
// ==========================================
class Stmt
{
public:
    virtual ~Stmt() = default;
};

class IfStatement : public Stmt
{
public:
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Stmt> thenBranch;
    std::unique_ptr<Stmt> elseBranch;

    IfStatement(
        std::unique_ptr<Expr> condition,
        std::unique_ptr<Stmt> thenBranch,
        std::unique_ptr<Stmt> elseBranch = nullptr
    )
        : condition(std::move(condition)),
          thenBranch(std::move(thenBranch)),
          elseBranch(std::move(elseBranch))
    {
    }
};

class WhileStatement : public Stmt
{
public:
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Stmt> body;

    WhileStatement(std::unique_ptr<Expr> condition, std::unique_ptr<Stmt> body)
        : condition(std::move(condition)), body(std::move(body))
    {
    }
};

class BreakStatement : public Stmt
{
public:
    BreakStatement() = default;
};

class ContinueStatement : public Stmt
{
public:
    ContinueStatement() = default;
};

class VariableDeclaration : public Stmt
{
public:
    std::string name;
    std::unique_ptr<Expr> initializer;

    VariableDeclaration(
        const std::string& name,
        std::unique_ptr<Expr> initializer
    )
        : name(name),
          initializer(std::move(initializer))
    {
    }
};

class PrintStatement : public Stmt
{
public:
    std::unique_ptr<Expr> expression;

    explicit PrintStatement(std::unique_ptr<Expr> expression)
        : expression(std::move(expression))
    {
    }
};

class ExpressionStatement : public Stmt
{
public:
    std::unique_ptr<Expr> expression;

    explicit ExpressionStatement(std::unique_ptr<Expr> expression)
        : expression(std::move(expression))
    {
    }
};

class BlockStatement : public Stmt
{
public:
    std::vector<std::unique_ptr<Stmt>> statements;

    explicit BlockStatement(std::vector<std::unique_ptr<Stmt>> stmts)
        : statements(std::move(stmts))
    {
    }
};

// ==========================================
// 3. Root Node: Program
// ==========================================
class Program
{
public:
    std::vector<std::unique_ptr<Stmt>> statements;
};

// --- Function Statement: fn name(param1, param2) { body } ---
class FunctionStatement : public Stmt {
public:
    std::string name;
    std::vector<std::string> params;
    std::unique_ptr<BlockStatement> body;

    FunctionStatement(std::string name, std::vector<std::string> params, std::unique_ptr<BlockStatement> body)
        : name(std::move(name)), params(std::move(params)), body(std::move(body)) {}
};

// --- Return Statement: return <expr>; ---
class ReturnStatement : public Stmt {
public:
    std::unique_ptr<Expr> value;

    explicit ReturnStatement(std::unique_ptr<Expr> value)
        : value(std::move(value)) {}
};

// --- Call Expression: callee(arg1, arg2) ---
class CallExpr : public Expr {
public:
    std::unique_ptr<Expr> callee;
    std::vector<std::unique_ptr<Expr>> arguments;

    CallExpr(std::unique_ptr<Expr> callee, std::vector<std::unique_ptr<Expr>> arguments)
        : callee(std::move(callee)), arguments(std::move(arguments)) {}
};
#endif // ADILANG_AST_H