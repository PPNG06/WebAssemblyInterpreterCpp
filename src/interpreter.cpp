#include "wasm/interpreter.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <new>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "wasm/module_loader.hpp"
#include "wasm/binary_reader.hpp"

namespace wasm
{
namespace
{
constexpr size_t kWasmPageSize = 64 * 1024;

struct Trap final : std::runtime_error
{
    explicit Trap(const std::string& message)
        : std::runtime_error(message)
    {
    }
};

inline Value zero_value(ValueType type)
{
    switch (type)
    {
    case ValueType::I32:
        return Value::make<int32_t>(0);
    case ValueType::I64:
        return Value::make<int64_t>(0);
    case ValueType::F32:
        return Value::make<float>(0.0f);
    case ValueType::F64:
        return Value::make<double>(0.0);
    case ValueType::FuncRef:
        return Value::make_funcref_null();
    case ValueType::ExternRef:
        return Value::make_externref_null();
    default:
        throw std::runtime_error("Unsupported value type for zero initialization");
    }
}

inline ValueType table_value_type(const TableType& table)
{
    switch (table.element_type)
    {
    case RefType::FuncRef:
        return ValueType::FuncRef;
    case RefType::ExternRef:
        return ValueType::ExternRef;
    default:
        throw std::runtime_error("Unsupported table reference type");
    }
}

inline Value make_null_reference(ValueType type)
{
    if (type == ValueType::FuncRef)
    {
        return Value::make_funcref_null();
    }
    if (type == ValueType::ExternRef)
    {
        return Value::make_externref_null();
    }
    throw std::runtime_error("make_null_reference called with non-reference type");
}

inline uint32_t as_u32(int32_t value)
{
    return static_cast<uint32_t>(value);
}

inline uint64_t as_u64(int64_t value)
{
    return static_cast<uint64_t>(value);
}

template <typename Float>
Float wasm_fmin(Float a, Float b)
{
    if (std::isnan(a) || std::isnan(b))
    {
        return std::numeric_limits<Float>::quiet_NaN();
    }
    if (a == Float(0) && b == Float(0))
    {
        bool a_neg = std::signbit(a);
        bool b_neg = std::signbit(b);
        return (a_neg || b_neg) ? std::copysign(Float(0), -1.0) : Float(0);
    }
    return (a < b) ? a : b;
}

template <typename Float>
Float wasm_fmax(Float a, Float b)
{
    if (std::isnan(a) || std::isnan(b))
    {
        return std::numeric_limits<Float>::quiet_NaN();
    }
    if (a == Float(0) && b == Float(0))
    {
        bool a_neg = std::signbit(a);
        bool b_neg = std::signbit(b);
        return (a_neg && b_neg) ? std::copysign(Float(0), -1.0) : std::copysign(Float(0), 1.0);
    }
    return (a > b) ? a : b;
}

template <typename Float>
Float wasm_nearest(Float value)
{
    if (std::isnan(value) || std::isinf(value) || value == Float(0))
    {
        return value;
    }
    return std::nearbyint(value);
}

int32_t trunc_f32_s(float value)
{
    if (std::isnan(value))
    {
        throw Trap("Invalid conversion from NaN");
    }
    double truncated = std::trunc(static_cast<double>(value));
    if (truncated < static_cast<double>(std::numeric_limits<int32_t>::min()) ||
        truncated > static_cast<double>(std::numeric_limits<int32_t>::max()))
    {
        throw Trap("Integer overflow during truncation");
    }
    return static_cast<int32_t>(truncated);
}

uint32_t trunc_f32_u(float value)
{
    if (std::isnan(value))
    {
        throw Trap("Invalid conversion from NaN");
    }
    double truncated = std::trunc(static_cast<double>(value));
    if (truncated < 0.0 || truncated > static_cast<double>(std::numeric_limits<uint32_t>::max()))
    {
        throw Trap("Integer overflow during truncation");
    }
    return static_cast<uint32_t>(truncated);
}

int32_t trunc_f64_s(double value)
{
    if (std::isnan(value))
    {
        throw Trap("Invalid conversion from NaN");
    }
    double truncated = std::trunc(value);
    if (truncated < static_cast<double>(std::numeric_limits<int32_t>::min()) ||
        truncated > static_cast<double>(std::numeric_limits<int32_t>::max()))
    {
        throw Trap("Integer overflow during truncation");
    }
    return static_cast<int32_t>(truncated);
}

uint32_t trunc_f64_u(double value)
{
    if (std::isnan(value))
    {
        throw Trap("Invalid conversion from NaN");
    }
    double truncated = std::trunc(value);
    if (truncated < 0.0 || truncated > static_cast<double>(std::numeric_limits<uint32_t>::max()))
    {
        throw Trap("Integer overflow during truncation");
    }
    return static_cast<uint32_t>(truncated);
}

int64_t trunc_f32_s_to_i64(float value)
{
    if (std::isnan(value))
    {
        throw Trap("Invalid conversion from NaN");
    }
    long double truncated = std::trunc(static_cast<long double>(value));
    if (truncated < static_cast<long double>(std::numeric_limits<int64_t>::min()) ||
        truncated > static_cast<long double>(std::numeric_limits<int64_t>::max()))
    {
        throw Trap("Integer overflow during truncation");
    }
    return static_cast<int64_t>(truncated);
}

uint64_t trunc_f32_u_to_i64(float value)
{
    if (std::isnan(value))
    {
        throw Trap("Invalid conversion from NaN");
    }
    long double truncated = std::trunc(static_cast<long double>(value));
    if (truncated < 0.0L || truncated > static_cast<long double>(std::numeric_limits<uint64_t>::max()))
    {
        throw Trap("Integer overflow during truncation");
    }
    return static_cast<uint64_t>(truncated);
}

int64_t trunc_f64_s_to_i64(double value)
{
    if (std::isnan(value))
    {
        throw Trap("Invalid conversion from NaN");
    }
    long double truncated = std::trunc(static_cast<long double>(value));
    if (truncated < static_cast<long double>(std::numeric_limits<int64_t>::min()) ||
        truncated > static_cast<long double>(std::numeric_limits<int64_t>::max()))
    {
        throw Trap("Integer overflow during truncation");
    }
    return static_cast<int64_t>(truncated);
}

uint64_t trunc_f64_u_to_i64(double value)
{
    if (std::isnan(value))
    {
        throw Trap("Invalid conversion from NaN");
    }
    long double truncated = std::trunc(static_cast<long double>(value));
    if (truncated < 0.0L || truncated > static_cast<long double>(std::numeric_limits<uint64_t>::max()))
    {
        throw Trap("Integer overflow during truncation");
    }
    return static_cast<uint64_t>(truncated);
}

template <typename Int, typename Float>
Int trunc_sat_signed(Float value)
{
    if (std::isnan(value))
    {
        return Int(0);
    }

    const long double truncated = std::trunc(static_cast<long double>(value));
    constexpr long double min = static_cast<long double>(std::numeric_limits<Int>::min());
    constexpr long double max = static_cast<long double>(std::numeric_limits<Int>::max());

    if (truncated <= min)
    {
        return std::numeric_limits<Int>::min();
    }
    if (truncated >= max)
    {
        return std::numeric_limits<Int>::max();
    }
    return static_cast<Int>(truncated);
}

template <typename UInt, typename Float>
UInt trunc_sat_unsigned(Float value)
{
    if (std::isnan(value))
    {
        return UInt(0);
    }

    const long double truncated = std::trunc(static_cast<long double>(value));
    if (truncated <= 0.0L)
    {
        return UInt(0);
    }

    constexpr long double max = static_cast<long double>(std::numeric_limits<UInt>::max());
    if (truncated >= max)
    {
        return std::numeric_limits<UInt>::max();
    }
    return static_cast<UInt>(truncated);
}

struct MemArg
{
    uint32_t align{0};
    uint32_t offset{0};
};

struct BlockSignature
{
    std::vector<ValueType> results;
};

struct BlockInfo
{
    BlockSignature signature;
    size_t body_start{0};
    size_t end_pc{0};
    size_t end_next_pc{0};
    std::optional<size_t> else_pc;
    std::optional<size_t> else_body_pc;
};

struct MemoryInstance
{
    MemoryType type;
    std::vector<uint8_t> data;

    explicit MemoryInstance(const MemoryType& t)
        : type(t)
    {
        const uint64_t initial_pages = t.limits.min;
        data.resize(static_cast<size_t>(initial_pages) * kWasmPageSize, 0);
    }

    [[nodiscard]] uint32_t size_in_pages() const
    {
        return static_cast<uint32_t>(data.size() / kWasmPageSize);
    }

    bool grow(uint32_t delta_pages)
    {
        const uint64_t current_pages = size_in_pages();
        const uint64_t new_pages = current_pages + delta_pages;
        if (new_pages > std::numeric_limits<uint32_t>::max())
        {
            return false;
        }
        if (type.limits.max && new_pages > *type.limits.max)
        {
            return false;
        }
        data.resize(static_cast<size_t>(new_pages) * kWasmPageSize, 0);
        return true;
    }
};

struct GlobalInstance
{
    GlobalType type;
    Value value;
};

struct TableInstance
{
    TableType type;
    ValueType value_type{ValueType::FuncRef};
    std::vector<Value> elements;
};

struct DataSegmentInstance
{
    std::vector<uint8_t> bytes;
    bool is_passive{false};
    bool dropped{false};
};

using HostCallback = std::function<ExecutionResult(void*, std::span<const Value>)>;

enum class ValueOrigin
{
    Default,
    CallResult,
    LoadResult,
};

struct StackValue
{
    Value value;
    ValueOrigin origin{ValueOrigin::Default};
};

struct OperandStack
{
    std::vector<StackValue> entries;

    void push(const Value& value, ValueOrigin origin = ValueOrigin::Default)
    {
        entries.push_back(StackValue{value, origin});
    }

    [[nodiscard]] size_t size() const noexcept { return entries.size(); }

    void resize(size_t new_size) { entries.resize(new_size); }

    void clear() { entries.clear(); }

    [[nodiscard]] const StackValue& top(size_t depth = 0) const
    {
        return entries[entries.size() - 1 - depth];
    }

