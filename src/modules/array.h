#ifndef ADILANG_ARRAY_H
#define ADILANG_ARRAY_H

#include "core/object.h"
#include "core/vm.h"

#include <vector>
#include <stdexcept>
#include <algorithm>

// ============================================================
// Array Length
// ============================================================

inline Value arrayLen(
    AdiArray* self,
    const std::vector<Value>& args
) {
    if (
        args.size() != 1 ||
        !std::holds_alternative<AdiArray*>(args[0])
    ) {
        throw std::runtime_error(
            "array.len() expects an array argument."
        );
    }

    AdiArray* arr =
        std::get<AdiArray*>(args[0]);

    return static_cast<double>(
        arr->elements.size()
    );
}

// ============================================================
// Array Push
// ============================================================

inline Value arrayPush(
    AdiArray* self,
    const std::vector<Value>& args
) {
    if (
        args.size() != 2 ||
        !std::holds_alternative<AdiArray*>(args[0])
    ) {
        throw std::runtime_error(
            "array.push() expects an array and a value."
        );
    }

    AdiArray* arr =
        std::get<AdiArray*>(args[0]);

    arr->elements.push_back(args[1]);

    return true;
}

// ============================================================
// Array Pop
// ============================================================

inline Value arrayPop(
    AdiArray* self,
    const std::vector<Value>& args
) {
    if (
        args.size() != 1 ||
        !std::holds_alternative<AdiArray*>(args[0])
    ) {
        throw std::runtime_error(
            "array.pop() expects an array argument."
        );
    }

    AdiArray* arr =
        std::get<AdiArray*>(args[0]);

    if (arr->elements.empty()) {
        throw std::runtime_error(
            "array.pop() cannot be used on an empty array."
        );
    }

    Value value =
        arr->elements.back();

    arr->elements.pop_back();

    return value;
}

// ============================================================
// Array Contains
// ============================================================

inline Value arrayContains(
    AdiArray* self,
    const std::vector<Value>& args
) {
    if (
        args.size() != 2 ||
        !std::holds_alternative<AdiArray*>(args[0])
    ) {
        throw std::runtime_error(
            "array.contains() expects two arguments (array, value)."
        );
    }

    AdiArray* arr =
        std::get<AdiArray*>(args[0]);

    const Value& target =
        args[1];

    for (const auto& val : arr->elements) {
        if (val == target) {
            return true;
        }
    }

    return false;
}

// ============================================================
// Array Insert
// ============================================================

inline Value arrayInsert(
    AdiArray* self,
    const std::vector<Value>& args
) {
    if (
        args.size() != 3 ||
        !std::holds_alternative<AdiArray*>(args[0]) ||
        !std::holds_alternative<double>(args[1])
    ) {
        throw std::runtime_error(
            "array.insert() expects an array, index, and value."
        );
    }

    AdiArray* arr =
        std::get<AdiArray*>(args[0]);

    double indexValue =
        std::get<double>(args[1]);

    if (indexValue < 0 ||
        indexValue > arr->elements.size()) {
        throw std::runtime_error(
            "array.insert() index out of bounds."
        );
    }

    size_t index =
        static_cast<size_t>(indexValue);

    arr->elements.insert(
        arr->elements.begin() + index,
        args[2]
    );

    return true;
}

// ============================================================
// Array Remove
// ============================================================

inline Value arrayRemove(
    AdiArray* self,
    const std::vector<Value>& args
) {
    if (
        args.size() != 2 ||
        !std::holds_alternative<AdiArray*>(args[0]) ||
        !std::holds_alternative<double>(args[1])
    ) {
        throw std::runtime_error(
            "array.remove() expects an array and index."
        );
    }

    AdiArray* arr =
        std::get<AdiArray*>(args[0]);

    double indexValue =
        std::get<double>(args[1]);

    if (
        indexValue < 0 ||
        indexValue >= arr->elements.size()
    ) {
        throw std::runtime_error(
            "array.remove() index out of bounds."
        );
    }

    size_t index =
        static_cast<size_t>(indexValue);

    Value removed =
        arr->elements[index];

    arr->elements.erase(
        arr->elements.begin() + index
    );

    return removed;
}

// ============================================================
// Array Clear
// ============================================================

inline Value arrayClear(
    AdiArray* self,
    const std::vector<Value>& args
) {
    if (
        args.size() != 1 ||
        !std::holds_alternative<AdiArray*>(args[0])
    ) {
        throw std::runtime_error(
            "array.clear() expects an array argument."
        );
    }

    AdiArray* arr =
        std::get<AdiArray*>(args[0]);

    arr->elements.clear();

    return true;
}

// ============================================================
// Create Array Module
// ============================================================

inline AdiInstance* createArrayModule(VM* vm) {

    AdiStructDef* blueprint =
        vm->allocateObject<AdiStructDef>(
            "ArrayModule",
            std::vector<std::string>{}
        );

    AdiInstance* moduleInstance =
        vm->allocateObject<AdiInstance>(
            blueprint
        );

    // Length
    moduleInstance->fields["len"] =
        vm->allocateObject<AdiNativeMethod>(
            "len",
            arrayLen,
            nullptr
        );

    // Push
    moduleInstance->fields["push"] =
        vm->allocateObject<AdiNativeMethod>(
            "push",
            arrayPush,
            nullptr
        );

    // Pop
    moduleInstance->fields["pop"] =
        vm->allocateObject<AdiNativeMethod>(
            "pop",
            arrayPop,
            nullptr
        );

    // Contains
    moduleInstance->fields["contains"] =
        vm->allocateObject<AdiNativeMethod>(
            "contains",
            arrayContains,
            nullptr
        );

    // Insert
    moduleInstance->fields["insert"] =
        vm->allocateObject<AdiNativeMethod>(
            "insert",
            arrayInsert,
            nullptr
        );

    // Remove
    moduleInstance->fields["remove"] =
        vm->allocateObject<AdiNativeMethod>(
            "remove",
            arrayRemove,
            nullptr
        );

    // Clear
    moduleInstance->fields["clear"] =
        vm->allocateObject<AdiNativeMethod>(
            "clear",
            arrayClear,
            nullptr
        );

    return moduleInstance;
}

#endif // ADILANG_ARRAY_H