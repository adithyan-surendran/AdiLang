#ifndef ADILANG_CHUNK_H
#define ADILANG_CHUNK_H

#include <vector>
#include <cstdint>
#include "object.h"

enum class OpCode : uint8_t {
    OP_CONSTANT,
    OP_NIL,
    OP_TRUE,
    OP_FALSE,
    OP_POP,
    OP_DEFINE_GLOBAL,
    OP_GET_GLOBAL,
    OP_SET_GLOBAL,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_NEGATE,
    OP_NOT,
    OP_PRINT,
    OP_EQUAL,
    OP_GREATER,
    OP_LESS,
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_LOOP,
    OP_GET_LOCAL,
    OP_SET_LOCAL,
    OP_CALL,
    OP_RETURN,
    OP_STRUCT_INSTANCE,
    OP_GET,
    OP_SET,
    OP_STRUCT,
    OP_SUPER,
    OP_ARRAY,
    OP_INDEX_GET,
    OP_INDEX_SET,
    OP_CLOSURE,
    OP_GET_UPVALUE,
    OP_SET_UPVALUE,
    OP_CLOSE_UPVALUE,
    OP_MAP
};

struct Chunk {
    std::vector<uint8_t> code;
    std::vector<int> lines;
    std::vector<Value> constants;

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