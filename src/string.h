#ifndef ADILANG_STRING_H
#define ADILANG_STRING_H

#include "object.h"
#include "vm.h"
#include <string>
#include <algorithm>
#include <vector>
#include <stdexcept>
#include <variant>

inline Value stringLength(AdiArray* self, const std::vector<Value>& args) {
    if (args.empty() || !std::holds_alternative<std::string>(args[0])) {
        throw std::runtime_error("Runtime Error: string.length() expects 1 string argument.");
    }
    const std::string& str = std::get<std::string>(args[0]);
    return static_cast<double>(str.length());
}

inline Value stringSubstr(AdiArray* self, const std::vector<Value>& args) {
    if (args.size() < 3 || !std::holds_alternative<std::string>(args[0]) ||
        !std::holds_alternative<double>(args[1]) || !std::holds_alternative<double>(args[2])) {
        throw std::runtime_error("Runtime Error: string.substr() expects (string, start, length).");
    }
    const std::string& str = std::get<std::string>(args[0]);
    int start = static_cast<int>(std::get<double>(args[1]));
    int length = static_cast<int>(std::get<double>(args[2]));

    if (start < 0 || start >= static_cast<int>(str.length())) {
        return std::string("");
    }
    return str.substr(start, length);
}

inline Value stringContains(AdiArray* self, const std::vector<Value>& args) {
    if (args.size() < 2 || !std::holds_alternative<std::string>(args[0]) || !std::holds_alternative<std::string>(args[1])) {
        throw std::runtime_error("Runtime Error: string.contains() expects 2 string arguments.");
    }
    const std::string& str = std::get<std::string>(args[0]);
    const std::string& sub = std::get<std::string>(args[1]);
    return str.find(sub) != std::string::npos;
}

inline Value stringToUpper(AdiArray* self, const std::vector<Value>& args) {
    if (args.empty() || !std::holds_alternative<std::string>(args[0])) {
        throw std::runtime_error("Runtime Error: string.toUpper() expects 1 string argument.");
    }
    std::string str = std::get<std::string>(args[0]);
    std::transform(str.begin(), str.end(), str.begin(), ::toupper);
    return str;
}

inline AdiInstance* createStringModule(VM* vm) {
    AdiStructDef* blueprint = vm->allocateObject<AdiStructDef>("StringModule", std::vector<std::string>{});
    AdiInstance* module = vm->allocateObject<AdiInstance>(blueprint);

    module->fields["length"] = vm->allocateObject<AdiNativeMethod>("length", stringLength, nullptr);
    module->fields["substr"] = vm->allocateObject<AdiNativeMethod>("substr", stringSubstr, nullptr);
    module->fields["contains"] = vm->allocateObject<AdiNativeMethod>("contains", stringContains, nullptr);
    module->fields["toUpper"] = vm->allocateObject<AdiNativeMethod>("toUpper", stringToUpper, nullptr);

    return module;
}

#endif // ADILANG_STRING_H