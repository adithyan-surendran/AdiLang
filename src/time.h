#ifndef ADILANG_TIME_H
#define ADILANG_TIME_H

#include "object.h"
#include "vm.h"
#include <chrono>
#include <thread>
#include <vector>
#include <stdexcept>
#include <variant>

inline Value timeNow(AdiArray* self, const std::vector<Value>& args) {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    double seconds = std::chrono::duration<double>(duration).count();
    return seconds;
}

inline Value timeSleep(AdiArray* self, const std::vector<Value>& args) {
    if (args.empty() || !std::holds_alternative<double>(args[0])) {
        throw std::runtime_error("Runtime Error: time.sleep() expects 1 numeric argument (seconds).");
    }
    double seconds = std::get<double>(args[0]);
    if (seconds < 0) {
        seconds = 0;
    }
    std::this_thread::sleep_for(std::chrono::duration<double>(seconds));
    return false;
}

inline AdiInstance* createTimeModule(VM* vm) {
    AdiStructDef* blueprint = vm->allocateObject<AdiStructDef>("TimeModule", std::vector<std::string>{});
    AdiInstance* module = vm->allocateObject<AdiInstance>(blueprint);

    module->fields["now"] = vm->allocateObject<AdiNativeMethod>("now", timeNow, nullptr);
    module->fields["sleep"] = vm->allocateObject<AdiNativeMethod>("sleep", timeSleep, nullptr);

    return module;
}

#endif // ADILANG_TIME_H