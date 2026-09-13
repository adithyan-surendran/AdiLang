#ifndef ADILANG_VM_H
#define ADILANG_VM_H

#include "chunk.h"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

struct CallFrame {
    AdiClosure* closure = nullptr;
    size_t ip = 0;
    size_t slots = 0;
};

enum class InterpretResult {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
};

class VM {
private:
    static constexpr int FRAMES_MAX = 64;
    static constexpr int STACK_MAX = FRAMES_MAX * 256;

    // Master GC linked-list anchor.
    AdiObject* objects = nullptr;

    // Approximate allocated bytes tracked by the VM.
    size_t bytesAllocated = 0;

    // Initial GC threshold: 1 MB.
    size_t nextGC = 1024 * 1024;

    CallFrame frames[FRAMES_MAX];
    int frameCount = 0;

    Chunk* chunk = nullptr;
    size_t ip = 0;

    std::vector<Value> stack;

    std::unordered_map<std::string, Value> globals;

    AdiUpvalue* openUpvalues = nullptr;

    // Runtime helpers.
    void runtimeError(const std::string& message);

    AdiUpvalue* captureUpvalue(Value* local);

    void closeUpvalues(Value* last);

    void resetStack();

    void push(Value value);

    Value pop();

    Value peek(int distance);

    void printValue(const Value& value);

    // Garbage collector.
    void markValue(Value value);

    void markObject(AdiObject* object);

    void markRoots();

    void sweep();

    // Bytecode safety helpers.
    bool checkInstructionBytes(
        size_t count,
        const CallFrame* frame
    ) const;

    bool checkConstantIndex(
        size_t index,
        const CallFrame* frame
    ) const;

    bool checkLocalSlot(
        size_t slot,
        const CallFrame* frame
    ) const;

    bool checkUpvalueSlot(
        size_t slot,
        const CallFrame* frame
    ) const;

    bool checkJumpBytes(
        size_t count,
        const CallFrame* frame
    ) const;

public:
    VM() = default;

    template <typename T, typename... Args>
    T* allocateObject(Args&&... args) {
        if (bytesAllocated > nextGC) {
            collectGarbage();

            // Avoid a zero threshold if the heap became empty.
            nextGC =
                bytesAllocated > 0
                    ? bytesAllocated * 2
                    : 1024 * 1024;
        }

        T* object =
            new T(std::forward<Args>(args)...);

        bytesAllocated += sizeof(T);

        AdiObject* gcObject =
            static_cast<AdiObject*>(object);

        gcObject->next = objects;
        objects = gcObject;

        return object;
    }

    void collectGarbage();

    InterpretResult interpret(
        Chunk* targetChunk
    );
};

#endif // ADILANG_VM_H