#ifndef ADILANG_ENVIRONMENT_H
#define ADILANG_ENVIRONMENT_H

#include <string>
#include <unordered_map>
#include <vector>
#include <variant>
#include <memory>
#include <stdexcept>
#include <iostream>
#include <functional>

// Include Chunk, Value definitions, and Object structures first
#include "chunk.h"
#include "object.h"

// Forward declarations of AST nodes
struct FunctionStatement;
class Interpreter;
class Environment;

struct AdiArray {
    std::vector<Value> elements;
    explicit AdiArray(std::vector<Value> elements) : elements(std::move(elements)) {}
};

struct AdiFunction {
    const FunctionStatement* declaration = nullptr;
    std::shared_ptr<Environment> closure;
    Chunk chunk;
    int arity = 0;
    std::string name;

    AdiFunction(const FunctionStatement* declaration, std::shared_ptr<Environment> closure)
        : declaration(declaration), closure(closure) {}

    AdiFunction(std::string name = "", const FunctionStatement* decl = nullptr)
        : declaration(decl), name(name) {}

    Value call(Interpreter& interpreter, const std::vector<Value>& arguments);
};

using NativeMethodFn = std::function<Value(std::shared_ptr<AdiArray>, const std::vector<Value>&)>;

struct AdiNativeMethod {
    std::string name;
    NativeMethodFn function;
    std::shared_ptr<AdiArray> self;

    AdiNativeMethod(std::string name, NativeMethodFn function, std::shared_ptr<AdiArray> self)
        : name(name), function(function), self(self) {}

    Value call(const std::vector<Value>& args) {
        return function(self, args);
    }
};

// Helper to print a Value
inline void printValue(const Value& val) {
    std::visit([](const auto& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, bool>) {
            std::cout << (v ? "true" : "false");
        } else if constexpr (std::is_same_v<T, std::shared_ptr<AdiFunction>>) {
            std::cout << "<fn>";
        } else if constexpr (std::is_same_v<T, std::shared_ptr<AdiNativeMethod>>) {
            std::cout << "<native fn>";
        } else if constexpr (std::is_same_v<T, std::shared_ptr<AdiStructBlueprint>>) {
            std::cout << "<struct blueprint>";
        } else if constexpr (std::is_same_v<T, std::shared_ptr<AdiInstance>>) {
            std::cout << "<object instance>";
        } else if constexpr (std::is_same_v<T, std::shared_ptr<AdiArray>>) {
            std::cout << "[";
            for (size_t i = 0; i < v->elements.size(); ++i) {
                printValue(v->elements[i]);
                if (i + 1 < v->elements.size()) std::cout << ", ";
            }
            std::cout << "]";
        } else {
            std::cout << v;
        }
    }, val);
}

class Environment {
private:
    std::unordered_map<std::string, Value> values;
    std::shared_ptr<Environment> enclosing;

public:
    Environment() : enclosing(nullptr) {}

    explicit Environment(std::shared_ptr<Environment> enclosing)
        : enclosing(std::move(enclosing)) {}

    void define(const std::string& name, const Value& value) {
        values[name] = value;
    }

    Value get(const std::string& name) const {
        auto it = values.find(name);
        if (it != values.end()) {
            return it->second;
        }

        if (enclosing != nullptr) {
            return enclosing->get(name);
        }

        throw std::runtime_error("Undefined variable '" + name + "'.");
    }

    void assign(const std::string& name, const Value& value) {
        auto it = values.find(name);
        if (it != values.end()) {
            it->second = value;
            return;
        }

        if (enclosing != nullptr) {
            enclosing->assign(name, value);
            return;
        }

        throw std::runtime_error("Undefined variable '" + name + "'.");
    }
};

#endif // ADILANG_ENVIRONMENT_H