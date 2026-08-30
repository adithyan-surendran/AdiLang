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

    NumberExpr(double value)
        : value(value)
    {
    }
};

class StringExpr : public Expr
{
public:
    std::string value;

    StringExpr(const std::string& value)
        : value(value)
    {
    }
};

class VariableExpr : public Expr
{
public:
    std::string name;

    VariableExpr(const std::string& name)
        : name(name)
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
    std::unique_ptr<Stmt> elseBranch; // Can be nullptr if there is no 'else'

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
// ==========================================
// 3. Root Node: Program
// ==========================================
class Program
{
public:
    std::vector<std::unique_ptr<Stmt>> statements;
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

#endif // ADILANG_AST_H