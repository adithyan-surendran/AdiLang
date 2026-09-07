#ifndef ADILANG_MATH_H
#define ADILANG_MATH_H

#include "object.h"
#include "vm.h"
#include <cmath>
#include <stdexcept>
#include <vector>

inline Value mathSqrt(AdiArray* self, const std::vector<Value>& args) {
    if (args.size() != 1 || !std::holds_alternative<double>(args[0])) {
        throw std::runtime_error("Runtime Error: sqrt() expects 1 numeric argument.");
    }
    return std::sqrt(std::get<double>(args[0]));
}

inline Value mathAbs(AdiArray* self, const std::vector<Value>& args) {
    if (args.empty() || !std::holds_alternative<double>(args[0])) {
        throw std::runtime_error("Runtime Error: abs() expects 1 numeric argument.");
    }
    return std::abs(std::get<double>(args[0]));
}

inline Value mathPow(AdiArray* self, const std::vector<Value>& args) {
    if (args.size() != 2 || !std::holds_alternative<double>(args[0]) || !std::holds_alternative<double>(args[1])) {
        throw std::runtime_error("Runtime Error: pow() expects 2 numeric arguments (base, exponent).");
    }
    return std::pow(std::get<double>(args[0]), std::get<double>(args[1]));
}

inline Value mathFloor(AdiArray* self, const std::vector<Value>& args) {
    if (args.size() != 1 || !std::holds_alternative<double>(args[0])) {
        throw std::runtime_error("Runtime Error: floor() expects 1 numeric argument.");
    }
    return std::floor(std::get<double>(args[0]));
}

inline Value mathCeil(AdiArray* self, const std::vector<Value>& args) {
    if (args.size() != 1 || !std::holds_alternative<double>(args[0])) {
        throw std::runtime_error("Runtime Error: ceil() expects 1 numeric argument.");
    }
    return std::ceil(std::get<double>(args[0]));
}

inline Value mathRound(AdiArray* self, const std::vector<Value>& args) {
    if (args.size() != 1 || !std::holds_alternative<double>(args[0])) {
        throw std::runtime_error("Runtime Error: round() expects 1 numeric argument.");
    }
    return std::round(std::get<double>(args[0]));
}

inline Value mathSin(AdiArray* self, const std::vector<Value>& args) {
    if (args.size() != 1 || !std::holds_alternative<double>(args[0])) {
        throw std::runtime_error("Runtime Error: sin() expects 1 numeric argument (radians).");
    }
    return std::sin(std::get<double>(args[0]));
}

inline Value mathCos(AdiArray* self, const std::vector<Value>& args) {
    if (args.size() != 1 || !std::holds_alternative<double>(args[0])) {
        throw std::runtime_error("Runtime Error: cos() expects 1 numeric argument (radians).");
    }
    return std::cos(std::get<double>(args[0]));
}

inline Value mathTan(AdiArray* self, const std::vector<Value>& args) {
    if (args.size() != 1 || !std::holds_alternative<double>(args[0])) {
        throw std::runtime_error("Runtime Error: tan() expects 1 numeric argument (radians).");
    }
    return std::tan(std::get<double>(args[0]));
}

inline AdiInstance* createMathModule(VM* vm) {
    AdiStructDef* blueprint = vm->allocateObject<AdiStructDef>("MathModule", std::vector<std::string>{});
    AdiInstance* module = vm->allocateObject<AdiInstance>(blueprint);
    
    // Core Functions
    module->fields["sqrt"] = vm->allocateObject<AdiNativeMethod>("sqrt", mathSqrt, nullptr);
    module->fields["abs"] = vm->allocateObject<AdiNativeMethod>("abs", mathAbs, nullptr);
    module->fields["pow"] = vm->allocateObject<AdiNativeMethod>("pow", mathPow, nullptr);
    
    // Rounding Functions
    module->fields["floor"] = vm->allocateObject<AdiNativeMethod>("floor", mathFloor, nullptr);
    module->fields["ceil"] = vm->allocateObject<AdiNativeMethod>("ceil", mathCeil, nullptr);
    module->fields["round"] = vm->allocateObject<AdiNativeMethod>("round", mathRound, nullptr);

    // Trigonometry
    module->fields["sin"] = vm->allocateObject<AdiNativeMethod>("sin", mathSin, nullptr);
    module->fields["cos"] = vm->allocateObject<AdiNativeMethod>("cos", mathCos, nullptr);
    module->fields["tan"] = vm->allocateObject<AdiNativeMethod>("tan", mathTan, nullptr);

    // Constants
    module->fields["PI"] = 3.141592653589793;
    module->fields["E"] = 2.718281828459045;

    return module;
}

#endif // ADILANG_MATH_H