#include "wasm/module_loader.hpp"

#include <span>
#include <stdexcept>
#include <string>
#include <utility>

#include "wasm/binary_reader.hpp"

namespace wasm
{
namespace
{
constexpr uint32_t kWasmMagic = 0x6D736100; // "\0asm"
constexpr uint32_t kWasmVersion = 0x00000001;

struct Section
{
    uint8_t id{0};
    std::span<const uint8_t> payload;
};

class ModuleParser
{
public:
    explicit ModuleParser(std::span<const uint8_t> data)
        : reader_(data)
        , data_(data)
    {
    }

    Module parse()
    {
        parse_header();
        while (!reader_.eof())
        {
            auto section = read_section();
            if (section.payload.empty())
            {
                continue;
            }
            BinaryReader section_reader(section.payload);
            switch (section.id)
            {
            case 0:
                // Custom section; ignore for now
                break;
            case 1:
                parse_type_section(section_reader);
                break;
            case 2:
                parse_import_section(section_reader);
                break;
            case 3:
                parse_function_section(section_reader);
                break;
            case 4:
                parse_table_section(section_reader);
                break;
            case 5:
                parse_memory_section(section_reader);
                break;
            case 6:
                parse_global_section(section_reader);
                break;
            case 7:
                parse_export_section(section_reader);
                break;
            case 8:
                parse_start_section(section_reader);
                break;
            case 9:
                parse_element_section(section_reader);
                break;
            case 10:
                parse_code_section(section_reader);
                break;
            case 11:
                parse_data_section(section_reader);
                break;
            case 12:
                parse_data_count_section(section_reader);
                break;
            default:
                throw std::runtime_error("Unsupported section id: " + std::to_string(section.id));
            }
        }

        if (module_.functions.size() != module_.codes.size())
        {
            throw std::runtime_error("Function and code section size mismatch");
        }

        return module_;
    }

    void parse_data_count_section(BinaryReader& section_reader)
    {
        (void)section_reader.read_varuint32();
    }

private:
    void parse_header()
    {
        auto magic = reader_.read_u32();
        if (magic != kWasmMagic)
        {
            throw std::runtime_error("Invalid WASM magic number");
        }

        auto version = reader_.read_u32();
        if (version != kWasmVersion)
        {
            throw std::runtime_error("Unsupported WASM version");
        }
    }

    Section read_section()
    {
        if (reader_.eof())
        {
            return {};
        }
        auto id = reader_.read_u8();
        auto size = reader_.read_varuint32();
        if (reader_.offset() + size > data_.size())
        {
            throw std::runtime_error("Section size exceeds module bounds");
        }
        auto payload = data_.subspan(reader_.offset(), size);
        reader_.skip_bytes(size);
        return Section{id, payload};
    }

    void parse_type_section(BinaryReader& section_reader)
    {
        auto count = section_reader.read_varuint32();
        module_.types.reserve(module_.types.size() + count);
        for (uint32_t i = 0; i < count; ++i)
        {
            auto form = section_reader.read_varuint7();
            if (form != 0x60)
            {
                throw std::runtime_error("Expected function type form 0x60");
            }
            FunctionType type;
            auto param_count = section_reader.read_varuint32();
            type.params.reserve(param_count);
            for (uint32_t j = 0; j < param_count; ++j)
            {
                type.params.push_back(static_cast<ValueType>(section_reader.read_varint7()));
            }
            auto result_count = section_reader.read_varuint32();
            type.results.reserve(result_count);
            for (uint32_t j = 0; j < result_count; ++j)
            {
                type.results.push_back(static_cast<ValueType>(section_reader.read_varint7()));
            }
            module_.types.push_back(std::move(type));
        }
    }

    Limits read_limits(BinaryReader& reader)
    {
        Limits limits;
        auto flags = reader.read_varuint1();
        limits.min = reader.read_varuint32();
        if ((flags & 0x1U) != 0)
        {
            limits.max = reader.read_varuint32();
        }
        return limits;
    }

    void parse_import_section(BinaryReader& section_reader)
    {
        auto count = section_reader.read_varuint32();
        module_.imports.reserve(module_.imports.size() + count);
        for (uint32_t i = 0; i < count; ++i)
        {
            Import import;
            import.module = read_name(section_reader);
            import.name = read_name(section_reader);
            import.kind = static_cast<ExternalKind>(section_reader.read_u8());
            switch (import.kind)
            {
            case ExternalKind::Function:
                import.type_index = section_reader.read_varuint32();
                break;
            case ExternalKind::Table:
                import.table_type.element_type = static_cast<RefType>(section_reader.read_u8());
                import.table_type.limits = read_limits(section_reader);
                break;
            case ExternalKind::Memory:
                import.memory_type.limits = read_limits(section_reader);
                break;
            case ExternalKind::Global:
                import.global_type.value_type = static_cast<ValueType>(section_reader.read_varint7());
                import.global_type.is_mutable = section_reader.read_varuint1() != 0;
                break;
            default:
                throw std::runtime_error("Unsupported import kind");
            }
            module_.imports.push_back(std::move(import));
        }
    }

