#ifndef ADILANG_ENVIRONMENT_H
#define ADILANG_ENVIRONMENT_H

#include <string>
#include <unordered_map>
#include <variant>
#include <memory>
#include <stdexcept>
#include <iostream>

// Represents any dynamic runtime value in AdiLang
using Value = std::variant<double, std::string, bool>;

// Helper to print a Value to an output stream
inline void printValue(const Value& val) {
    std::visit([](const auto& v) {
        if constexpr (std::is_same_v<std::decay_t<decltype(v)>, bool>) {
            std::cout << (v ? "true" : "false");
        } else {
            std::cout << v;
        }
    }, val);
}

class Environment {
private:
    std::unordered_map<std::string, Value> values;
    std::shared_ptr<Environment> enclosing; // Outer scope pointer

public:
    // Global scope constructor
    Environment() : enclosing(nullptr) {}

    // Inner scope constructor linked to parent
    explicit Environment(std::shared_ptr<Environment> enclosing)
        : enclosing(std::move(enclosing)) {}

    // Define or overwrite variable in the CURRENT scope
    void define(const std::string& name, const Value& value) {
        values[name] = value;
    }

    // Look up a variable: checks current scope, then traverses parent scopes
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

    // Assign to an EXISTING variable (supports variable mutation if needed)
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