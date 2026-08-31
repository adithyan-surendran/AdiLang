#ifndef ADILANG_CHUNK_H
#define ADILANG_CHUNK_H

#include <vector>
#include <variant>
#include <string>
#include <memory>

// Forward declarations for runtime objects
struct AdiArray;
struct AdiFunction;
struct AdiNativeMethod;
struct AdiStructBlueprint;
struct AdiInstance;

// Unified Value type for the VM and bytecode engine
using Value = std::variant<
    double,
    bool,
    std::string,
    std::shared_ptr<AdiArray>,
    std::shared_ptr<AdiFunction>,
    std::shared_ptr<AdiNativeMethod>,
    std::shared_ptr<AdiStructBlueprint>,
    std::shared_ptr<AdiInstance>
>;

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
    OP_RETURN
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