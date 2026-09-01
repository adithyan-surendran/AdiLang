#ifndef ADILANG_CHUNK_H
#define ADILANG_CHUNK_H

#include <vector>
#include <variant>
#include <string>
#include <memory>
#include <unordered_map>
#include <stdexcept>
#include "object.h"

// Forward declarations
struct AdiArray;
struct AdiFunction;
struct AdiNativeMethod;
struct AdiStructDef;
struct AdiInstance;
struct AdiBoundMethod;

using Value = std::variant<
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

// Define AdiInstance here where Value is 100% a complete type
struct AdiInstance : public std::enable_shared_from_this<AdiInstance> {
    std::shared_ptr<AdiStructDef> blueprint;
    std::unordered_map<std::string, Value> fields;

    explicit AdiInstance(std::shared_ptr<AdiStructDef> bp) : blueprint(bp) {}

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

struct AdiBoundMethod {
    std::shared_ptr<AdiInstance> receiver;
    std::shared_ptr<AdiFunction> method;
    
    AdiBoundMethod(std::shared_ptr<AdiInstance> rec, std::shared_ptr<AdiFunction> meth) 
        : receiver(rec), method(meth) {}
};

enum class OpCode : uint8_t {
    OP_CONSTANT,
    OP_NIL,
    OP_TRUE,
    OP_FALSE,
    OP_POP,
    OP_GET_LOCAL,
    OP_SET_LOCAL,
    OP_GET_GLOBAL,
    OP_DEFINE_GLOBAL,
    OP_SET_GLOBAL,
    OP_EQUAL,
    OP_GREATER,
    OP_LESS,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_NOT,
    OP_NEGATE,
    OP_PRINT,
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_LOOP,
    OP_CALL,
    OP_CLOSURE,
    OP_RETURN,
    OP_STRUCT,
    OP_STRUCT_INSTANCE,
    OP_GET,
    OP_SET,
    OP_ARRAY,
    OP_INDEX_GET,
    OP_INDEX_SET
};

struct Chunk {
    std::vector<uint8_t> code;
    std::vector<Value> constants;
    std::vector<int> lines;

    void write(uint8_t byte, int line) {
        code.push_back(byte);
        lines.push_back(line);
    }

    int addConstant(Value value) {
        constants.push_back(value);
        return static_cast<int>(constants.size() - 1);
    }
};

#endif // ADILANG_CHUNK_H