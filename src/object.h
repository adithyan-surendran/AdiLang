#ifndef ADILANG_OBJECT_H
#define ADILANG_OBJECT_H

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <variant>

// Include full definitions needed for member embedding
#include "chunk.h"

struct StructStmt;
struct AdiNativeMethod;
struct AdiArray;
struct AdiFunction;
struct AdiStructDef;
struct AdiInstance;
struct AdiBoundMethod;

using ObjectValue = std::variant<
    double, 
    bool, 
    std::string, 
    std::shared_ptr<AdiArray>, 
    std::shared_ptr<AdiFunction>, 
    std::shared_ptr<AdiNativeMethod>, 
    std::shared_ptr<AdiStructDef>, 
    std::shared_ptr<AdiInstance>, 
    std::shared_ptr<AdiBoundMethod>
>;

struct AdiArray {
    std::vector<ObjectValue> elements;

    AdiArray() = default;
    explicit AdiArray(std::vector<ObjectValue> elems) 
        : elements(std::move(elems)) {}
};

struct AdiFunction {
    int arity = 0;
    int upvalueCount = 0;
    Chunk chunk;
    std::string name;

    AdiFunction() = default;
    
    // Constructor matching (int arity, std::string name)
    AdiFunction(int arity, std::string name) 
        : arity(arity), name(std::move(name)) {}

    // Overloads to gracefully absorb various compiler make_shared patterns
    explicit AdiFunction(std::string name) 
        : name(std::move(name)) {}

    AdiFunction(std::string name, const struct FunctionStatement* /*stmt*/) 
        : name(std::move(name)) {}
};

struct AdiStructDef {
    std::string name;
    std::vector<std::string> fields;
    std::unordered_map<std::string, std::shared_ptr<AdiFunction>> methods;

    AdiStructDef() = default;
    AdiStructDef(std::string name, std::vector<std::string> fields)
        : name(std::move(name)), fields(std::move(fields)) {}
    
    explicit AdiStructDef(const StructStmt* stmt);
};

using AdiStructBlueprint = AdiStructDef;

#endif // ADILANG_OBJECT_H