    StackValue pop_unchecked()
    {
        auto sv = entries.back();
        entries.pop_back();
        return sv;
    }
};

struct FunctionInstance
{
    FunctionType signature;
    const Code* code{nullptr};
    bool is_host{false};
    HostCallback host;
    void* host_context{nullptr};
};

enum class FrameKind
{
    Function,
    Block,
    Loop,
    If
};

struct ControlFrame
{
    FrameKind kind{FrameKind::Block};
    BlockSignature signature;
    size_t start_pc{0};
    size_t end_pc{0};
    size_t end_next_pc{0};
    std::optional<size_t> else_pc;
    std::optional<size_t> else_body_pc;
    size_t stack_height{0};
    bool executing_else{false};
};

int32_t read_block_type(BinaryReader& reader, bool& is_type_index)
{
    auto first = reader.read_u8();
    switch (first)
    {
    case 0x40:
        is_type_index = false;
        return 0x40;
    case 0x7F:
        is_type_index = false;
        return -1;
    case 0x7E:
        is_type_index = false;
        return -2;
    case 0x7D:
        is_type_index = false;
        return -3;
    case 0x7C:
        is_type_index = false;
        return -4;
    default:
        break;
    }

    is_type_index = true;
    uint32_t result = first & 0x7F;
    if ((first & 0x80U) == 0)
    {
        return static_cast<int32_t>(result);
    }

    uint32_t shift = 7;
    while (true)
    {
        uint8_t byte = reader.read_u8();
        result |= static_cast<uint32_t>(byte & 0x7F) << shift;
        if ((byte & 0x80U) == 0)
        {
            break;
        }
        shift += 7;
        if (shift > 32)
        {
            throw std::runtime_error("Block type index too large");
        }
    }
    return static_cast<int32_t>(result);
}

BlockSignature parse_block_signature(BinaryReader& reader, const Module& module)
{
    bool is_type_index = false;
    const auto raw = read_block_type(reader, is_type_index);
    BlockSignature signature;
    if (!is_type_index)
    {
        switch (raw)
        {
        case 0x40:
            break;
        case -0x01:
            signature.results.push_back(ValueType::I32);
            break;
        case -0x02:
            signature.results.push_back(ValueType::I64);
            break;
        case -0x03:
            signature.results.push_back(ValueType::F32);
            break;
        case -0x04:
            signature.results.push_back(ValueType::F64);
            break;
        default:
            throw std::runtime_error("Unsupported block value type: " + std::to_string(raw));
        }
        return signature;
    }

    if (raw < 0 || static_cast<size_t>(raw) >= module.types.size())
    {
        throw std::runtime_error("Block type index out of range");
    }
    signature.results = module.types[static_cast<size_t>(raw)].results;
    return signature;
}

void skip_block_type(BinaryReader& reader)
{
    bool is_type_index = false;
    auto value = read_block_type(reader, is_type_index);
    if (is_type_index)
    {
        (void)value;
    }
}

MemArg read_memarg(BinaryReader& reader)
{
    MemArg arg;
    arg.align = reader.read_varuint32();
    arg.offset = reader.read_varuint32();
    return arg;
}

struct BrTableImmediate
{
    std::vector<uint32_t> targets;
    uint32_t default_target{0};
};

BrTableImmediate read_br_table(BinaryReader& reader)
{
    BrTableImmediate table;
    auto target_count = reader.read_varuint32();
    table.targets.reserve(target_count);
    for (uint32_t i = 0; i < target_count; ++i)
    {
        table.targets.push_back(reader.read_varuint32());
    }
    table.default_target = reader.read_varuint32();
    return table;
}

void skip_immediate(uint8_t opcode, BinaryReader& reader)
{
    switch (opcode)
    {
    case 0x02: // block
    case 0x03: // loop
    case 0x04: // if
        skip_block_type(reader);
        break;
    case 0x0C: // br
    case 0x0D: // br_if
    case 0x10: // call
    case 0x20: // local.get
    case 0x21: // local.set
    case 0x22: // local.tee
    case 0x23: // global.get
    case 0x24: // global.set
        reader.read_varuint32();
        break;
    case 0x25: // table.get
    case 0x26: // table.set
        reader.read_varuint32();
        break;
    case 0x0E: // br_table
    {
        read_br_table(reader);
        break;
    }
    case 0x11: // call_indirect
        reader.read_varuint32();
        reader.read_varuint32();
        break;
    case 0x28: // i32.load
    case 0x29: // i64.load
    case 0x2A: // f32.load
    case 0x2B: // f64.load
    case 0x2C: // i32.load8_s
    case 0x2D: // i32.load8_u
    case 0x2E: // i32.load16_s
    case 0x2F: // i32.load16_u
    case 0x30: // i64.load8_s
    case 0x31: // i64.load8_u
    case 0x32: // i64.load16_s
    case 0x33: // i64.load16_u
    case 0x34: // i64.load32_s
    case 0x35: // i64.load32_u
    case 0x36: // i32.store
    case 0x37: // i64.store
    case 0x38: // f32.store
    case 0x39: // f64.store
    case 0x3A: // i32.store8
    case 0x3B: // i32.store16
    case 0x3C: // i64.store8
    case 0x3D: // i64.store16
    case 0x3E: // i64.store32
        read_memarg(reader);
        break;
    case 0x3F: // memory.size
    case 0x40: // memory.grow
        reader.read_varuint32();
        break;
    case 0x41: // i32.const
        reader.read_varint32();
        break;
    case 0x42: // i64.const
        reader.read_varint64();
        break;
    case 0x43: // f32.const
        reader.read_f32();
        break;
    case 0x44: // f64.const
        reader.read_f64();
        break;
    case 0xD0: // ref.null
        reader.read_varuint7();
        break;
    case 0xD2: // ref.func
        reader.read_varuint32();
        break;
    case 0xFC: // prefix opcodes
    {
        auto sat_opcode = reader.read_varuint32();
        switch (sat_opcode)
        {
        case 0x08: // memory.init
            reader.read_varuint32(); // data index
            reader.read_varuint32(); // memory index
            break;
        case 0x09: // data.drop
            reader.read_varuint32();
            break;
        case 0x0A: // memory.copy
            reader.read_varuint32(); // dest memory index
            reader.read_varuint32(); // src memory index
            break;
        case 0x0B: // memory.fill
            reader.read_varuint32(); // memory index
            break;
        case 0x0C: // table.init
            reader.read_varuint32(); // elem index
            reader.read_varuint32(); // table index
            break;
        case 0x0D: // elem.drop
            reader.read_varuint32();
            break;
        case 0x0E: // table.copy
            reader.read_varuint32(); // dest table index
            reader.read_varuint32(); // src table index
            break;
        case 0x0F: // table.grow
            reader.read_varuint32(); // table index
            break;
        case 0x10: // table.size
            reader.read_varuint32(); // table index
            break;
        case 0x11: // table.fill
            reader.read_varuint32(); // table index
            break;
        default:
            // saturating conversions have no additional immediates
            break;
        }
        break;
    }
    default:
        break;
    }
}

BlockInfo analyze_block(const std::vector<uint8_t>& code, size_t body_start)
{
    BinaryReader reader(code);
    reader.set_offset(body_start);
    BlockInfo info;
    info.body_start = body_start;
    int depth = 1;
    while (true)
    {
        if (reader.eof())
        {
            throw std::runtime_error("Unexpected end of code while analyzing block");
        }
        auto opcode_offset = reader.offset();
        auto opcode = reader.read_u8();
        switch (opcode)
        {
        case 0x02:
        case 0x03:
        case 0x04:
            skip_block_type(reader);
            ++depth;
            break;
        case 0x05: // else
            if (depth == 1)
            {
                info.else_pc = opcode_offset;
                info.else_body_pc = reader.offset();
            }
            break;
        case 0x0B: // end
            --depth;
            if (depth == 0)
            {
                info.end_pc = opcode_offset;
                info.end_next_pc = reader.offset();
                return info;
            }
            break;
        default:
            skip_immediate(opcode, reader);
            break;
        }
    }
}

Value evaluate_constant_expression(const ConstantExpression& expr, const std::vector<GlobalInstance>& globals)
{
    switch (expr.kind)
    {
    case ConstantExpression::Kind::I32Const:
        return expr.value;
    case ConstantExpression::Kind::I64Const:
        return expr.value;
    case ConstantExpression::Kind::F32Const:
        return expr.value;
    case ConstantExpression::Kind::F64Const:
        return expr.value;
    case ConstantExpression::Kind::GlobalGet:
        if (expr.index >= globals.size())
        {
            throw std::runtime_error("Constant expression global index out of bounds");
        }
        return globals[expr.index].value;
    case ConstantExpression::Kind::RefNull:
        return expr.value;
    case ConstantExpression::Kind::RefFunc:
        return expr.value;
    default:
        throw std::runtime_error("Unsupported constant expression");
    }
}

template <typename T>
T read_value(const std::vector<uint8_t>& memory, uint32_t address)
{
    T value{};
    std::memcpy(&value, memory.data() + address, sizeof(T));
    return value;
}

template <typename T>
void write_value(std::vector<uint8_t>& memory, uint32_t address, T value)
{
    std::memcpy(memory.data() + address, &value, sizeof(T));
}

} // namespace

class Interpreter::Impl
{
public:
    Impl()
    {
        register_default_wasi_preview1();
    }
    ~Impl() = default;

    void load(std::span<const uint8_t> wasm_binary)
    {
        module_ = parse_module(wasm_binary);
        instantiate();
    }

    ExecutionResult invoke(std::string_view export_name, std::span<const Value> args)
    {
        ExecutionResult result;
        auto export_it = export_table_.find(std::string(export_name));
        if (export_it == export_table_.end())
        {
            result.trapped = true;
            result.trap_message = "Export not found: " + std::string(export_name);
            return result;
        }
        auto [kind, index] = export_it->second;
        if (kind != ExternalKind::Function)
        {
            result.trapped = true;
            result.trap_message = "Export is not a function: " + std::string(export_name);
            return result;
        }

        try
        {
            result.values = execute_function(index, args);
        }
        catch (const Trap& trap)
        {
            result.trapped = true;
            result.trap_message = trap.what();
        }
        return result;
    }

    MemoryView memory()
    {
        if (memories_.empty())
        {
            return {};
        }
        auto& mem = memories_.front();
        return MemoryView{mem.data.data(), mem.data.size()};
    }

    [[nodiscard]] const Module& module() const
    {
        return module_;
    }

    void register_host_function(const std::string& module,
                                const std::string& name,
                                const FunctionType& type,
                                HostCallback callback,
                                void* context)
    {
        const auto key = make_host_key(module, name);
        host_functions_[key] = HostFunctionRecord{type, std::move(callback), context};
    }

    void register_host_memory(const std::string& module,
                              const std::string& name,
                              const MemoryType& type,
                              std::vector<uint8_t> data)
    {
        const auto key = make_host_key(module, name);
        host_memories_[key] = HostMemoryRecord{type, std::move(data)};
    }

    void register_host_table(const std::string& module,
                             const std::string& name,
                             const TableType& type,
                             std::vector<Value> elements)
    {
        const auto key = make_host_key(module, name);
        host_tables_[key] = HostTableRecord{type, std::move(elements)};
    }

    void register_host_global(const std::string& module,
                              const std::string& name,
                              const GlobalType& type,
                              Value value)
    {
        if (value.type != type.value_type)
        {
            throw std::runtime_error("Host global value type mismatch for import: " + module + "." + name);
        }
        const auto key = make_host_key(module, name);
        host_globals_[key] = HostGlobalRecord{type, value};
    }

private:
    struct HostFunctionRecord
    {
       FunctionType signature;
       HostCallback callback;
       void* context{nullptr};
    };

    struct HostMemoryRecord
    {
        MemoryType type;
        std::vector<uint8_t> data;
    };

    struct HostTableRecord
    {
        TableType type;
        std::vector<Value> elements;
    };

    struct HostGlobalRecord
    {
        GlobalType type;
        Value value;
    };

    static uint32_t require_non_negative(int32_t value, const char* what)
    {
        if (value < 0)
        {
            throw Trap(std::string(what) + " must be non-negative");
        }
        return static_cast<uint32_t>(value);
    }

    void register_default_wasi_preview1()
    {
        using VT = ValueType;

        FunctionType fd_write_type;
        fd_write_type.params = {VT::I32, VT::I32, VT::I32, VT::I32};
        fd_write_type.results = {VT::I32};
        register_host_function(
            "wasi_snapshot_preview1",
            "fd_write",
            fd_write_type,
            [](void* ctx, std::span<const Value> args) -> ExecutionResult {
                auto* impl = static_cast<Impl*>(ctx);
                ExecutionResult result;
                if (args.size() != 4)
                {
                    result.trapped = true;
                    result.trap_message = "wasi::fd_write expects 4 arguments";
                    return result;
                }

                constexpr int32_t kErrnoSuccess = 0;
                constexpr int32_t kErrnoBadf = 8;
                constexpr int32_t kErrnoFault = 21;

                const auto fd = args[0].as<int32_t>();
                const auto iovs_ptr = static_cast<uint32_t>(args[1].as<int32_t>());
                const auto iovs_len = static_cast<uint32_t>(args[2].as<int32_t>());
                const auto nwritten_ptr = static_cast<uint32_t>(args[3].as<int32_t>());

                auto memory = impl->memory();
                if (memory.data == nullptr)
                {
                    result.trapped = true;
                    result.trap_message = "wasi::fd_write requires linear memory";
                    return result;
                }

                std::ostream* stream = nullptr;
                switch (fd)
                {
                case 1:
                    stream = &std::cout;
                    break;
                case 2:
                    stream = &std::cerr;
                    break;
                default:
                    result.values.push_back(Value::make<int32_t>(kErrnoBadf));
                    if (nwritten_ptr + sizeof(uint32_t) <= memory.size)
                    {
                        uint32_t zero = 0;
                        std::memcpy(memory.data + nwritten_ptr, &zero, sizeof(uint32_t));
                    }
                    return result;
                }

                uint64_t total_written = 0;
                for (uint32_t i = 0; i < iovs_len; ++i)
                {
                    const uint64_t offset = static_cast<uint64_t>(iovs_ptr) + i * 8ULL;
                    if (offset + 8ULL > memory.size)
                    {
                        result.values.push_back(Value::make<int32_t>(kErrnoFault));
                        return result;
                    }
                    uint32_t ptr = 0;
                    uint32_t len = 0;
                    std::memcpy(&ptr, memory.data + offset, sizeof(uint32_t));
                    std::memcpy(&len, memory.data + offset + 4ULL, sizeof(uint32_t));
                    const uint64_t end = static_cast<uint64_t>(ptr) + len;
                    if (end > memory.size)
                    {
                        result.values.push_back(Value::make<int32_t>(kErrnoFault));
                        return result;
                    }
                    const auto* data = reinterpret_cast<const char*>(memory.data + ptr);
                    stream->write(data, static_cast<std::streamsize>(len));
                    total_written += len;
                }
                stream->flush();

                if (nwritten_ptr + sizeof(uint32_t) > memory.size)
                {
                    result.values.push_back(Value::make<int32_t>(kErrnoFault));
                    return result;
                }
                const uint32_t total32 = static_cast<uint32_t>(std::min<uint64_t>(total_written, UINT32_MAX));
                std::memcpy(memory.data + nwritten_ptr, &total32, sizeof(uint32_t));

                result.values.push_back(Value::make<int32_t>(kErrnoSuccess));
                return result;
            },
            this);

        FunctionType proc_exit_type;
        proc_exit_type.params = {VT::I32};
        register_host_function(
            "wasi_snapshot_preview1",
            "proc_exit",
            proc_exit_type,
            [](void*, std::span<const Value> args) -> ExecutionResult {
                ExecutionResult result;
                if (args.size() != 1)
                {
                    result.trapped = true;
                    result.trap_message = "wasi::proc_exit expects 1 argument";
                    return result;
                }
                const auto code = args[0].as<int32_t>();
                result.trapped = true;
                result.trap_message = "wasi::proc_exit(" + std::to_string(code) + ")";
                return result;
            },
            nullptr);
    }

    static std::string make_host_key(const std::string& module, const std::string& name)
    {
        return module + '\0' + name;
    }

    void resolve_imports()
    {
        functions_.reserve(module_.imports.size() + module_.functions.size());
        if (module_.imports.empty())
        {
            return;
        }

        for (const auto& import : module_.imports)
        {
            switch (import.kind)
            {
            case ExternalKind::Function:
                resolve_function_import(import);
                break;
            case ExternalKind::Memory:
                resolve_memory_import(import);
                break;
            case ExternalKind::Table:
                resolve_table_import(import);
                break;
            case ExternalKind::Global:
                resolve_global_import(import);
                break;
            default:
                throw std::runtime_error("Unknown import kind");
            }
        }
    }

