#ifndef ADILANG_STDIO_H
#define ADILANG_STDIO_H

#include "object.h"
#include "vm.h"
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>

inline Value ioScan(AdiArray* self, const std::vector<Value>& args) {
    if (args.size() > 1) {
        throw std::runtime_error("Runtime Error: scan() takes at most 1 argument.");
    }
    if (args.size() == 1 && std::holds_alternative<std::string>(args[0])) {
        std::cout << std::get<std::string>(args[0]);
    }
    std::string line;
    std::getline(std::cin, line);
    return line;
}

inline AdiInstance* createIOModule(VM* vm) {
    AdiStructDef* blueprint = vm->allocateObject<AdiStructDef>("StdIOModule", std::vector<std::string>{});
    AdiInstance* module = vm->allocateObject<AdiInstance>(blueprint);
    
    // Pass nullptr for self since it's a module function, not an array method
    module->fields["scan"] = vm->allocateObject<AdiNativeMethod>("scan", ioScan, nullptr);

    return module;
}

#endif