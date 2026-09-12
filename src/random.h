#ifndef ADILANG_RANDOM_H
#define ADILANG_RANDOM_H

#include "object.h"
#include "vm.h"
#include <random>
#include <vector>
#include <stdexcept>
#include <variant>

inline Value randomFloat(AdiArray* self, const std::vector<Value>& args) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dis(0.0, 1.0);
    return dis(gen);
}

inline Value randomInt(AdiArray* self, const std::vector<Value>& args) {
    if (args.size() < 2 || !std::holds_alternative<double>(args[0]) || !std::holds_alternative<double>(args[1])) {
        throw std::runtime_error("Runtime Error: random.int() expects (min, max) numeric arguments.");
    }
    int minVal = static_cast<int>(std::get<double>(args[0]));
    int maxVal = static_cast<int>(std::get<double>(args[1]));

    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(minVal, maxVal);
    return static_cast<double>(dis(gen));
}

inline AdiInstance* createRandomModule(VM* vm) {
    AdiStructDef* blueprint = vm->allocateObject<AdiStructDef>("RandomModule", std::vector<std::string>{});
    AdiInstance* module = vm->allocateObject<AdiInstance>(blueprint);

    module->fields["float"] = vm->allocateObject<AdiNativeMethod>("float", randomFloat, nullptr);
    module->fields["int"] = vm->allocateObject<AdiNativeMethod>("int", randomInt, nullptr);

    return module;
}

#endif // ADILANG_RANDOM_H