    void parse_function_section(BinaryReader& section_reader)
    {
        auto count = section_reader.read_varuint32();
        module_.functions.reserve(module_.functions.size() + count);
        for (uint32_t i = 0; i < count; ++i)
        {
            module_.functions.push_back(section_reader.read_varuint32());
        }
    }

    void parse_table_section(BinaryReader& section_reader)
    {
        auto count = section_reader.read_varuint32();
        module_.tables.reserve(module_.tables.size() + count);
        for (uint32_t i = 0; i < count; ++i)
        {
            TableType table;
            auto element_type_raw = section_reader.read_u8();
            switch (element_type_raw)
            {
            case 0x70:
                table.element_type = RefType::FuncRef;
                break;
            case 0x6F:
                table.element_type = RefType::ExternRef;
                break;
            default:
                throw std::runtime_error("Unsupported table element type: " + std::to_string(element_type_raw));
            }
            table.limits = read_limits(section_reader);
            module_.tables.push_back(table);
        }
    }

    void parse_memory_section(BinaryReader& section_reader)
    {
        auto count = section_reader.read_varuint32();
        module_.memories.reserve(module_.memories.size() + count);
        for (uint32_t i = 0; i < count; ++i)
        {
            MemoryType memory;
            memory.limits = read_limits(section_reader);
            module_.memories.push_back(memory);
        }
    }

    ConstantExpression parse_constant_expression(BinaryReader& reader)
    {
        auto opcode = reader.read_u8();
        ConstantExpression expr;
        switch (opcode)
        {
        case 0x41: // i32.const
            expr.kind = ConstantExpression::Kind::I32Const;
            expr.value = Value::make<int32_t>(reader.read_varint32());
            break;
        case 0x42: // i64.const
            expr.kind = ConstantExpression::Kind::I64Const;
            expr.value = Value::make<int64_t>(reader.read_varint64());
            break;
        case 0x43: // f32.const
            expr.kind = ConstantExpression::Kind::F32Const;
            expr.value = Value::make<float>(reader.read_f32());
            break;
        case 0x44: // f64.const
            expr.kind = ConstantExpression::Kind::F64Const;
            expr.value = Value::make<double>(reader.read_f64());
            break;
        case 0x23: // global.get
            expr.kind = ConstantExpression::Kind::GlobalGet;
            expr.index = reader.read_varuint32();
            break;
        case 0xD0: // ref.null
        {
            expr.kind = ConstantExpression::Kind::RefNull;
            auto heap_type = reader.read_varuint7();
            switch (heap_type)
            {
            case 0x70:
                expr.value = Value::make_funcref_null();
                break;
            case 0x6F:
                expr.value = Value::make_externref_null();
                break;
            default:
                throw std::runtime_error("Unsupported heap type for ref.null constant: " +
                                         std::to_string(heap_type));
            }
            break;
        }
        case 0xD2: // ref.func
        {
            expr.kind = ConstantExpression::Kind::RefFunc;
            auto func_index = reader.read_varuint32();
            expr.index = func_index;
            expr.value = Value::make_funcref(func_index);
            break;
        }
        default:
            throw std::runtime_error("Unsupported constant expression opcode");
        }
        auto end = reader.read_u8();
        if (end != 0x0B)
        {
            throw std::runtime_error("Constant expression missing end opcode");
        }
        return expr;
    }

    void parse_global_section(BinaryReader& section_reader)
    {
        auto count = section_reader.read_varuint32();
        module_.globals.reserve(module_.globals.size() + count);
        for (uint32_t i = 0; i < count; ++i)
        {
            Global global;
            global.type.value_type = static_cast<ValueType>(section_reader.read_varint7());
            global.type.is_mutable = section_reader.read_varuint1() != 0;
            global.init = parse_constant_expression(section_reader);
            module_.globals.push_back(std::move(global));
        }
    }

    void parse_export_section(BinaryReader& section_reader)
    {
        auto count = section_reader.read_varuint32();
        module_.exports.reserve(module_.exports.size() + count);
        for (uint32_t i = 0; i < count; ++i)
        {
            Export exp;
            exp.name = read_name(section_reader);
            exp.kind = static_cast<ExternalKind>(section_reader.read_u8());
            exp.index = section_reader.read_varuint32();
            module_.exports.push_back(std::move(exp));
        }
    }

    void parse_start_section(BinaryReader& section_reader)
    {
        module_.start_function = section_reader.read_varuint32();
    }

