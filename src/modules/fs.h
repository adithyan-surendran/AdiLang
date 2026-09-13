#ifndef ADILANG_FS_H
#define ADILANG_FS_H

#include "core/object.h"
#include "core/vm.h"
#include <fstream>
#include <sstream>

inline Value fsRead(AdiArray* self, const std::vector<Value>& args) {
    if (args.size() != 1 || !std::holds_alternative<std::string>(args[0])) {
        throw std::runtime_error("fs.read() expects a single string argument (filepath).");
    }
    std::string path = std::get<std::string>(args[0]);
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file for reading: " + path);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

inline Value fsWrite(AdiArray* self, const std::vector<Value>& args) {
    if (args.size() != 2 || !std::holds_alternative<std::string>(args[0]) || !std::holds_alternative<std::string>(args[1])) {
        throw std::runtime_error("fs.write() expects two string arguments (filepath, content).");
    }
    std::string path = std::get<std::string>(args[0]);
    std::string content = std::get<std::string>(args[1]);
    
    std::ofstream file(path, std::ios::out);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " + path);
    }
    file << content;
    return true;
}

inline Value fsAppend(AdiArray* self, const std::vector<Value>& args) {
    if (args.size() != 2 || !std::holds_alternative<std::string>(args[0]) || !std::holds_alternative<std::string>(args[1])) {
        throw std::runtime_error("fs.append() expects two string arguments (filepath, content).");
    }
    std::string path = std::get<std::string>(args[0]);
    std::string content = std::get<std::string>(args[1]);
    
    std::ofstream file(path, std::ios::app);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file for appending: " + path);
    }
    file << content;
    return true;
}

inline Value fsExists(AdiArray* self, const std::vector<Value>& args) {
    if (args.size() != 1 || !std::holds_alternative<std::string>(args[0])) {
        throw std::runtime_error("fs.exists() expects a single string argument (filepath).");
    }
    std::string path = std::get<std::string>(args[0]);
    std::ifstream file(path);
    return file.is_open();
}

inline Value createFSModule(VM* vm) {
    AdiArray* selfObj = vm->allocateObject<AdiArray>();
    AdiInstance* moduleInstance = vm->allocateObject<AdiInstance>(nullptr);

    moduleInstance->fields["read"] = vm->allocateObject<AdiNativeMethod>("read", fsRead, selfObj);
    moduleInstance->fields["write"] = vm->allocateObject<AdiNativeMethod>("write", fsWrite, selfObj);
    moduleInstance->fields["append"] = vm->allocateObject<AdiNativeMethod>("append", fsAppend, selfObj);
    moduleInstance->fields["exists"] = vm->allocateObject<AdiNativeMethod>("exists", fsExists, selfObj);

    return moduleInstance;
}

#endif // ADILANG_FS_H