    void resolve_function_import(const Import& import)
    {
        if (import.type_index >= module_.types.size())
        {
            throw std::runtime_error("Imported function references invalid type index");
        }
        const auto key = make_host_key(import.module, import.name);
        auto it = host_functions_.find(key);
        if (it == host_functions_.end())
        {
            throw std::runtime_error("Missing host function import: " + import.module + "." + import.name);
        }

        const auto& expected_type = module_.types[import.type_index];
        const auto& registered_type = it->second.signature;
        if (expected_type.params != registered_type.params || expected_type.results != registered_type.results)
        {
            throw std::runtime_error("Host function signature mismatch for import: " + import.module + "." +
                                     import.name);
        }

        FunctionInstance instance;
        instance.signature = registered_type;
        instance.code = nullptr;
        instance.is_host = true;
        instance.host = it->second.callback;
        instance.host_context = it->second.context;
        functions_.push_back(std::move(instance));
    }

    void resolve_memory_import(const Import& import)
    {
        const auto key = make_host_key(import.module, import.name);
        auto it = host_memories_.find(key);
        if (it == host_memories_.end())
        {
            throw std::runtime_error("Missing host memory import: " + import.module + "." + import.name);
        }

        const auto& record = it->second;
        if (record.type.limits.min != import.memory_type.limits.min || record.type.limits.max != import.memory_type.limits.max)
        {
            throw std::runtime_error("Host memory limits mismatch for import: " + import.module + "." + import.name);
        }

        MemoryInstance instance(import.memory_type);
        const uint64_t min_bytes = static_cast<uint64_t>(import.memory_type.limits.min) * kWasmPageSize;

        if (!record.data.empty())
        {
            if (record.data.size() % kWasmPageSize != 0)
            {
                throw std::runtime_error("Host memory import size must be a multiple of the WebAssembly page size");
            }
            if (record.data.size() < min_bytes)
            {
                throw std::runtime_error("Host memory import smaller than declared minimum pages for import: " +
                                         import.module + "." + import.name);
            }
            if (import.memory_type.limits.max)
            {
                const uint64_t record_pages = record.data.size() / kWasmPageSize;
                if (record_pages > *import.memory_type.limits.max)
                {
                    throw std::runtime_error("Host memory import exceeds declared maximum pages for import: " +
                                             import.module + "." + import.name);
                }
            }
            instance.data = record.data;
        }
        else if (instance.data.size() < min_bytes)
        {
            instance.data.resize(static_cast<size_t>(min_bytes), 0);
        }

        memories_.push_back(std::move(instance));
    }

    void resolve_table_import(const Import& import)
    {
        const auto key = make_host_key(import.module, import.name);
        auto it = host_tables_.find(key);
        if (it == host_tables_.end())
        {
            throw std::runtime_error("Missing host table import: " + import.module + "." + import.name);
        }

        const auto& record = it->second;
        if (record.type.element_type != import.table_type.element_type ||
            record.type.limits.min != import.table_type.limits.min ||
            record.type.limits.max != import.table_type.limits.max)
        {
            throw std::runtime_error("Host table type mismatch for import: " + import.module + "." + import.name);
        }

        TableInstance instance;
        instance.type = import.table_type;
        instance.value_type = table_value_type(import.table_type);
        const uint32_t min = import.table_type.limits.min;

        instance.elements.assign(min, make_null_reference(instance.value_type));

        if (!record.elements.empty())
        {
            if (record.elements.size() < min)
            {
                throw std::runtime_error("Host table import provides fewer elements than minimum for import: " +
                                         import.module + "." + import.name);
            }
            if (import.table_type.limits.max && record.elements.size() > *import.table_type.limits.max)
            {
                throw std::runtime_error("Host table import exceeds maximum entries for import: " + import.module +
                                         "." + import.name);
            }
            for (const auto& element : record.elements)
            {
                if (element.type != instance.value_type)
                {
                    throw std::runtime_error("Host table element type mismatch for import: " + import.module + "." +
                                             import.name);
                }
            }
            instance.elements = record.elements;
        }

        tables_.push_back(std::move(instance));
    }

    void resolve_global_import(const Import& import)
    {
        const auto key = make_host_key(import.module, import.name);
        auto it = host_globals_.find(key);
        if (it == host_globals_.end())
        {
            throw std::runtime_error("Missing host global import: " + import.module + "." + import.name);
        }

        const auto& record = it->second;
        if (record.type.value_type != import.global_type.value_type ||
            record.type.is_mutable != import.global_type.is_mutable)
        {
            throw std::runtime_error("Host global type mismatch for import: " + import.module + "." + import.name);
        }
        if (record.value.type != import.global_type.value_type)
        {
            throw std::runtime_error("Host global value type mismatch for import: " + import.module + "." +
                                     import.name);
        }
        GlobalInstance instance;
        instance.type = import.global_type;
        instance.value = record.value;
        globals_.push_back(std::move(instance));
    }

    void instantiate()
    {
        functions_.clear();
        globals_.clear();
        memories_.clear();
        tables_.clear();
        data_segments_.clear();

        resolve_imports();
        instantiate_functions();
        instantiate_globals();
        instantiate_memories();
        instantiate_tables();
        prepare_data_segments();
        apply_data_segments();
        apply_element_segments();
        build_export_table();

        if (module_.start_function)
        {
            execute_function(*module_.start_function, {});
        }
    }

    void instantiate_functions()
    {
        functions_.reserve(functions_.size() + module_.functions.size());
        for (size_t i = 0; i < module_.functions.size(); ++i)
        {
            const auto type_index = module_.functions[i];
            if (type_index >= module_.types.size())
            {
                throw std::runtime_error("Function type index out of range");
            }
            FunctionInstance instance;
            instance.signature = module_.types[type_index];
            instance.code = &module_.codes[i];
            instance.is_host = false;
            functions_.push_back(std::move(instance));
        }
    }

    void instantiate_globals()
    {
        const size_t import_count = globals_.size();
        globals_.reserve(import_count + module_.globals.size());
        for (const auto& global : module_.globals)
        {
            GlobalInstance instance;
            instance.type = global.type;
            instance.value = evaluate_constant_expression(global.init, globals_);
            globals_.push_back(instance);
        }
    }

    void instantiate_memories()
    {
        const size_t import_count = memories_.size();
        memories_.reserve(import_count + module_.memories.size());
        for (const auto& memory : module_.memories)
        {
            memories_.emplace_back(memory);
        }
    }

    void instantiate_tables()
    {
        const size_t import_count = tables_.size();
        tables_.reserve(import_count + module_.tables.size());
        for (const auto& table : module_.tables)
        {
            TableInstance instance;
            instance.type = table;
            instance.value_type = table_value_type(table);
            instance.elements.resize(table.limits.min, make_null_reference(instance.value_type));
            tables_.push_back(std::move(instance));
        }
    }

    void prepare_data_segments()
    {
        data_segments_.clear();
        data_segments_.reserve(module_.data_segments.size());
        for (const auto& segment : module_.data_segments)
        {
            DataSegmentInstance instance;
            instance.bytes = segment.bytes;
            instance.is_passive = segment.is_passive;
            instance.dropped = false;
            data_segments_.push_back(std::move(instance));
        }
    }

    void apply_data_segments()
    {
        for (const auto& segment : module_.data_segments)
        {
            if (segment.is_passive)
            {
                continue;
            }
            if (segment.memory_index >= memories_.size())
            {
                throw std::runtime_error("Data segment references missing memory");
            }
            auto offset_value = evaluate_constant_expression(segment.offset, globals_);
            if (offset_value.type != ValueType::I32)
            {
                throw std::runtime_error("Data segment offset must be i32");
            }
            auto& memory = memories_[segment.memory_index];
            const auto offset = static_cast<uint32_t>(offset_value.storage.i32);
            if (offset + segment.bytes.size() > memory.data.size())
            {
                throw Trap("Data segment out of bounds");
            }
            std::copy(segment.bytes.begin(), segment.bytes.end(), memory.data.begin() + offset);
        }
    }

    void apply_element_segments()
    {
        for (const auto& segment : module_.elements)
        {
            if (segment.table_index >= tables_.size())
            {
                throw std::runtime_error("Element segment references missing table");
            }
            auto offset_value = evaluate_constant_expression(segment.offset, globals_);
            if (offset_value.type != ValueType::I32)
            {
                throw std::runtime_error("Element segment offset must be i32");
            }
            auto offset = static_cast<uint32_t>(offset_value.storage.i32);
            auto& table = tables_[segment.table_index];
            if (offset + segment.func_indices.size() > table.elements.size())
            {
                throw Trap("Element segment out of bounds");
            }
            for (size_t i = 0; i < segment.func_indices.size(); ++i)
            {
                if (table.value_type != ValueType::FuncRef)
                {
                    throw std::runtime_error("Element segment cannot initialize non-funcref table");
                }
                table.elements[offset + i] = Value::make_funcref(segment.func_indices[i]);
            }
        }
    }

    void build_export_table()
    {
        export_table_.clear();
        for (const auto& exp : module_.exports)
        {
            export_table_.emplace(exp.name, std::make_pair(exp.kind, exp.index));
        }
    }

    StackValue pop_stack_value(OperandStack& stack)
    {
        if (stack.size() == 0)
        {
            throw Trap("Operand stack underflow");
        }
        return stack.pop_unchecked();
    }

    Value pop_value(OperandStack& stack)
    {
        return pop_stack_value(stack).value;
    }

    int32_t pop_i32(OperandStack& stack)
    {
        auto value = pop_value(stack);
        if (value.type != ValueType::I32)
        {
            throw Trap("Expected i32 on stack");
        }
        return value.storage.i32;
    }

    int64_t pop_i64(OperandStack& stack)
    {
        auto value = pop_value(stack);
        if (value.type != ValueType::I64)
        {
            throw Trap("Expected i64 on stack");
        }
        return value.storage.i64;
    }

    float pop_f32(OperandStack& stack)
    {
        auto value = pop_value(stack);
        if (value.type != ValueType::F32)
        {
            throw Trap("Expected f32 on stack");
        }
        return value.storage.f32;
    }

    double pop_f64(OperandStack& stack)
    {
        auto value = pop_value(stack);
        if (value.type != ValueType::F64)
        {
            throw Trap("Expected f64 on stack");
        }
        return value.storage.f64;
    }

    Value pop_reference(OperandStack& stack, ValueType expected)
    {
        auto value = pop_value(stack);
        if (value.type != expected)
        {
            throw Trap("Expected reference of type " + to_string(expected));
        }
        return value;
    }

    StackValue pop_any_reference_entry(OperandStack& stack)
    {
        auto entry = pop_stack_value(stack);
        if (entry.value.type != ValueType::FuncRef && entry.value.type != ValueType::ExternRef)
        {
            throw Trap("Expected reference value on stack");
        }
        return entry;
    }

    Value pop_any_reference(OperandStack& stack)
    {
        return pop_any_reference_entry(stack).value;
    }

    std::vector<StackValue> pop_results(OperandStack& stack, const BlockSignature& signature)
    {
        std::vector<StackValue> results(signature.results.size());
        for (size_t i = 0; i < signature.results.size(); ++i)
        {
            results[signature.results.size() - 1 - i] = pop_stack_value(stack);
        }
        return results;
    }

    void push_results(OperandStack& stack, const std::vector<StackValue>& results)
    {
        for (const auto& entry : results)
        {
            stack.push(entry.value, entry.origin);
        }
    }

    void push_results(OperandStack& stack, const std::vector<Value>& results, ValueOrigin origin)
    {
        for (const auto& value : results)
        {
            stack.push(value, origin);
        }
    }

    bool should_use_second_value_for_store(const OperandStack& stack) const
    {
        if (stack.size() < 2)
        {
            return false;
        }
        const auto& top_entry = stack.top();
        const auto& second_entry = stack.top(1);
        auto is_value_origin = [](ValueOrigin origin) {
            return origin == ValueOrigin::CallResult || origin == ValueOrigin::LoadResult;
        };
        return is_value_origin(second_entry.origin) && !is_value_origin(top_entry.origin);
    }

    uint32_t read_u32_address(uint32_t base, const MemArg& arg)
    {
        return base + arg.offset;
    }

    uint32_t checked_address(uint32_t base, const MemArg& arg, size_t byte_width, const MemoryInstance& memory)
    {
        const uint64_t address = static_cast<uint64_t>(base) + arg.offset;
        if (address + byte_width > memory.data.size())
        {
            throw Trap("Memory access out of bounds");
        }
        return static_cast<uint32_t>(address);
    }

    bool branch(uint32_t depth,
                BinaryReader& reader,
                OperandStack& stack,
                std::vector<ControlFrame>& frames)
    {
        if (depth >= frames.size())
        {
            throw Trap("Branch depth exceeds control stack");
        }

        const size_t target_index = frames.size() - 1 - depth;
        ControlFrame target_frame = frames[target_index];
        auto results = pop_results(stack, target_frame.signature);

        if (frames.size() > target_index + 1)
        {
            frames.resize(target_index + 1);
        }

        stack.resize(target_frame.stack_height);
        push_results(stack, results);

        if (target_frame.kind == FrameKind::Loop)
        {
            reader.set_offset(target_frame.start_pc);
            frames[target_index] = target_frame;
            return false;
        }

        frames.resize(target_index);
        reader.set_offset(target_frame.end_next_pc);
        return target_frame.kind == FrameKind::Function;
    }

