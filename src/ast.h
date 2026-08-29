#ifndef ADILANG_AST_H
#define ADILANG_AST_H

#include "lexer.h"
#include <memory>
#include <string>

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

#endif