    void parse_element_section(BinaryReader& section_reader)
    {
        auto count = section_reader.read_varuint32();
        module_.elements.reserve(module_.elements.size() + count);
        for (uint32_t i = 0; i < count; ++i)
        {
            ElementSegment segment;
            auto flags = section_reader.read_varuint32();
            bool is_active = (flags & 0x01U) == 0;
            bool has_table_index = is_active && ((flags & 0x02U) != 0);
            bool is_declarative = (flags & 0x03U) == 0x03U;
            bool use_expressions = (flags & 0x04U) != 0;

            if (use_expressions)
            {
                throw std::runtime_error("Element segments with expressions are not supported");
            }

            if (is_active)
            {
                segment.table_index = has_table_index ? section_reader.read_varuint32() : 0;
                segment.offset = parse_constant_expression(section_reader);
            }

            uint8_t elem_kind = section_reader.read_u8();
            if (elem_kind != 0x00 && elem_kind != 0x01)
            {
                // rewind one byte; it belongs to the element count
                section_reader.set_offset(section_reader.offset() - 1);
            }
            else if (elem_kind != 0x00)
            {
                throw std::runtime_error("Unsupported element kind: " + std::to_string(elem_kind));
            }

            auto func_count = section_reader.read_varuint32();
            std::vector<uint32_t> func_indices(func_count);
            for (uint32_t j = 0; j < func_count; ++j)
            {
                func_indices[j] = section_reader.read_varuint32();
            }

            if (is_active && !is_declarative)
            {
                segment.func_indices = std::move(func_indices);
                module_.elements.push_back(std::move(segment));
            }
        }
    }

    void parse_code_section(BinaryReader& section_reader)
    {
        auto count = section_reader.read_varuint32();
        module_.codes.reserve(module_.codes.size() + count);
        for (uint32_t i = 0; i < count; ++i)
        {
            auto size = section_reader.read_varuint32();
            auto section_data = section_reader.data();
            if (section_reader.offset() + size > section_data.size())
            {
                throw std::runtime_error("Code entry exceeds section bounds");
            }
            auto entry_span = section_data.subspan(section_reader.offset(), size);
            BinaryReader entry_reader(entry_span);
            Code code;
            auto local_count = entry_reader.read_varuint32();
            code.locals.reserve(local_count);
            for (uint32_t j = 0; j < local_count; ++j)
            {
                LocalDecl decl;
                decl.count = entry_reader.read_varuint32();
                decl.type = static_cast<ValueType>(entry_reader.read_varint7());
                code.locals.push_back(decl);
            }
            auto body_span = entry_span.subspan(entry_reader.offset());
            code.body.assign(body_span.begin(), body_span.end());
            module_.codes.push_back(std::move(code));
            section_reader.skip_bytes(size);
        }
    }

    void parse_data_section(BinaryReader& section_reader)
    {
        auto count = section_reader.read_varuint32();
        module_.data_segments.reserve(module_.data_segments.size() + count);
        for (uint32_t i = 0; i < count; ++i)
        {
            DataSegment segment;
            auto mode_or_index = section_reader.read_varuint32();
            if (mode_or_index <= 2)
            {
                switch (mode_or_index)
                {
                case 0: // active, memory index 0
                    segment.is_passive = false;
                    segment.has_memory_index = true;
                    segment.memory_index = 0;
                    segment.offset = parse_constant_expression(section_reader);
                    break;
                case 1: // passive
                    segment.is_passive = true;
                    segment.has_memory_index = false;
                    break;
                case 2: // active with explicit memory index
                    segment.is_passive = false;
                    segment.has_memory_index = true;
                    segment.memory_index = section_reader.read_varuint32();
                    segment.offset = parse_constant_expression(section_reader);
                    break;
                default:
                    break;
                }
            }
            else
            {
                // Legacy encoding: first value is memory index
                segment.is_passive = false;
                segment.has_memory_index = true;
                segment.memory_index = mode_or_index;
                segment.offset = parse_constant_expression(section_reader);
            }

            auto byte_count = section_reader.read_varuint32();
            segment.bytes.resize(byte_count);
            for (uint32_t j = 0; j < byte_count; ++j)
            {
                segment.bytes[j] = section_reader.read_u8();
            }
            module_.data_segments.push_back(std::move(segment));
        }
    }

    std::string read_name(BinaryReader& reader)
    {
        auto length = reader.read_varuint32();
        auto data = reader.data();
        if (reader.offset() + length > data.size())
        {
            throw std::runtime_error("Name exceeds section bounds");
        }
        std::string name(length, '\0');
        for (uint32_t i = 0; i < length; ++i)
        {
            name[i] = static_cast<char>(reader.read_u8());
        }
        return name;
    }

    BinaryReader reader_;
    std::span<const uint8_t> data_;
    Module module_;
};
} // namespace

Module parse_module(std::span<const uint8_t> bytes)
{
    ModuleParser parser(bytes);
    return parser.parse();
}

} // namespace wasm
