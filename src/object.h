#ifndef ADILANG_OBJECT_H
#define ADILANG_OBJECT_H

#include <string>
#include <vector>
#include <memory>

struct StructStmt;

struct AdiStructDef {
    std::string name;
    std::vector<std::string> fields;

    AdiStructDef() = default;
    AdiStructDef(std::string name, std::vector<std::string> fields)
        : name(std::move(name)), fields(std::move(fields)) {}
    
    explicit AdiStructDef(const StructStmt* stmt);
};

inline AdiStructDef::AdiStructDef(const StructStmt* stmt) {
    if (stmt) {
        name = stmt->name;
        fields = stmt->fields;
    }
}

using AdiStructBlueprint = AdiStructDef;

#endif // ADILANG_OBJECT_H