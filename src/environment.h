#ifndef ADILANG_ENVIRONMENT_H
#define ADILANG_ENVIRONMENT_H

#include <string>
#include <unordered_map>
#include <variant>
#include <memory>
#include <stdexcept>
#include <iostream>

// Forward declaration
struct AdiFunction;
struct AdiArray;
struct AdiStructBlueprint;
struct AdiInstance;
struct AdiNativeMethod;

// Represents any dynamic runtime value in AdiLang (including user functions)
using Value = std::variant<
    double, 
    std::string, 
    bool, 
    std::shared_ptr<AdiFunction>,
    std::shared_ptr<AdiArray>,
    std::shared_ptr<AdiStructBlueprint>,
    std::shared_ptr<AdiInstance>,
    std::shared_ptr<AdiNativeMethod>
    >;

// Runtime representation of an AdiLang array
struct AdiArray {
    std::vector<Value> elements;
    explicit AdiArray(std::vector<Value> elements) : elements(std::move(elements)) {}
};

// Helper to print a Value to an output stream
inline void printValue(const Value& val) {
    std::visit([](const auto& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, bool>) {
            std::cout << (v ? "true" : "false");
        } else if constexpr (std::is_same_v<T, std::shared_ptr<AdiFunction>>) {
            std::cout << "<fn>";
        } else if constexpr (std::is_same_v<T, std::shared_ptr<AdiNativeMethod>>) {
            std::cout << "<native fn>"; // Add
        } else if constexpr (std::is_same_v<T, std::shared_ptr<AdiStructBlueprint>>) {
            std::cout << "<struct blueprint>"; // Add
        } else if constexpr (std::is_same_v<T, std::shared_ptr<AdiInstance>>) {
            std::cout << "<object instance>"; // Add
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