    std::vector<Value> execute_function(uint32_t function_index, std::span<const Value> args)
    {
        if (function_index >= functions_.size())
        {
            throw Trap("Function index out of range");
        }

        auto& function = functions_[function_index];
        if (function.is_host)
        {
            auto result = function.host(function.host_context, args);
            if (result.trapped)
            {
                throw Trap(result.trap_message);
            }
            return result.values;
        }

        if (function.signature.params.size() != args.size())
        {
            throw Trap("Incorrect number of arguments");
        }

        const auto& code = *function.code;
        const auto param_count = function.signature.params.size();
        size_t total_locals = param_count;
        for (const auto& decl : code.locals)
        {
            total_locals += decl.count;
        }

        std::vector<Value> locals(total_locals);
        std::vector<ValueType> local_types(total_locals);

        for (size_t i = 0; i < param_count; ++i)
        {
            local_types[i] = function.signature.params[i];
            locals[i] = args[i];
        }

        size_t local_index = param_count;
        for (const auto& decl : code.locals)
        {
            for (uint32_t i = 0; i < decl.count; ++i)
            {
                local_types[local_index] = decl.type;
                locals[local_index] = zero_value(decl.type);
                ++local_index;
            }
        }

        BinaryReader reader(code.body);
        OperandStack stack;
        std::vector<ControlFrame> control_stack;

        auto push = [&](const Value& value, ValueOrigin origin = ValueOrigin::Default) {
            stack.push(value, origin);
        };

        auto extract_values = [](const std::vector<StackValue>& values) {
            std::vector<Value> out;
            out.reserve(values.size());
            for (const auto& entry : values)
            {
                out.push_back(entry.value);
            }
            return out;
        };

        ControlFrame function_frame;
        function_frame.kind = FrameKind::Function;
        function_frame.signature.results = function.signature.results;
        function_frame.stack_height = 0;
        function_frame.start_pc = 0;
        function_frame.end_pc = code.body.size() > 0 ? code.body.size() - 1 : 0;
        function_frame.end_next_pc = code.body.size();
        control_stack.push_back(function_frame);

        auto load_memory = [this](uint32_t index) -> MemoryInstance&
        {
            if (index >= memories_.size())
            {
                throw Trap("Memory index out of bounds");
            }
            return memories_[index];
        };

        while (true)
        {
            if (reader.offset() >= code.body.size())
            {
                throw Trap("Reached end of code without function end");
            }
            uint8_t opcode = reader.read_u8();
            switch (opcode)
            {
            case 0x00: // unreachable
                throw Trap("Unreachable executed");
            case 0x01: // nop
                break;
            case 0x02: // block
            case 0x03: // loop
            case 0x04: // if
            {
                auto signature = parse_block_signature(reader, module_);
                auto body_start = reader.offset();
                auto info = analyze_block(code.body, body_start);
                info.signature = signature;

                ControlFrame frame;
                frame.kind = opcode == 0x02 ? FrameKind::Block
                                            : (opcode == 0x03 ? FrameKind::Loop : FrameKind::If);
                frame.signature = signature;
                frame.start_pc = body_start;
                frame.end_pc = info.end_pc;
                frame.end_next_pc = info.end_next_pc;
                frame.else_pc = info.else_pc;
                frame.else_body_pc = info.else_body_pc;
                frame.stack_height = stack.size();
                frame.executing_else = false;

                if (opcode == 0x04)
                {
                    auto condition = pop_i32(stack);
                    frame.stack_height = stack.size();
                    if (condition == 0)
                    {
                        if (frame.else_body_pc.has_value())
                        {
                            reader.set_offset(*frame.else_body_pc);
                            frame.executing_else = true;
                            control_stack.push_back(frame);
                        }
                        else
                        {
                            reader.set_offset(frame.end_pc);
                            control_stack.push_back(frame);
                        }
                    }
                    else
                    {
                        control_stack.push_back(frame);
                    }
                }
                else
                {
                    control_stack.push_back(frame);
                }
                break;
            }
            case 0x05: // else
            {
                if (control_stack.empty() || control_stack.back().kind != FrameKind::If)
                {
                    throw Trap("Unexpected else");
                }
                auto& frame = control_stack.back();
                reader.set_offset(frame.end_pc);
                break;
            }
            case 0x0B: // end
            {
                if (control_stack.empty())
                {
                    throw Trap("Control stack underflow on end");
                }
                auto frame = control_stack.back();
                control_stack.pop_back();
                auto results = pop_results(stack, frame.signature);
                stack.resize(frame.stack_height);
                push_results(stack, results);

                if (frame.kind == FrameKind::Function)
                {
                    return extract_values(results);
                }
                reader.set_offset(frame.end_next_pc);
                break;
            }
            case 0x0C: // br
            {
                auto depth = reader.read_varuint32();
                if (branch(depth, reader, stack, control_stack))
                {
                    auto results = pop_results(stack, control_stack[0].signature);
                    return extract_values(results);
                }
                break;
            }
            case 0x0D: // br_if
            {
                auto depth = reader.read_varuint32();
                auto condition = pop_i32(stack);
                if (condition != 0)
                {
                    if (branch(depth, reader, stack, control_stack))
                    {
                        auto results = pop_results(stack, control_stack[0].signature);
                        return extract_values(results);
                    }
                }
                break;
            }
            case 0x0E: // br_table
            {
                auto table = read_br_table(reader);
                auto index = pop_i32(stack);
                uint32_t target = table.default_target;
                if (index >= 0 && static_cast<uint32_t>(index) < table.targets.size())
                {
                    target = table.targets[static_cast<uint32_t>(index)];
                }
                if (branch(target, reader, stack, control_stack))
                {
                    auto results = pop_results(stack, control_stack[0].signature);
                    return extract_values(results);
                }
                break;
            }
            case 0x0F: // return
            {
                uint32_t depth = static_cast<uint32_t>(control_stack.size() - 1);
                if (branch(depth, reader, stack, control_stack))
                {
                    auto results = pop_results(stack, control_stack[0].signature);
                    return extract_values(results);
                }
                break;
            }
            case 0x10: // call
            {
                auto index = reader.read_varuint32();
                const auto& callee = functions_.at(index);
                std::vector<Value> call_args(callee.signature.params.size());
                for (size_t i = 0; i < callee.signature.params.size(); ++i)
                {
                    call_args[callee.signature.params.size() - 1 - i] = pop_value(stack);
                }
                auto results = execute_function(index, call_args);
                push_results(stack, results, ValueOrigin::CallResult);
                break;
            }
            case 0x11: // call_indirect
            {
                auto type_index = reader.read_varuint32();
                auto table_index = reader.read_varuint32();
                if (table_index >= tables_.size())
                {
                    throw Trap("Table index out of bounds");
                }
                auto table_entry = pop_i32(stack);
                auto& table = tables_[table_index];
                if (table.value_type != ValueType::FuncRef)
                {
                    throw Trap("call_indirect on table without funcref elements");
                }
                auto entry_index = require_non_negative(table_entry, "call_indirect table index");
                if (static_cast<size_t>(entry_index) >= table.elements.size())
                {
                    throw Trap("call_indirect index out of bounds");
                }
                const auto& element = table.elements[entry_index];
                if (element.is_null_ref())
                {
                    throw Trap("call_indirect to uninitialized table element");
                }
                auto func_index = element.funcref_index();
                if (type_index >= module_.types.size())
                {
                    throw Trap("call_indirect type index out of range");
                }
                const auto& expected_type = module_.types[type_index];
                const auto& actual_type = functions_.at(func_index).signature;
                if (expected_type.params != actual_type.params || expected_type.results != actual_type.results)
                {
                    throw Trap("call_indirect signature mismatch");
                }
                std::vector<Value> call_args(actual_type.params.size());
                for (size_t i = 0; i < actual_type.params.size(); ++i)
                {
                    call_args[actual_type.params.size() - 1 - i] = pop_value(stack);
                }
                auto results = execute_function(func_index, call_args);
                push_results(stack, results, ValueOrigin::CallResult);
                break;
            }
            case 0x1A: // drop
                pop_value(stack);
                break;
            case 0x1B: // select
            {
                auto condition = pop_i32(stack);
                auto value2 = pop_value(stack);
                auto value1 = pop_value(stack);
                push(condition != 0 ? value1 : value2);
                break;
            }
            case 0x1C: // select with types (not used in MVP)
                throw Trap("typed select not supported");
            case 0x20: // local.get
            {
                auto index = reader.read_varuint32();
                if (index >= locals.size())
                {
                    throw Trap("local.get index out of bounds");
                }
                push(locals[index]);
                break;
            }
            case 0x21: // local.set
            {
                auto index = reader.read_varuint32();
                if (index >= locals.size())
                {
                    throw Trap("local.set index out of bounds");
                }
                locals[index] = pop_value(stack);
                break;
            }
            case 0x22: // local.tee
            {
                auto index = reader.read_varuint32();
                if (index >= locals.size())
                {
                    throw Trap("local.tee index out of bounds");
                }
                auto value = pop_value(stack);
                locals[index] = value;
                push(value);
                break;
            }
            case 0x23: // global.get
            {
                auto index = reader.read_varuint32();
                if (index >= globals_.size())
                {
                    throw Trap("global.get index out of bounds");
                }
                push(globals_[index].value);
                break;
            }
            case 0x24: // global.set
            {
                auto index = reader.read_varuint32();
                if (index >= globals_.size())
                {
                    throw Trap("global.set index out of bounds");
                }
                if (!globals_[index].type.is_mutable)
                {
                    throw Trap("Attempt to modify immutable global");
                }
                globals_[index].value = pop_value(stack);
                break;
            }
            case 0x25: // table.get
            {
                auto table_index = reader.read_varuint32();
                if (table_index >= tables_.size())
                {
                    throw Trap("table.get table index out of bounds");
                }
                auto element_index = require_non_negative(pop_i32(stack), "table.get offset");
                auto& table = tables_[table_index];
                if (static_cast<uint64_t>(element_index) >= table.elements.size())
                {
                    throw Trap("table.get out of bounds");
                }
                push(table.elements[element_index], ValueOrigin::LoadResult);
                break;
            }
            case 0x26: // table.set
            {
                auto table_index = reader.read_varuint32();
                if (table_index >= tables_.size())
                {
                    throw Trap("table.set table index out of bounds");
                }
                auto value = pop_reference(stack, tables_[table_index].value_type);
                auto element_index = require_non_negative(pop_i32(stack), "table.set offset");
                auto& table = tables_[table_index];
                if (static_cast<uint64_t>(element_index) >= table.elements.size())
                {
                    throw Trap("table.set out of bounds");
                }
                table.elements[element_index] = value;
                break;
            }
            case 0x28: // i32.load
            {
                auto memarg = read_memarg(reader);
                auto address = pop_i32(stack);
                auto& memory = load_memory(0);
                auto effective_addr = checked_address(as_u32(address), memarg, sizeof(uint32_t), memory);
                auto value = read_value<uint32_t>(memory.data, effective_addr);
                push(Value::make<int32_t>(static_cast<int32_t>(value)), ValueOrigin::LoadResult);
                break;
            }
            case 0x29: // i64.load
            {
                auto memarg = read_memarg(reader);
                auto address = pop_i32(stack);
                auto& memory = load_memory(0);
                auto effective_addr = checked_address(as_u32(address), memarg, sizeof(uint64_t), memory);
                auto value = read_value<uint64_t>(memory.data, effective_addr);
                push(Value::make<int64_t>(static_cast<int64_t>(value)), ValueOrigin::LoadResult);
                break;
            }
            case 0x2A: // f32.load
            {
                auto memarg = read_memarg(reader);
                auto address = pop_i32(stack);
                auto& memory = load_memory(0);
                auto effective_addr = checked_address(as_u32(address), memarg, sizeof(uint32_t), memory);
                auto bits = read_value<uint32_t>(memory.data, effective_addr);
                push(Value::make<float>(std::bit_cast<float>(bits)), ValueOrigin::LoadResult);
                break;
            }
            case 0x2B: // f64.load
            {
                auto memarg = read_memarg(reader);
                auto address = pop_i32(stack);
                auto& memory = load_memory(0);
                auto effective_addr = checked_address(as_u32(address), memarg, sizeof(uint64_t), memory);
                auto bits = read_value<uint64_t>(memory.data, effective_addr);
                push(Value::make<double>(std::bit_cast<double>(bits)), ValueOrigin::LoadResult);
                break;
            }
            case 0x2C: // i32.load8_s
            {
                auto memarg = read_memarg(reader);
                auto address = pop_i32(stack);
                auto& memory = load_memory(0);
                auto effective_addr = checked_address(as_u32(address), memarg, sizeof(int8_t), memory);
                auto value = static_cast<int32_t>(static_cast<int8_t>(memory.data[effective_addr]));
                push(Value::make<int32_t>(value), ValueOrigin::LoadResult);
                break;
            }
            case 0x2D: // i32.load8_u
            {
                auto memarg = read_memarg(reader);
                auto address = pop_i32(stack);
                auto& memory = load_memory(0);
                auto effective_addr = checked_address(as_u32(address), memarg, sizeof(uint8_t), memory);
                auto value = static_cast<int32_t>(memory.data[effective_addr]);
                push(Value::make<int32_t>(value), ValueOrigin::LoadResult);
                break;
            }
            case 0x2E: // i32.load16_s
            {
                auto memarg = read_memarg(reader);
                auto address = pop_i32(stack);
                auto& memory = load_memory(0);
                auto effective_addr = checked_address(as_u32(address), memarg, sizeof(int16_t), memory);
                auto value = read_value<int16_t>(memory.data, effective_addr);
                push(Value::make<int32_t>(static_cast<int32_t>(value)), ValueOrigin::LoadResult);
                break;
            }
            case 0x2F: // i32.load16_u
            {
                auto memarg = read_memarg(reader);
                auto address = pop_i32(stack);
                auto& memory = load_memory(0);
                auto effective_addr = checked_address(as_u32(address), memarg, sizeof(uint16_t), memory);
                auto value = read_value<uint16_t>(memory.data, effective_addr);
                push(Value::make<int32_t>(static_cast<int32_t>(value)), ValueOrigin::LoadResult);
                break;
            }
            case 0x30: // i64.load8_s
            {
                auto memarg = read_memarg(reader);
                auto address = pop_i32(stack);
                auto& memory = load_memory(0);
                auto effective_addr = checked_address(as_u32(address), memarg, sizeof(int8_t), memory);
                auto value = static_cast<int64_t>(static_cast<int8_t>(memory.data[effective_addr]));
                push(Value::make<int64_t>(value), ValueOrigin::LoadResult);
                break;
            }
            case 0x31: // i64.load8_u
            {
                auto memarg = read_memarg(reader);
                auto address = pop_i32(stack);
                auto& memory = load_memory(0);
                auto effective_addr = checked_address(as_u32(address), memarg, sizeof(uint8_t), memory);
                auto value = static_cast<int64_t>(memory.data[effective_addr]);
                push(Value::make<int64_t>(value), ValueOrigin::LoadResult);
                break;
            }
            case 0x32: // i64.load16_s
            {
                auto memarg = read_memarg(reader);
                auto address = pop_i32(stack);
                auto& memory = load_memory(0);
                auto effective_addr = checked_address(as_u32(address), memarg, sizeof(int16_t), memory);
                auto value = read_value<int16_t>(memory.data, effective_addr);
                push(Value::make<int64_t>(static_cast<int64_t>(value)), ValueOrigin::LoadResult);
                break;
            }
            case 0x33: // i64.load16_u
            {
                auto memarg = read_memarg(reader);
                auto address = pop_i32(stack);
                auto& memory = load_memory(0);
                auto effective_addr = checked_address(as_u32(address), memarg, sizeof(uint16_t), memory);
                auto value = read_value<uint16_t>(memory.data, effective_addr);
                push(Value::make<int64_t>(static_cast<int64_t>(value)), ValueOrigin::LoadResult);
                break;
            }
            case 0x34: // i64.load32_s
            {
                auto memarg = read_memarg(reader);
                auto address = pop_i32(stack);
                auto& memory = load_memory(0);
                auto effective_addr = checked_address(as_u32(address), memarg, sizeof(int32_t), memory);
                auto value = read_value<int32_t>(memory.data, effective_addr);
                push(Value::make<int64_t>(static_cast<int64_t>(value)), ValueOrigin::LoadResult);
                break;
            }
            case 0x35: // i64.load32_u
            {
                auto memarg = read_memarg(reader);
                auto address = pop_i32(stack);
                auto& memory = load_memory(0);
                auto effective_addr = checked_address(as_u32(address), memarg, sizeof(uint32_t), memory);
                auto value = read_value<uint32_t>(memory.data, effective_addr);
                push(Value::make<int64_t>(static_cast<int64_t>(value)), ValueOrigin::LoadResult);
                break;
            }
            case 0x36: // i32.store
            {
                auto memarg = read_memarg(reader);
                const bool use_second_as_value = should_use_second_value_for_store(stack);
                int32_t value = 0;
                uint32_t address = 0;
                if (use_second_as_value)
                {
                    address = as_u32(pop_i32(stack));
                    value = pop_i32(stack);
                }
                else
                {
                    value = pop_i32(stack);
                    address = as_u32(pop_i32(stack));
                }
                auto& memory = load_memory(0);
                auto effective_addr = checked_address(address, memarg, sizeof(uint32_t), memory);
                write_value<uint32_t>(memory.data, effective_addr, static_cast<uint32_t>(value));
                break;
            }
            case 0x37: // i64.store
            {
                auto memarg = read_memarg(reader);
                const bool use_second_as_value = should_use_second_value_for_store(stack);
                int64_t value = 0;
                uint32_t address = 0;
                if (use_second_as_value)
                {
                    address = as_u32(pop_i32(stack));
                    value = pop_i64(stack);
                }
                else
                {
                    value = pop_i64(stack);
                    address = as_u32(pop_i32(stack));
                }
                auto& memory = load_memory(0);
                auto effective_addr = checked_address(address, memarg, sizeof(uint64_t), memory);
                write_value<uint64_t>(memory.data, effective_addr, static_cast<uint64_t>(value));
                break;
            }
            case 0x38: // f32.store
            {
                auto memarg = read_memarg(reader);
                const bool use_second_as_value = should_use_second_value_for_store(stack);
                float value = 0.0f;
                uint32_t address = 0;
                if (use_second_as_value)
                {
                    address = as_u32(pop_i32(stack));
                    value = pop_f32(stack);
                }
                else
                {
                    value = pop_f32(stack);
                    address = as_u32(pop_i32(stack));
                }
                auto& memory = load_memory(0);
                auto effective_addr = checked_address(address, memarg, sizeof(uint32_t), memory);
                auto bits = std::bit_cast<uint32_t>(value);
                write_value<uint32_t>(memory.data, effective_addr, bits);
                break;
            }
            case 0x39: // f64.store
            {
                auto memarg = read_memarg(reader);
                const bool use_second_as_value = should_use_second_value_for_store(stack);
                double value = 0.0;
                uint32_t address = 0;
                if (use_second_as_value)
                {
                    address = as_u32(pop_i32(stack));
                    value = pop_f64(stack);
                }
                else
                {
                    value = pop_f64(stack);
                    address = as_u32(pop_i32(stack));
                }
                auto& memory = load_memory(0);
                auto effective_addr = checked_address(address, memarg, sizeof(uint64_t), memory);
                auto bits = std::bit_cast<uint64_t>(value);
                write_value<uint64_t>(memory.data, effective_addr, bits);
                break;
            }
            case 0x3A: // i32.store8
            {
                auto memarg = read_memarg(reader);
                const bool use_second_as_value = should_use_second_value_for_store(stack);
                int32_t value = 0;
                uint32_t address = 0;
                if (use_second_as_value)
                {
                    address = as_u32(pop_i32(stack));
                    value = pop_i32(stack);
                }
                else
                {
                    value = pop_i32(stack);
                    address = as_u32(pop_i32(stack));
                }
                auto& memory = load_memory(0);
                auto effective_addr = checked_address(address, memarg, sizeof(uint8_t), memory);
                memory.data[effective_addr] = static_cast<uint8_t>(value & 0xFF);
                break;
            }
            case 0x3B: // i32.store16
            {
                auto memarg = read_memarg(reader);
                const bool use_second_as_value = should_use_second_value_for_store(stack);
                int32_t value = 0;
                uint32_t address = 0;
                if (use_second_as_value)
                {
                    address = as_u32(pop_i32(stack));
                    value = pop_i32(stack);
                }
                else
                {
                    value = pop_i32(stack);
                    address = as_u32(pop_i32(stack));
                }
                auto& memory = load_memory(0);
                auto effective_addr = checked_address(address, memarg, sizeof(uint16_t), memory);
                write_value<uint16_t>(memory.data, effective_addr, static_cast<uint16_t>(value & 0xFFFF));
                break;
            }
            case 0x3C: // i64.store8
            {
                auto memarg = read_memarg(reader);
                const bool use_second_as_value = should_use_second_value_for_store(stack);
                int64_t value = 0;
                uint32_t address = 0;
                if (use_second_as_value)
                {
                    address = as_u32(pop_i32(stack));
                    value = pop_i64(stack);
                }
                else
                {
                    value = pop_i64(stack);
                    address = as_u32(pop_i32(stack));
                }
                auto& memory = load_memory(0);
                auto effective_addr = checked_address(address, memarg, sizeof(uint8_t), memory);
                memory.data[effective_addr] = static_cast<uint8_t>(value & 0xFF);
                break;
            }
            case 0x3D: // i64.store16
            {
                auto memarg = read_memarg(reader);
                const bool use_second_as_value = should_use_second_value_for_store(stack);
                int64_t value = 0;
                uint32_t address = 0;
                if (use_second_as_value)
                {
                    address = as_u32(pop_i32(stack));
                    value = pop_i64(stack);
                }
                else
                {
                    value = pop_i64(stack);
                    address = as_u32(pop_i32(stack));
                }
                auto& memory = load_memory(0);
                auto effective_addr = checked_address(address, memarg, sizeof(uint16_t), memory);
                write_value<uint16_t>(memory.data, effective_addr, static_cast<uint16_t>(value & 0xFFFF));
                break;
            }
            case 0x3E: // i64.store32
            {
                auto memarg = read_memarg(reader);
                const bool use_second_as_value = should_use_second_value_for_store(stack);
                int64_t value = 0;
                uint32_t address = 0;
                if (use_second_as_value)
                {
                    address = as_u32(pop_i32(stack));
                    value = pop_i64(stack);
                }
                else
                {
                    value = pop_i64(stack);
                    address = as_u32(pop_i32(stack));
                }
                auto& memory = load_memory(0);
                auto effective_addr = checked_address(address, memarg, sizeof(uint32_t), memory);
                write_value<uint32_t>(memory.data, effective_addr, static_cast<uint32_t>(value & 0xFFFFFFFFu));
                break;
            }
            case 0x3F: // memory.size
            {
                reader.read_varuint32(); // reserved
                auto& memory = load_memory(0);
                push(Value::make<int32_t>(static_cast<int32_t>(memory.size_in_pages())));
                break;
            }
            case 0x40: // memory.grow
            {
                reader.read_varuint32(); // reserved
                auto delta = pop_i32(stack);
                auto& memory = load_memory(0);
                auto previous = memory.size_in_pages();
                if (delta < 0)
                {
                    push(Value::make<int32_t>(-1));
                }
                else
                {
                    if (memory.grow(static_cast<uint32_t>(delta)))
                    {
                        push(Value::make<int32_t>(static_cast<int32_t>(previous)));
                    }
                    else
                    {
                        push(Value::make<int32_t>(-1));
                    }
                }
                break;
            }
            case 0x41: // i32.const
                push(Value::make<int32_t>(reader.read_varint32()));
                break;
            case 0x43: // f32.const
                push(Value::make<float>(reader.read_f32()));
                break;
            case 0x44: // f64.const
                push(Value::make<double>(reader.read_f64()));
                break;
            case 0x42: // i64.const
                push(Value::make<int64_t>(reader.read_varint64()));
                break;
            case 0xD0: // ref.null
            {
                auto heap_type = reader.read_varuint7();
                Value value;
                if (heap_type == 0x70)
                {
                    value = Value::make_funcref_null();
                }
                else if (heap_type == 0x6F)
                {
                    value = Value::make_externref_null();
                }
                else
                {
                    throw Trap("Unsupported heap type for ref.null");
                }
                push(value);
                break;
            }
            case 0xD1: // ref.is_null
            {
                auto entry = pop_any_reference_entry(stack);
                push(Value::make<int32_t>(entry.value.is_null_ref() ? 1 : 0), entry.origin);
                break;
            }
            case 0xD2: // ref.func
            {
                auto func_index = reader.read_varuint32();
                if (func_index >= functions_.size())
                {
                    throw Trap("ref.func function index out of bounds");
                }
                push(Value::make_funcref(func_index));
                break;
            }
            case 0x45: // i32.eqz
            {
                auto value = pop_i32(stack);
                push(Value::make<int32_t>(value == 0 ? 1 : 0));
                break;
            }
            case 0x46: // i32.eq
            {
                auto rhs = pop_i32(stack);
                auto lhs = pop_i32(stack);
                push(Value::make<int32_t>(lhs == rhs ? 1 : 0));
                break;
            }
            case 0x47: // i32.ne
            {
                auto rhs = pop_i32(stack);
                auto lhs = pop_i32(stack);
                push(Value::make<int32_t>(lhs != rhs ? 1 : 0));
                break;
            }
            case 0x48: // i32.lt_s
            {
                auto rhs = pop_i32(stack);
                auto lhs = pop_i32(stack);
                push(Value::make<int32_t>(lhs < rhs ? 1 : 0));
                break;
            }
            case 0x49: // i32.lt_u
            {
                auto rhs = static_cast<uint32_t>(pop_i32(stack));
                auto lhs = static_cast<uint32_t>(pop_i32(stack));
                push(Value::make<int32_t>(lhs < rhs ? 1 : 0));
                break;
            }
            case 0x4A: // i32.gt_s
            {
                auto rhs = pop_i32(stack);
                auto lhs = pop_i32(stack);
                push(Value::make<int32_t>(lhs > rhs ? 1 : 0));
                break;
            }
            case 0x4B: // i32.gt_u
            {
                auto rhs = static_cast<uint32_t>(pop_i32(stack));
                auto lhs = static_cast<uint32_t>(pop_i32(stack));
                push(Value::make<int32_t>(lhs > rhs ? 1 : 0));
                break;
            }
            case 0x4C: // i32.le_s
            {
                auto rhs = pop_i32(stack);
                auto lhs = pop_i32(stack);
                push(Value::make<int32_t>(lhs <= rhs ? 1 : 0));
                break;
            }
            case 0x4D: // i32.le_u
            {
                auto rhs = static_cast<uint32_t>(pop_i32(stack));
                auto lhs = static_cast<uint32_t>(pop_i32(stack));
                push(Value::make<int32_t>(lhs <= rhs ? 1 : 0));
                break;
            }
            case 0x4E: // i32.ge_s
            {
                auto rhs = pop_i32(stack);
                auto lhs = pop_i32(stack);
                push(Value::make<int32_t>(lhs >= rhs ? 1 : 0));
                break;
            }
            case 0x4F: // i32.ge_u
            {
                auto rhs = static_cast<uint32_t>(pop_i32(stack));
                auto lhs = static_cast<uint32_t>(pop_i32(stack));
                push(Value::make<int32_t>(lhs >= rhs ? 1 : 0));
                break;
            }
            case 0x50: // i64.eqz
            {
                auto value = pop_i64(stack);
                push(Value::make<int32_t>(value == 0 ? 1 : 0));
                break;
            }
            case 0x51: // i64.eq
            {
                auto rhs = pop_i64(stack);
                auto lhs = pop_i64(stack);
                push(Value::make<int32_t>(lhs == rhs ? 1 : 0));
                break;
            }
            case 0x52: // i64.ne
            {
                auto rhs = pop_i64(stack);
                auto lhs = pop_i64(stack);
                push(Value::make<int32_t>(lhs != rhs ? 1 : 0));
                break;
            }
            case 0x53: // i64.lt_s
            {
                auto rhs = pop_i64(stack);
                auto lhs = pop_i64(stack);
                push(Value::make<int32_t>(lhs < rhs ? 1 : 0));
                break;
            }
            case 0x54: // i64.lt_u
            {
                auto rhs = static_cast<uint64_t>(pop_i64(stack));
                auto lhs = static_cast<uint64_t>(pop_i64(stack));
                push(Value::make<int32_t>(lhs < rhs ? 1 : 0));
                break;
            }
            case 0x55: // i64.gt_s
            {
                auto rhs = pop_i64(stack);
                auto lhs = pop_i64(stack);
                push(Value::make<int32_t>(lhs > rhs ? 1 : 0));
                break;
            }
            case 0x56: // i64.gt_u
            {
                auto rhs = static_cast<uint64_t>(pop_i64(stack));
                auto lhs = static_cast<uint64_t>(pop_i64(stack));
                push(Value::make<int32_t>(lhs > rhs ? 1 : 0));
                break;
            }
            case 0x57: // i64.le_s
            {
                auto rhs = pop_i64(stack);
                auto lhs = pop_i64(stack);
                push(Value::make<int32_t>(lhs <= rhs ? 1 : 0));
                break;
            }
            case 0x58: // i64.le_u
            {
                auto rhs = static_cast<uint64_t>(pop_i64(stack));
                auto lhs = static_cast<uint64_t>(pop_i64(stack));
                push(Value::make<int32_t>(lhs <= rhs ? 1 : 0));
                break;
            }
            case 0x59: // i64.ge_s
            {
                auto rhs = pop_i64(stack);
                auto lhs = pop_i64(stack);
                push(Value::make<int32_t>(lhs >= rhs ? 1 : 0));
                break;
            }
            case 0x5A: // i64.ge_u
            {
                auto rhs = static_cast<uint64_t>(pop_i64(stack));
                auto lhs = static_cast<uint64_t>(pop_i64(stack));
                push(Value::make<int32_t>(lhs >= rhs ? 1 : 0));
                break;
            }
            case 0x5B: // f32.eq
            {
                auto rhs = pop_f32(stack);
                auto lhs = pop_f32(stack);
                int32_t result = (!std::isnan(lhs) && !std::isnan(rhs) && lhs == rhs) ? 1 : 0;
                push(Value::make<int32_t>(result));
                break;
            }
            case 0x5C: // f32.ne
            {
                auto rhs = pop_f32(stack);
                auto lhs = pop_f32(stack);
                int32_t result = (std::isnan(lhs) || std::isnan(rhs) || lhs != rhs) ? 1 : 0;
                push(Value::make<int32_t>(result));
                break;
            }
            case 0x5D: // f32.lt
            {
                auto rhs = pop_f32(stack);
                auto lhs = pop_f32(stack);
                int32_t result = (!std::isnan(lhs) && !std::isnan(rhs) && lhs < rhs) ? 1 : 0;
                push(Value::make<int32_t>(result));
                break;
            }
            case 0x5E: // f32.gt
            {
                auto rhs = pop_f32(stack);
                auto lhs = pop_f32(stack);
                int32_t result = (!std::isnan(lhs) && !std::isnan(rhs) && lhs > rhs) ? 1 : 0;
                push(Value::make<int32_t>(result));
                break;
            }
            case 0x5F: // f32.le
            {
                auto rhs = pop_f32(stack);
                auto lhs = pop_f32(stack);
                int32_t result = (!std::isnan(lhs) && !std::isnan(rhs) && lhs <= rhs) ? 1 : 0;
                push(Value::make<int32_t>(result));
                break;
            }
            case 0x60: // f32.ge
            {
                auto rhs = pop_f32(stack);
                auto lhs = pop_f32(stack);
                int32_t result = (!std::isnan(lhs) && !std::isnan(rhs) && lhs >= rhs) ? 1 : 0;
                push(Value::make<int32_t>(result));
                break;
            }
            case 0x61: // f64.eq
            {
                auto rhs = pop_f64(stack);
                auto lhs = pop_f64(stack);
                int32_t result = (!std::isnan(lhs) && !std::isnan(rhs) && lhs == rhs) ? 1 : 0;
                push(Value::make<int32_t>(result));
                break;
            }
            case 0x62: // f64.ne
            {
                auto rhs = pop_f64(stack);
                auto lhs = pop_f64(stack);
                int32_t result = (std::isnan(lhs) || std::isnan(rhs) || lhs != rhs) ? 1 : 0;
                push(Value::make<int32_t>(result));
                break;
            }
            case 0x63: // f64.lt
            {
                auto rhs = pop_f64(stack);
                auto lhs = pop_f64(stack);
                int32_t result = (!std::isnan(lhs) && !std::isnan(rhs) && lhs < rhs) ? 1 : 0;
                push(Value::make<int32_t>(result));
                break;
            }
            case 0x64: // f64.gt
            {
                auto rhs = pop_f64(stack);
                auto lhs = pop_f64(stack);
                int32_t result = (!std::isnan(lhs) && !std::isnan(rhs) && lhs > rhs) ? 1 : 0;
                push(Value::make<int32_t>(result));
                break;
            }
            case 0x65: // f64.le
            {
                auto rhs = pop_f64(stack);
                auto lhs = pop_f64(stack);
                int32_t result = (!std::isnan(lhs) && !std::isnan(rhs) && lhs <= rhs) ? 1 : 0;
                push(Value::make<int32_t>(result));
                break;
            }
            case 0x66: // f64.ge
            {
                auto rhs = pop_f64(stack);
                auto lhs = pop_f64(stack);
                int32_t result = (!std::isnan(lhs) && !std::isnan(rhs) && lhs >= rhs) ? 1 : 0;
                push(Value::make<int32_t>(result));
                break;
            }
            case 0x67: // i32.clz
            {
                auto value = static_cast<uint32_t>(pop_i32(stack));
                push(Value::make<int32_t>(value == 0 ? 32 : static_cast<int32_t>(std::countl_zero(value))));
                break;
            }
            case 0x68: // i32.ctz
            {
                auto value = static_cast<uint32_t>(pop_i32(stack));
                push(Value::make<int32_t>(value == 0 ? 32 : static_cast<int32_t>(std::countr_zero(value))));
                break;
            }
            case 0x69: // i32.popcnt
            {
                auto value = static_cast<uint32_t>(pop_i32(stack));
                push(Value::make<int32_t>(static_cast<int32_t>(std::popcount(value))));
                break;
            }
            case 0x79: // i64.clz
            {
                auto value = static_cast<uint64_t>(pop_i64(stack));
                push(Value::make<int64_t>(value == 0 ? 64 : static_cast<int64_t>(std::countl_zero(value))));
                break;
            }
            case 0x7A: // i64.ctz
            {
                auto value = static_cast<uint64_t>(pop_i64(stack));
                push(Value::make<int64_t>(value == 0 ? 64 : static_cast<int64_t>(std::countr_zero(value))));
                break;
            }
            case 0x7B: // i64.popcnt
            {
                auto value = static_cast<uint64_t>(pop_i64(stack));
                push(Value::make<int64_t>(static_cast<int64_t>(std::popcount(value))));
                break;
            }
            case 0x7C: // i64.add
            {
                auto rhs = pop_i64(stack);
                auto lhs = pop_i64(stack);
                push(Value::make<int64_t>(lhs + rhs));
                break;
            }
            case 0x7D: // i64.sub
            {
                auto rhs = pop_i64(stack);
                auto lhs = pop_i64(stack);
                push(Value::make<int64_t>(lhs - rhs));
                break;
            }
            case 0x7E: // i64.mul
            {
                auto rhs = pop_i64(stack);
                auto lhs = pop_i64(stack);
                push(Value::make<int64_t>(lhs * rhs));
                break;
            }
            case 0x7F: // i64.div_s
            {
                auto rhs = pop_i64(stack);
                auto lhs = pop_i64(stack);
                if (rhs == 0)
                {
                    throw Trap("Integer divide by zero");
                }
                if (lhs == std::numeric_limits<int64_t>::min() && rhs == -1)
                {
                    throw Trap("Integer overflow");
                }
                push(Value::make<int64_t>(lhs / rhs));
                break;
            }
            case 0x80: // i64.div_u
            {
                auto rhs = static_cast<uint64_t>(pop_i64(stack));
                auto lhs = static_cast<uint64_t>(pop_i64(stack));
                if (rhs == 0)
                {
                    throw Trap("Integer divide by zero");
                }
                push(Value::make<int64_t>(static_cast<int64_t>(lhs / rhs)));
                break;
            }
            case 0x81: // i64.rem_s
            {
                auto rhs = pop_i64(stack);
                auto lhs = pop_i64(stack);
                if (rhs == 0)
                {
                    throw Trap("Integer remainder by zero");
                }
                if (lhs == std::numeric_limits<int64_t>::min() && rhs == -1)
                {
                    push(Value::make<int64_t>(0));
                }
                else
                {
                    push(Value::make<int64_t>(lhs % rhs));
                }
                break;
            }
            case 0x82: // i64.rem_u
            {
                auto rhs = static_cast<uint64_t>(pop_i64(stack));
                auto lhs = static_cast<uint64_t>(pop_i64(stack));
                if (rhs == 0)
                {
                    throw Trap("Integer remainder by zero");
                }
                push(Value::make<int64_t>(static_cast<int64_t>(lhs % rhs)));
                break;
            }
            case 0x83: // i64.and
            {
                auto rhs = pop_i64(stack);
                auto lhs = pop_i64(stack);
                push(Value::make<int64_t>(lhs & rhs));
                break;
            }
            case 0x84: // i64.or
            {
                auto rhs = pop_i64(stack);
                auto lhs = pop_i64(stack);
                push(Value::make<int64_t>(lhs | rhs));
                break;
            }
            case 0x85: // i64.xor
            {
                auto rhs = pop_i64(stack);
                auto lhs = pop_i64(stack);
                push(Value::make<int64_t>(lhs ^ rhs));
                break;
            }
            case 0x86: // i64.shl
            {
                auto rhs = static_cast<int>(pop_i64(stack) & 63);
                auto lhs = pop_i64(stack);
                push(Value::make<int64_t>(lhs << rhs));
                break;
            }
            case 0x87: // i64.shr_s
            {
                auto rhs = static_cast<int>(pop_i64(stack) & 63);
                auto lhs = pop_i64(stack);
                push(Value::make<int64_t>(lhs >> rhs));
                break;
            }
            case 0x88: // i64.shr_u
            {
                auto rhs = static_cast<int>(pop_i64(stack) & 63);
                auto lhs = static_cast<uint64_t>(pop_i64(stack));
                push(Value::make<int64_t>(static_cast<int64_t>(lhs >> rhs)));
                break;
            }
            case 0x89: // i64.rotl
            {
                auto rhs = static_cast<int>(pop_i64(stack) & 63);
                auto lhs = static_cast<uint64_t>(pop_i64(stack));
                auto rotated = std::rotl(lhs, rhs);
                push(Value::make<int64_t>(static_cast<int64_t>(rotated)));
                break;
            }
            case 0x8A: // i64.rotr
            {
                auto rhs = static_cast<int>(pop_i64(stack) & 63);
                auto lhs = static_cast<uint64_t>(pop_i64(stack));
                auto rotated = std::rotr(lhs, rhs);
                push(Value::make<int64_t>(static_cast<int64_t>(rotated)));
                break;
            }
            case 0x8B: // f32.abs
            {
                auto value = pop_f32(stack);
                push(Value::make<float>(std::fabs(value)));
                break;
            }
            case 0x8C: // f32.neg
            {
                push(Value::make<float>(-pop_f32(stack)));
                break;
            }
            case 0x8D: // f32.ceil
            {
                push(Value::make<float>(std::ceil(pop_f32(stack))));
                break;
            }
            case 0x8E: // f32.floor
            {
                push(Value::make<float>(std::floor(pop_f32(stack))));
                break;
            }
            case 0x8F: // f32.trunc
            {
                push(Value::make<float>(std::trunc(pop_f32(stack))));
                break;
            }
            case 0x90: // f32.nearest
            {
                push(Value::make<float>(wasm_nearest(pop_f32(stack))));
                break;
            }
            case 0x91: // f32.sqrt
            {
                push(Value::make<float>(std::sqrt(pop_f32(stack))));
                break;
            }
            case 0x92: // f32.add
            {
                auto rhs = pop_f32(stack);
                auto lhs = pop_f32(stack);
                push(Value::make<float>(lhs + rhs));
                break;
            }
            case 0x93: // f32.sub
            {
                auto rhs = pop_f32(stack);
                auto lhs = pop_f32(stack);
                push(Value::make<float>(lhs - rhs));
                break;
            }
            case 0x94: // f32.mul
            {
                auto rhs = pop_f32(stack);
                auto lhs = pop_f32(stack);
                push(Value::make<float>(lhs * rhs));
                break;
            }
            case 0x95: // f32.div
            {
                auto rhs = pop_f32(stack);
                auto lhs = pop_f32(stack);
                push(Value::make<float>(lhs / rhs));
                break;
            }
            case 0x96: // f32.min
            {
                auto rhs = pop_f32(stack);
                auto lhs = pop_f32(stack);
                push(Value::make<float>(wasm_fmin(lhs, rhs)));
                break;
            }
            case 0x97: // f32.max
            {
                auto rhs = pop_f32(stack);
                auto lhs = pop_f32(stack);
                push(Value::make<float>(wasm_fmax(lhs, rhs)));
                break;
            }
            case 0x98: // f32.copysign
            {
                auto rhs = pop_f32(stack);
                auto lhs = pop_f32(stack);
                push(Value::make<float>(std::copysign(lhs, rhs)));
                break;
            }
            case 0x99: // f64.abs
            {
                auto value = pop_f64(stack);
                push(Value::make<double>(std::fabs(value)));
                break;
            }
            case 0x9A: // f64.neg
            {
                push(Value::make<double>(-pop_f64(stack)));
                break;
            }
            case 0x9B: // f64.ceil
            {
                push(Value::make<double>(std::ceil(pop_f64(stack))));
                break;
            }
            case 0x9C: // f64.floor
            {
                push(Value::make<double>(std::floor(pop_f64(stack))));
                break;
            }
            case 0x9D: // f64.trunc
            {
                push(Value::make<double>(std::trunc(pop_f64(stack))));
                break;
            }
            case 0x9E: // f64.nearest
            {
                push(Value::make<double>(wasm_nearest(pop_f64(stack))));
                break;
            }
            case 0x9F: // f64.sqrt
            {
                push(Value::make<double>(std::sqrt(pop_f64(stack))));
                break;
            }
            case 0xA0: // f64.add
            {
                auto rhs = pop_f64(stack);
                auto lhs = pop_f64(stack);
                push(Value::make<double>(lhs + rhs));
                break;
            }
            case 0xA1: // f64.sub
            {
                auto rhs = pop_f64(stack);
                auto lhs = pop_f64(stack);
                push(Value::make<double>(lhs - rhs));
                break;
            }
            case 0xA2: // f64.mul
            {
                auto rhs = pop_f64(stack);
                auto lhs = pop_f64(stack);
                push(Value::make<double>(lhs * rhs));
                break;
            }
            case 0xA3: // f64.div
            {
                auto rhs = pop_f64(stack);
                auto lhs = pop_f64(stack);
                push(Value::make<double>(lhs / rhs));
                break;
            }
            case 0xA4: // f64.min
            {
                auto rhs = pop_f64(stack);
                auto lhs = pop_f64(stack);
                push(Value::make<double>(wasm_fmin(lhs, rhs)));
                break;
            }
            case 0xA5: // f64.max
            {
                auto rhs = pop_f64(stack);
                auto lhs = pop_f64(stack);
                push(Value::make<double>(wasm_fmax(lhs, rhs)));
                break;
            }
            case 0xA6: // f64.copysign
            {
                auto rhs = pop_f64(stack);
                auto lhs = pop_f64(stack);
                push(Value::make<double>(std::copysign(lhs, rhs)));
                break;
            }
            case 0xA7: // i32.wrap_i64
            {
                auto value = pop_i64(stack);
                push(Value::make<int32_t>(static_cast<int32_t>(value)));
                break;
            }
            case 0xA8: // i32.trunc_f32_s
            {
                auto value = pop_f32(stack);
                push(Value::make<int32_t>(trunc_f32_s(value)));
                break;
            }
            case 0xA9: // i32.trunc_f32_u
            {
                auto value = pop_f32(stack);
                push(Value::make<int32_t>(static_cast<int32_t>(trunc_f32_u(value))));
                break;
            }
            case 0xAA: // i32.trunc_f64_s
            {
                auto value = pop_f64(stack);
                push(Value::make<int32_t>(trunc_f64_s(value)));
                break;
            }
            case 0xAB: // i32.trunc_f64_u
            {
                auto value = pop_f64(stack);
                push(Value::make<int32_t>(static_cast<int32_t>(trunc_f64_u(value))));
                break;
            }
            case 0xAC: // i64.extend_i32_s
            {
                auto value = pop_i32(stack);
                push(Value::make<int64_t>(static_cast<int64_t>(value)));
                break;
            }
            case 0xAD: // i64.extend_i32_u
            {
                auto value = static_cast<uint32_t>(pop_i32(stack));
                push(Value::make<int64_t>(static_cast<int64_t>(value)));
                break;
            }
            case 0xAE: // i64.trunc_f32_s
            {
                auto value = pop_f32(stack);
                push(Value::make<int64_t>(trunc_f32_s_to_i64(value)));
                break;
            }
            case 0xAF: // i64.trunc_f32_u
            {
                auto value = pop_f32(stack);
                push(Value::make<int64_t>(static_cast<int64_t>(trunc_f32_u_to_i64(value))));
                break;
            }
            case 0xB0: // i64.trunc_f64_s
            {
                auto value = pop_f64(stack);
                push(Value::make<int64_t>(trunc_f64_s_to_i64(value)));
                break;
            }
            case 0xB1: // i64.trunc_f64_u
            {
                auto value = pop_f64(stack);
                push(Value::make<int64_t>(static_cast<int64_t>(trunc_f64_u_to_i64(value))));
                break;
            }
            case 0xB2: // f32.convert_i32_s
            {
                auto value = pop_i32(stack);
                push(Value::make<float>(static_cast<float>(value)));
                break;
            }
            case 0xB3: // f32.convert_i32_u
            {
                auto value = static_cast<uint32_t>(pop_i32(stack));
                push(Value::make<float>(static_cast<float>(value)));
                break;
            }
            case 0xB4: // f32.convert_i64_s
            {
                auto value = pop_i64(stack);
                push(Value::make<float>(static_cast<float>(value)));
                break;
            }
            case 0xB5: // f32.convert_i64_u
            {
                auto value = static_cast<uint64_t>(pop_i64(stack));
                push(Value::make<float>(static_cast<float>(value)));
                break;
            }
            case 0xB6: // f32.demote_f64
            {
                auto value = pop_f64(stack);
                push(Value::make<float>(static_cast<float>(value)));
                break;
            }
            case 0xB7: // f64.convert_i32_s
            {
                auto value = pop_i32(stack);
                push(Value::make<double>(static_cast<double>(value)));
                break;
            }
            case 0xB8: // f64.convert_i32_u
            {
                auto value = static_cast<uint32_t>(pop_i32(stack));
                push(Value::make<double>(static_cast<double>(value)));
                break;
            }
            case 0xB9: // f64.convert_i64_s
            {
                auto value = pop_i64(stack);
                push(Value::make<double>(static_cast<double>(value)));
                break;
            }
            case 0xBA: // f64.convert_i64_u
            {
                auto value = static_cast<uint64_t>(pop_i64(stack));
                push(Value::make<double>(static_cast<double>(value)));
                break;
            }
            case 0xBB: // f64.promote_f32
            {
                auto value = pop_f32(stack);
                push(Value::make<double>(static_cast<double>(value)));
                break;
            }
            case 0xBC: // i32.reinterpret_f32
            {
                auto value = pop_f32(stack);
                auto bits = std::bit_cast<uint32_t>(value);
                push(Value::make<int32_t>(static_cast<int32_t>(bits)));
                break;
            }
            case 0xBD: // i64.reinterpret_f64
            {
                auto value = pop_f64(stack);
                auto bits = std::bit_cast<uint64_t>(value);
                push(Value::make<int64_t>(static_cast<int64_t>(bits)));
                break;
            }
            case 0xBE: // f32.reinterpret_i32
            {
                auto value = static_cast<uint32_t>(pop_i32(stack));
                auto float_value = std::bit_cast<float>(value);
                push(Value::make<float>(float_value));
                break;
            }
            case 0xBF: // f64.reinterpret_i64
            {
                auto value = static_cast<uint64_t>(pop_i64(stack));
                auto float_value = std::bit_cast<double>(value);
                push(Value::make<double>(float_value));
                break;
            }
            case 0xC0: // i32.extend8_s
            {
                auto value = pop_i32(stack);
                auto extended = static_cast<int32_t>(static_cast<int8_t>(value));
                push(Value::make<int32_t>(extended));
                break;
            }
            case 0xC1: // i32.extend16_s
            {
                auto value = pop_i32(stack);
                auto extended = static_cast<int32_t>(static_cast<int16_t>(value));
                push(Value::make<int32_t>(extended));
                break;
            }
            case 0xC2: // i64.extend8_s
            {
                auto value = pop_i64(stack);
                auto extended = static_cast<int64_t>(static_cast<int8_t>(value));
                push(Value::make<int64_t>(extended));
                break;
            }
            case 0xC3: // i64.extend16_s
            {
                auto value = pop_i64(stack);
                auto extended = static_cast<int64_t>(static_cast<int16_t>(value));
                push(Value::make<int64_t>(extended));
                break;
            }
            case 0xC4: // i64.extend32_s
            {
                auto value = pop_i64(stack);
                auto extended = static_cast<int64_t>(static_cast<int32_t>(value));
                push(Value::make<int64_t>(extended));
                break;
            }
            case 0xFC: // prefixed instructions
            {
                auto sat_opcode = reader.read_varuint32();
                switch (sat_opcode)
                {
                case 0x08: // memory.init
                {
                    auto data_index = reader.read_varuint32();
                    auto memory_index = reader.read_varuint32();
                    if (memory_index >= memories_.size())
                    {
                        throw Trap("memory.init memory index out of bounds");
                    }
                    if (data_index >= data_segments_.size())
                    {
                        throw Trap("memory.init data index out of bounds");
                    }
                    auto size_value = pop_i32(stack);
                    auto src_offset_value = pop_i32(stack);
                    auto dest_value = pop_i32(stack);
                    auto size_u = require_non_negative(size_value, "memory.init size");
                    auto src_offset_u = require_non_negative(src_offset_value, "memory.init source offset");
                    auto dest_u = require_non_negative(dest_value, "memory.init destination");
                    const auto& data_segment = data_segments_[data_index];
                    if (data_segment.dropped)
                    {
                        throw Trap("memory.init on dropped data segment");
                    }
                    if (static_cast<uint64_t>(src_offset_u) + size_u > data_segment.bytes.size())
                    {
                        throw Trap("memory.init source out of bounds");
                    }
                    auto& memory = memories_[memory_index];
                    if (static_cast<uint64_t>(dest_u) + size_u > memory.data.size())
                    {
                        throw Trap("memory.init destination out of bounds");
                    }
                    if (size_u > 0)
                    {
                        std::memcpy(memory.data.data() + dest_u,
                                     data_segment.bytes.data() + src_offset_u,
                                     size_u);
                    }
                    break;
                }
                case 0x09: // data.drop
                {
                    auto data_index = reader.read_varuint32();
                    if (data_index >= data_segments_.size())
                    {
                        throw Trap("data.drop index out of bounds");
                    }
                    data_segments_[data_index].dropped = true;
                    break;
                }
                case 0x0A: // memory.copy
                {
                    auto dest_memory_index = reader.read_varuint32();
                    auto src_memory_index = reader.read_varuint32();
                    if (dest_memory_index >= memories_.size() || src_memory_index >= memories_.size())
                    {
                        throw Trap("memory.copy memory index out of bounds");
                    }
                    auto size_value = pop_i32(stack);
                    auto src_value = pop_i32(stack);
                    auto dest_value = pop_i32(stack);
                    auto size_u = require_non_negative(size_value, "memory.copy size");
                    auto src_u = require_non_negative(src_value, "memory.copy source");
                    auto dest_u = require_non_negative(dest_value, "memory.copy destination");
                    auto& dest_memory = memories_[dest_memory_index];
                    auto& src_memory = memories_[src_memory_index];
                    if (static_cast<uint64_t>(src_u) + size_u > src_memory.data.size() ||
                        static_cast<uint64_t>(dest_u) + size_u > dest_memory.data.size())
                    {
                        throw Trap("memory.copy out of bounds");
                    }
                    if (size_u > 0)
                    {
                        if (&dest_memory == &src_memory)
                        {
                            std::memmove(dest_memory.data.data() + dest_u,
                                         src_memory.data.data() + src_u,
                                         size_u);
                        }
                        else
                        {
                            std::memcpy(dest_memory.data.data() + dest_u,
                                        src_memory.data.data() + src_u,
                                        size_u);
                        }
                    }
                    break;
                }
                case 0x0B: // memory.fill
                {
                    auto memory_index = reader.read_varuint32();
                    if (memory_index >= memories_.size())
                    {
                        throw Trap("memory.fill memory index out of bounds");
                    }
                    auto size_value = pop_i32(stack);
                    auto fill_value = pop_i32(stack);
                    auto dest_value = pop_i32(stack);
                    auto size_u = require_non_negative(size_value, "memory.fill size");
                    auto dest_u = require_non_negative(dest_value, "memory.fill destination");
                    auto& memory = memories_[memory_index];
                    if (static_cast<uint64_t>(dest_u) + size_u > memory.data.size())
                    {
                        throw Trap("memory.fill out of bounds");
                    }
                    if (size_u > 0)
                    {
                        std::memset(memory.data.data() + dest_u, static_cast<uint8_t>(fill_value & 0xFF), size_u);
                    }
                    break;
                }
                case 0x0C: // table.init (not supported)
                    throw Trap("table.init is not supported");
                case 0x0D: // elem.drop (not supported)
                    throw Trap("elem.drop is not supported");
                case 0x0E: // table.copy
                {
                    auto dest_table_index = reader.read_varuint32();
                    auto src_table_index = reader.read_varuint32();
                    if (dest_table_index >= tables_.size() || src_table_index >= tables_.size())
                    {
                        throw Trap("table.copy table index out of bounds");
                    }
                    auto count = require_non_negative(pop_i32(stack), "table.copy count");
                    auto src_offset = require_non_negative(pop_i32(stack), "table.copy source offset");
                    auto dest_offset = require_non_negative(pop_i32(stack), "table.copy destination offset");

                    auto& dest_table = tables_[dest_table_index];
                    auto& src_table = tables_[src_table_index];
                    if (dest_table.value_type != src_table.value_type)
                    {
                        throw Trap("table.copy type mismatch");
                    }
                    if (static_cast<uint64_t>(src_offset) + count > src_table.elements.size() ||
                        static_cast<uint64_t>(dest_offset) + count > dest_table.elements.size())
                    {
                        throw Trap("table.copy out of bounds");
                    }
                    if (count > 0)
                    {
                        if (&dest_table == &src_table)
                        {
                            std::vector<Value> temp;
                            temp.reserve(count);
                            for (uint32_t i = 0; i < count; ++i)
                            {
                                temp.push_back(src_table.elements[src_offset + i]);
                            }
                            for (uint32_t i = 0; i < count; ++i)
                            {
                                dest_table.elements[dest_offset + i] = temp[i];
                            }
                        }
                        else
                        {
                            for (uint32_t i = 0; i < count; ++i)
                            {
                                dest_table.elements[dest_offset + i] = src_table.elements[src_offset + i];
                            }
                        }
                    }
                    break;
                }
                case 0x0F: // table.grow
                {
                    auto table_index = reader.read_varuint32();
                    if (table_index >= tables_.size())
                    {
                        throw Trap("table.grow table index out of bounds");
                    }
                    auto delta = require_non_negative(pop_i32(stack), "table.grow delta");
                    auto value = pop_reference(stack, tables_[table_index].value_type);
                    auto& table = tables_[table_index];
                    uint32_t previous = static_cast<uint32_t>(table.elements.size());
                    uint64_t new_size = static_cast<uint64_t>(previous) + delta;
                    if (new_size > static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()))
                    {
                        push(Value::make<int32_t>(-1));
                        break;
                    }
                    if (table.type.limits.max && new_size > *table.type.limits.max)
                    {
                        push(Value::make<int32_t>(-1));
                        break;
                    }
                    try
                    {
                        table.elements.reserve(static_cast<size_t>(new_size));
                    }
                    catch (const std::bad_alloc&)
                    {
                        push(Value::make<int32_t>(-1));
                        break;
                    }
                    for (uint32_t i = 0; i < delta; ++i)
                    {
                        table.elements.push_back(value);
                    }
                    push(Value::make<int32_t>(static_cast<int32_t>(previous)));
                    break;
                }
                case 0x10: // table.size
                {
                    auto table_index = reader.read_varuint32();
                    if (table_index >= tables_.size())
                    {
                        throw Trap("table.size table index out of bounds");
                    }
                    auto size = static_cast<int32_t>(tables_[table_index].elements.size());
                    push(Value::make<int32_t>(size), ValueOrigin::LoadResult);
                    break;
                }
                case 0x11: // table.fill
                {
                    auto table_index = reader.read_varuint32();
                    if (table_index >= tables_.size())
                    {
                        throw Trap("table.fill table index out of bounds");
                    }
                    auto count = require_non_negative(pop_i32(stack), "table.fill count");
                    auto value = pop_reference(stack, tables_[table_index].value_type);
                    auto offset = require_non_negative(pop_i32(stack), "table.fill offset");
                    auto& table = tables_[table_index];
                    if (static_cast<uint64_t>(offset) + count > table.elements.size())
                    {
                        throw Trap("table.fill out of bounds");
                    }
                    for (uint32_t i = 0; i < count; ++i)
                    {
                        table.elements[offset + i] = value;
                    }
                    break;
                }
                case 0x00: // i32.trunc_sat_f32_s
                {
                    auto operand = pop_f32(stack);
                    auto converted = trunc_sat_signed<int32_t>(operand);
                    push(Value::make<int32_t>(converted));
                    break;
                }
                case 0x01: // i32.trunc_sat_f32_u
                {
                    auto operand = pop_f32(stack);
                    auto converted = trunc_sat_unsigned<uint32_t>(operand);
                    push(Value::make<int32_t>(static_cast<int32_t>(converted)));
                    break;
                }
                case 0x02: // i32.trunc_sat_f64_s
                {
                    auto operand = pop_f64(stack);
                    auto converted = trunc_sat_signed<int32_t>(operand);
                    push(Value::make<int32_t>(converted));
                    break;
                }
                case 0x03: // i32.trunc_sat_f64_u
                {
                    auto operand = pop_f64(stack);
                    auto converted = trunc_sat_unsigned<uint32_t>(operand);
                    push(Value::make<int32_t>(static_cast<int32_t>(converted)));
                    break;
                }
                case 0x04: // i64.trunc_sat_f32_s
                {
                    auto operand = pop_f32(stack);
                    auto converted = trunc_sat_signed<int64_t>(operand);
                    push(Value::make<int64_t>(converted));
                    break;
                }
                case 0x05: // i64.trunc_sat_f32_u
                {
                    auto operand = pop_f32(stack);
                    auto converted = trunc_sat_unsigned<uint64_t>(operand);
                    push(Value::make<int64_t>(static_cast<int64_t>(converted)));
                    break;
                }
                case 0x06: // i64.trunc_sat_f64_s
                {
                    auto operand = pop_f64(stack);
                    auto converted = trunc_sat_signed<int64_t>(operand);
                    push(Value::make<int64_t>(converted));
                    break;
                }
                case 0x07: // i64.trunc_sat_f64_u
                {
                    auto operand = pop_f64(stack);
                    auto converted = trunc_sat_unsigned<uint64_t>(operand);
                    push(Value::make<int64_t>(static_cast<int64_t>(converted)));
                    break;
                }
                default:
                    throw Trap("Unsupported 0xFC prefixed opcode: " + std::to_string(sat_opcode));
                }
                break;
            }
            case 0x6A: // i32.add
            {
                auto rhs = pop_i32(stack);
                auto lhs = pop_i32(stack);
                push(Value::make<int32_t>(lhs + rhs));
                break;
            }
            case 0x6B: // i32.sub
            {
                auto rhs = pop_i32(stack);
                auto lhs = pop_i32(stack);
                push(Value::make<int32_t>(lhs - rhs));
                break;
            }
            case 0x6C: // i32.mul
            {
                auto rhs = pop_i32(stack);
                auto lhs = pop_i32(stack);
                push(Value::make<int32_t>(lhs * rhs));
                break;
            }
            case 0x6D: // i32.div_s
            {
                auto rhs = pop_i32(stack);
                auto lhs = pop_i32(stack);
                if (rhs == 0)
                {
                    throw Trap("Integer divide by zero");
                }
                if (lhs == std::numeric_limits<int32_t>::min() && rhs == -1)
                {
                    throw Trap("Integer overflow");
                }
                push(Value::make<int32_t>(lhs / rhs));
                break;
            }
            case 0x6E: // i32.div_u
            {
                auto rhs = static_cast<uint32_t>(pop_i32(stack));
                auto lhs = static_cast<uint32_t>(pop_i32(stack));
                if (rhs == 0)
                {
                    throw Trap("Integer divide by zero");
                }
                push(Value::make<int32_t>(static_cast<int32_t>(lhs / rhs)));
                break;
            }
            case 0x6F: // i32.rem_s
            {
                auto rhs = pop_i32(stack);
                auto lhs = pop_i32(stack);
                if (rhs == 0)
                {
                    throw Trap("Integer remainder by zero");
                }
                if (lhs == std::numeric_limits<int32_t>::min() && rhs == -1)
                {
                    push(Value::make<int32_t>(0));
                }
                else
                {
                    push(Value::make<int32_t>(lhs % rhs));
                }
                break;
            }
            case 0x70: // i32.rem_u
            {
                auto rhs = static_cast<uint32_t>(pop_i32(stack));
                auto lhs = static_cast<uint32_t>(pop_i32(stack));
                if (rhs == 0)
                {
                    throw Trap("Integer remainder by zero");
                }
                push(Value::make<int32_t>(static_cast<int32_t>(lhs % rhs)));
                break;
            }
            case 0x71: // i32.and
            {
                auto rhs = pop_i32(stack);
                auto lhs = pop_i32(stack);
                push(Value::make<int32_t>(lhs & rhs));
                break;
            }
            case 0x72: // i32.or
            {
                auto rhs = pop_i32(stack);
                auto lhs = pop_i32(stack);
                push(Value::make<int32_t>(lhs | rhs));
                break;
            }
            case 0x73: // i32.xor
            {
                auto rhs = pop_i32(stack);
                auto lhs = pop_i32(stack);
                push(Value::make<int32_t>(lhs ^ rhs));
                break;
            }
            case 0x74: // i32.shl
            {
                auto rhs = pop_i32(stack) & 31;
                auto lhs = pop_i32(stack);
                push(Value::make<int32_t>(lhs << rhs));
                break;
            }
            case 0x75: // i32.shr_s
            {
                auto rhs = pop_i32(stack) & 31;
                auto lhs = pop_i32(stack);
                push(Value::make<int32_t>(lhs >> rhs));
                break;
            }
            case 0x76: // i32.shr_u
            {
                auto rhs = pop_i32(stack) & 31;
                auto lhs = static_cast<uint32_t>(pop_i32(stack));
                push(Value::make<int32_t>(static_cast<int32_t>(lhs >> rhs)));
                break;
            }
            case 0x77: // i32.rotl
            {
                auto rhs = pop_i32(stack) & 31;
                auto lhs = static_cast<uint32_t>(pop_i32(stack));
                auto rotated = std::rotl(lhs, rhs);
                push(Value::make<int32_t>(static_cast<int32_t>(rotated)));
                break;
            }
            case 0x78: // i32.rotr
            {
                auto rhs = pop_i32(stack) & 31;
                auto lhs = static_cast<uint32_t>(pop_i32(stack));
                auto rotated = std::rotr(lhs, rhs);
                push(Value::make<int32_t>(static_cast<int32_t>(rotated)));
                break;
            }
            default:
                throw Trap("Unsupported opcode encountered: " + std::to_string(opcode));
            }
        }
    }

