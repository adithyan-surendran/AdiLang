#ifndef ADILANG_SYMBOL_TABLE_H
#define ADILANG_SYMBOL_TABLE_H

#include <string>
#include <vector>
#include <unordered_set>

class SymbolTable {
private:
    // Stack of scopes: index 0 is Global Scope, back() is Innermost Scope
    std::vector<std::unordered_set<std::string>> scopes;

public:
    SymbolTable() {
        // Always start with the global scope active
        enterScope();
    }

    // Called when entering a block: '{'
    void enterScope() {
        scopes.emplace_back();
    }

    // Called when exiting a block: '}'
    void exitScope() {
        if (!scopes.empty()) {
            scopes.pop_back();
        }
    }

    // Declare a variable in the CURRENT (innermost) scope
    bool declare(const std::string& name) {
        if (scopes.empty()) return false;

        // Check if variable already exists in the current scope
        if (scopes.back().find(name) != scopes.back().end()) {
            return false; // Duplicate declaration in same scope
        }

        scopes.back().insert(name);
        return true;
    }

    // Look up a variable: searches from innermost -> outermost scope
    bool isDeclared(const std::string& name) const {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            if (it->find(name) != it->end()) {
                return true; // Found in this scope level
            }
        }
        return false; // Not declared in any active scope
    }
};

#endif // ADILANG_SYMBOL_TABLE_H