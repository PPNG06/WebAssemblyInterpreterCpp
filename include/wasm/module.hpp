#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "wasm/types.hpp"

namespace wasm
{
struct Limits
{
    uint32_t min{0};
    std::optional<uint32_t> max;
};

enum class RefType : uint8_t
{
    FuncRef = 0x70,
    ExternRef = 0x6F,
};

struct TableType
{
    RefType element_type{RefType::FuncRef};
    Limits limits;
};

struct MemoryType
{
    Limits limits;
};

struct GlobalType
{
    ValueType value_type{ValueType::I32};
    bool is_mutable{false};
};

struct FunctionType
{
    std::vector<ValueType> params;
    std::vector<ValueType> results;
};

enum class ExternalKind : uint8_t
{
    Function = 0x00,
    Table = 0x01,
    Memory = 0x02,
    Global = 0x03,
};

struct ConstantExpression
{
    enum class Kind
    {
        I32Const,
        I64Const,
        F32Const,
        F64Const,
        GlobalGet,
        RefNull,
        RefFunc,
    };

    Kind kind{Kind::I32Const};
    Value value;
    uint32_t index{0}; // for global.get
};

struct Import
{
    std::string module;
    std::string name;
    ExternalKind kind{ExternalKind::Function};
    uint32_t type_index{0}; // for functions
    TableType table_type;
    MemoryType memory_type;
    GlobalType global_type;
};

struct Export
{
    std::string name;
    ExternalKind kind{ExternalKind::Function};
    uint32_t index{0};
};

struct LocalDecl
{
    uint32_t count{0};
    ValueType type{ValueType::I32};
};

struct Code
{
    std::vector<LocalDecl> locals;
    std::vector<uint8_t> body;
};

struct Global
{
    GlobalType type;
    ConstantExpression init;
};

struct ElementSegment
{
    uint32_t table_index{0};
    ConstantExpression offset;
    std::vector<uint32_t> func_indices;
};

struct DataSegment
{
    uint32_t memory_index{0};
    bool has_memory_index{true};
    bool is_passive{false};
    ConstantExpression offset;
    std::vector<uint8_t> bytes;
};

struct Module
{
    std::vector<FunctionType> types;
    std::vector<Import> imports;
    std::vector<uint32_t> functions; // type indices
    std::vector<TableType> tables;
    std::vector<MemoryType> memories;
    std::vector<Global> globals;
    std::vector<Export> exports;
    std::optional<uint32_t> start_function;
    std::vector<ElementSegment> elements;
    std::vector<Code> codes;
    std::vector<DataSegment> data_segments;
};

} // namespace wasm