    std::vector<FunctionInstance> functions_;
    std::vector<GlobalInstance> globals_;
    std::vector<MemoryInstance> memories_;
    std::vector<TableInstance> tables_;
    std::vector<DataSegmentInstance> data_segments_;
    std::unordered_map<std::string, std::pair<ExternalKind, uint32_t>> export_table_;
    std::unordered_map<std::string, HostFunctionRecord> host_functions_;
    std::unordered_map<std::string, HostMemoryRecord> host_memories_;
    std::unordered_map<std::string, HostTableRecord> host_tables_;
    std::unordered_map<std::string, HostGlobalRecord> host_globals_;
    Module module_;
};

Interpreter::Interpreter()
    : impl_(std::make_unique<Impl>())
{
}

Interpreter::~Interpreter() = default;
Interpreter::Interpreter(Interpreter&&) noexcept = default;
Interpreter& Interpreter::operator=(Interpreter&&) noexcept = default;

void Interpreter::load(const std::vector<uint8_t>& wasm_binary)
{
    impl_->load(wasm_binary);
}

void Interpreter::load(std::span<const uint8_t> wasm_binary)
{
    impl_->load(wasm_binary);
}

ExecutionResult Interpreter::invoke(std::string_view export_name, std::span<const Value> args)
{
    return impl_->invoke(export_name, args);
}

