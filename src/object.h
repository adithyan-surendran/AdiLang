#ifndef ADILANG_OBJECT_H
#define ADILANG_OBJECT_H

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <variant>
#include <functional>

// Forward declarations
struct StructStmt;
struct Chunk;
struct AdiNativeMethod;
struct AdiArray;
struct AdiFunction;
struct AdiStructDef;
struct AdiInstance;
struct AdiBoundMethod;
struct Environment;
struct AdiUpvalue;
struct AdiClosure; 

using Value = std::variant<
    double, 
    bool, 
    std::string, 
    std::shared_ptr<AdiArray>, 
    std::shared_ptr<AdiFunction>, 
    std::shared_ptr<AdiNativeMethod>, 
    std::shared_ptr<AdiStructDef>, 
    std::shared_ptr<AdiInstance>, 
    std::shared_ptr<AdiBoundMethod>,
    std::shared_ptr<AdiClosure> // Valid because AdiClosure is defined above
>;
// 1. Define AdiUpvalue and AdiClosure FIRST so they are known types
struct AdiUpvalue {
    Value* location; 
    Value closed = false; 
    AdiUpvalue* next = nullptr; 
};

struct AdiClosure {
    std::shared_ptr<AdiFunction> function;
    std::vector<std::shared_ptr<AdiUpvalue>> upvalues;
};


struct AdiArray {
    std::vector<Value> elements;

    AdiArray() = default;
    explicit AdiArray(std::vector<Value> elems) 
        : elements(std::move(elems)) {}
};

struct AdiFunction {
    int arity = 0;
    int upvalueCount = 0;
    std::shared_ptr<Chunk> chunk;
    std::string name;
    struct FunctionStatement* declaration = nullptr;
    std::shared_ptr<Environment> closure;

    AdiFunction() : chunk(std::make_shared<Chunk>()) {}
    
    explicit AdiFunction(std::string name) 
        : chunk(std::make_shared<Chunk>()), name(std::move(name)) {}

    AdiFunction(int arity, std::string name) 
        : arity(arity), chunk(std::make_shared<Chunk>()), name(std::move(name)) {}
        
    AdiFunction(std::string name, const struct FunctionStatement* stmt) 
        : chunk(std::make_shared<Chunk>()), name(std::move(name)), declaration(const_cast<FunctionStatement*>(stmt)) {}
    
    AdiFunction(const struct FunctionStatement* stmt, std::shared_ptr<Environment> closureEnv)
        : chunk(std::make_shared<Chunk>()), declaration(const_cast<FunctionStatement*>(stmt)), closure(closureEnv) {}

    Value call(class Interpreter& interpreter, const std::vector<Value>& arguments);
};

struct AdiStructDef {
    std::string name;
    std::shared_ptr<AdiStructDef> superclass; 
    std::vector<std::string> fields;
    std::unordered_map<std::string, std::shared_ptr<AdiFunction>> methods;

    AdiStructDef() = default;
    
    AdiStructDef(std::string name, std::vector<std::string> fields, std::shared_ptr<AdiStructDef> superclass = nullptr)
        : name(std::move(name)), superclass(std::move(superclass)), fields(std::move(fields)) {}
    
    explicit AdiStructDef(const StructStmt* stmt);
};
using AdiStructBlueprint = AdiStructDef;

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

#endif // ADILANG_OBJECT_H