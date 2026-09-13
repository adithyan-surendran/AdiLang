#ifndef ADILANG_JSON_H
#define ADILANG_JSON_H

#include "core/object.h"
#include "core/vm.h"
#include <string>
#include <vector>
#include <stdexcept>
#include <variant>

// Lightweight serializer supporting numbers, booleans, strings, and arrays
inline std::string valueToJsonString(const Value& val) {
    if (std::holds_alternative<double>(val)) {
        double d = std::get<double>(val);
        if (d == static_cast<int>(d)) {
            return std::to_string(static_cast<int>(d));
        }
        return std::to_string(d);
    } else if (std::holds_alternative<bool>(val)) {
        return std::get<bool>(val) ? "true" : "false";
    } else if (std::holds_alternative<std::string>(val)) {
        return "\"" + std::get<std::string>(val) + "\"";
    } else if (std::holds_alternative<AdiArray*>(val)) {
        AdiArray* arr = std::get<AdiArray*>(val);
        std::string result = "[";
        for (size_t i = 0; i < arr->elements.size(); ++i) {
            result += valueToJsonString(arr->elements[i]);
            if (i + 1 < arr->elements.size()) {
                result += ",";
            }
        }
        result += "]";
        return result;
    }
    return "null";
}

inline Value jsonStringify(AdiArray* self, const std::vector<Value>& args) {
    if (args.empty()) {
        throw std::runtime_error("Runtime Error: json.stringify() expects 1 argument.");
    }
    return valueToJsonString(args[0]);
}

inline Value jsonParse(AdiArray* self, const std::vector<Value>& args) {
    if (args.empty() || !std::holds_alternative<std::string>(args[0])) {
        throw std::runtime_error("Runtime Error: json.parse() expects 1 string argument.");
    }
    // Basic placeholder parser returning the string for raw inspection until full map objects arrive in v0.17
    return args[0];
}

inline AdiInstance* createJsonModule(VM* vm) {
    AdiStructDef* blueprint = vm->allocateObject<AdiStructDef>("JsonModule", std::vector<std::string>{});
    AdiInstance* module = vm->allocateObject<AdiInstance>(blueprint);

    module->fields["stringify"] = vm->allocateObject<AdiNativeMethod>("stringify", jsonStringify, nullptr);
    module->fields["parse"] = vm->allocateObject<AdiNativeMethod>("parse", jsonParse, nullptr);

    return module;
}

#endif // ADILANG_JSON_H