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

// Master base class for all heap-allocated objects tracked by the GC
struct AdiObject {
    bool isMarked = false;
    AdiObject* next = nullptr;
    virtual ~AdiObject() = default;
};

using Value = std::variant<
    double, 
    bool, 
    std::string, 
    AdiArray*, 
    AdiFunction*, 
    AdiNativeMethod*, 
    AdiStructDef*, 
    AdiInstance*, 
    AdiBoundMethod*,
    AdiClosure*
>;

struct AdiUpvalue : public AdiObject {
    Value* location; 
    Value closed = false; 
    AdiUpvalue* next = nullptr; 
};

struct AdiClosure : public AdiObject {
    AdiFunction* function;
    std::vector<AdiUpvalue*> upvalues;
};

struct AdiArray : public AdiObject {
    std::vector<Value> elements;

    AdiArray() = default;
    explicit AdiArray(std::vector<Value> elems) 
        : elements(std::move(elems)) {}
};
struct AdiInstance : public AdiObject {
    AdiStructDef* blueprint;
    std::unordered_map<std::string, Value> fields;

    explicit AdiInstance(AdiStructDef* bp) : blueprint(bp) {}

    Value get(const std::string& name) {
        auto it = fields.find(name);
        if (it != fields.end()) {
            return it->second;
        }
        throw std::runtime_error("Undefined property '" + name + "'.");
    }

    void set(const std::string& name, Value value) {
        fields[name] = value;
    }
};

struct AdiBoundMethod : public AdiObject {
    AdiInstance* receiver;
    AdiFunction* method;
    
    AdiBoundMethod(AdiInstance* rec, AdiFunction* meth) 
        : receiver(rec), method(meth) {}
};
struct AdiFunction : public AdiObject {
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

struct AdiStructDef : public AdiObject {
    std::string name;
    AdiStructDef* superclass; 
    std::vector<std::string> fields;
    std::unordered_map<std::string, AdiFunction*> methods; // Use raw pointer here

    AdiStructDef() = default;
    
    AdiStructDef(std::string name, std::vector<std::string> fields, AdiStructDef* superclass = nullptr)
        : name(std::move(name)), superclass(superclass), fields(std::move(fields)) {}
    
    explicit AdiStructDef(const StructStmt* stmt);
};
using AdiStructBlueprint = AdiStructDef;

using NativeMethodFn = std::function<Value(AdiArray*, const std::vector<Value>&)>;

struct AdiNativeMethod : public AdiObject {
    std::string name;
    NativeMethodFn function;
    AdiArray* self;

    AdiNativeMethod(std::string name, NativeMethodFn function, AdiArray* self)
        : name(name), function(function), self(self) {}

    Value call(const std::vector<Value>& args) {
        return function(self, args);
    }
};

#endif // ADILANG_OBJECT_H