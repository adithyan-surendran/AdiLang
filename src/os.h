#ifndef ADILANG_OS_H
#define ADILANG_OS_H

#include "object.h"
#include "vm.h"
#include <cstdlib>
#include <string>
#include <vector>
#include <filesystem>
#include <stdexcept>
#include <variant>

inline Value osGetenv(AdiArray* self, const std::vector<Value>& args) {
    if (args.empty() || !std::holds_alternative<std::string>(args[0])) {
        throw std::runtime_error("Runtime Error: os.getenv() expects 1 string argument.");
    }
    const std::string& varName = std::get<std::string>(args[0]);
    const char* val = std::getenv(varName.c_str());
    if (val == nullptr) {
        return std::string("");
    }
    return std::string(val);
}

inline Value osSystem(AdiArray* self, const std::vector<Value>& args) {
    if (args.empty() || !std::holds_alternative<std::string>(args[0])) {
        throw std::runtime_error("Runtime Error: os.system() expects 1 string argument.");
    }
    const std::string& cmd = std::get<std::string>(args[0]);
    int result = std::system(cmd.c_str());
    return static_cast<double>(result);
}

inline Value osExists(AdiArray* self, const std::vector<Value>& args) {
    if (args.empty() || !std::holds_alternative<std::string>(args[0])) {
        throw std::runtime_error("Runtime Error: os.exists() expects 1 string argument.");
    }
    const std::string& path = std::get<std::string>(args[0]);
    return std::filesystem::exists(path);
}

inline AdiInstance* createOSModule(VM* vm) {
    AdiStructDef* blueprint = vm->allocateObject<AdiStructDef>("OSModule", std::vector<std::string>{});
    AdiInstance* module = vm->allocateObject<AdiInstance>(blueprint);

    module->fields["getenv"] = vm->allocateObject<AdiNativeMethod>("getenv", osGetenv, nullptr);
    module->fields["system"] = vm->allocateObject<AdiNativeMethod>("system", osSystem, nullptr);
    module->fields["exists"] = vm->allocateObject<AdiNativeMethod>("exists", osExists, nullptr);

    return module;
}

#endif // ADILANG_OS_H