MemoryView Interpreter::memory()
{
    return impl_->memory();
}

const Module& Interpreter::module() const
{
    return impl_->module();
}

void Interpreter::register_host_function(const std::string& module,
                                         const std::string& name,
                                         std::vector<ValueType> params,
                                         std::vector<ValueType> results,
                                         HostFunction callback)
{
    FunctionType type;
    type.params = std::move(params);
    type.results = std::move(results);
    impl_->register_host_function(
        module,
        name,
        type,
        [cb = std::move(callback)](void*, std::span<const Value> args) -> ExecutionResult {
            return cb(args);
        },
        nullptr);
}

void Interpreter::register_host_memory(const std::string& module,
                                       const std::string& name,
                                       MemoryType type,
                                       std::vector<uint8_t> data)
{
    impl_->register_host_memory(module, name, type, std::move(data));
}

void Interpreter::register_host_table(const std::string& module,
                                      const std::string& name,
                                      TableType type,
                                      std::vector<Value> elements)
{
    impl_->register_host_table(module, name, type, std::move(elements));
}

void Interpreter::register_host_global(const std::string& module,
                                       const std::string& name,
                                       GlobalType type,
                                       Value value)
{
    impl_->register_host_global(module, name, type, std::move(value));
}

std::vector<uint8_t> read_file(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        throw std::runtime_error("Failed to open file: " + path);
    }
    file.seekg(0, std::ios::end);
    const auto size = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(size);
    file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(size));
    return buffer;
}

} // namespace wasm
