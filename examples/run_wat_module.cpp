#include "wasm/interpreter.hpp"
#include "wasm/module_loader.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <random>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <vector>

namespace
{
using wasm::ExternalKind;
using wasm::FunctionType;
using wasm::Interpreter;
using wasm::Module;
using wasm::ExecutionResult;
using wasm::Value;
using wasm::ValueType;

struct Options
{
    std::string module_path;
    std::optional<std::string> export_name;
    std::vector<Value> call_args;
    bool list_exports{false};
    bool list_imports{false};
    bool skip_invoke{false};
    std::vector<std::string> wasi_args;
    std::vector<std::string> wasi_env;
};

[[noreturn]] void print_usage_and_exit(const std::string& program)
{
    std::cerr << "Usage: " << program << " [options] <module.wasm>\n"
              << "Options:\n"
              << "  --invoke <name>          Exported function to invoke (default: auto-detect)\n"
              << "  --arg-i32 <value>        Append i32 argument\n"
              << "  --arg-i64 <value>        Append i64 argument\n"
              << "  --arg-f32 <value>        Append f32 argument\n"
              << "  --arg-f64 <value>        Append f64 argument\n"
              << "  --arg-funcref-null       Append null funcref argument\n"
              << "  --arg-externref-null     Append null externref argument\n"
              << "  --wasi-arg <value>       Pass value as WASI argv entry\n"
              << "  --wasi-env <key=value>   Pass entry to WASI environment\n"
              << "  --list-exports           Print exported items before running\n"
              << "  --list-imports           Print imported items before running\n"
              << "  --no-run                 Skip invocation (useful with --list-*)\n"
              << "  -h, --help               Show this message\n";
    std::exit(EXIT_FAILURE);
}

Value make_value_i32(const std::string& text)
{
    std::size_t consumed = 0;
    long long parsed = 0;
    try
    {
        parsed = std::stoll(text, &consumed, 0);
    }
    catch (const std::exception& ex)
    {
        throw std::runtime_error("failed to parse i32 argument '" + text + "': " + ex.what());
    }
    if (consumed != text.size())
    {
        throw std::runtime_error("trailing characters in i32 argument '" + text + "'");
    }
    if (parsed < std::numeric_limits<int32_t>::min() || parsed > std::numeric_limits<int32_t>::max())
    {
        throw std::runtime_error("i32 argument out of range: '" + text + "'");
    }
    return Value::make<int32_t>(static_cast<int32_t>(parsed));
}

Value make_value_i64(const std::string& text)
{
    std::size_t consumed = 0;
    long long parsed = 0;
    try
    {
        parsed = std::stoll(text, &consumed, 0);
    }
    catch (const std::exception& ex)
    {
        throw std::runtime_error("failed to parse i64 argument '" + text + "': " + ex.what());
    }
    if (consumed != text.size())
    {
        throw std::runtime_error("trailing characters in i64 argument '" + text + "'");
    }
    return Value::make<int64_t>(static_cast<int64_t>(parsed));
}

Value make_value_f32(const std::string& text)
{
    std::size_t consumed = 0;
    float parsed = 0.0f;
    try
    {
        parsed = std::stof(text, &consumed);
    }
    catch (const std::exception& ex)
    {
        throw std::runtime_error("failed to parse f32 argument '" + text + "': " + ex.what());
    }
    if (consumed != text.size())
    {
        throw std::runtime_error("trailing characters in f32 argument '" + text + "'");
    }
    return Value::make<float>(parsed);
}

Value make_value_f64(const std::string& text)
{
    std::size_t consumed = 0;
    double parsed = 0.0;
    try
    {
        parsed = std::stod(text, &consumed);
    }
    catch (const std::exception& ex)
    {
        throw std::runtime_error("failed to parse f64 argument '" + text + "': " + ex.what());
    }
    if (consumed != text.size())
    {
        throw std::runtime_error("trailing characters in f64 argument '" + text + "'");
    }
    return Value::make<double>(parsed);
}

Options parse_options(int argc, char** argv)
{
    if (argc < 2)
    {
        print_usage_and_exit(argv[0]);
    }

    Options options;
    for (int i = 1; i < argc; ++i)
    {
        std::string_view arg = argv[i];
        if (arg == "--help" || arg == "-h")
        {
            print_usage_and_exit(argv[0]);
        }
        else if (arg == "--invoke")
        {
            if (i + 1 >= argc)
            {
                throw std::runtime_error("--invoke requires a following name");
            }
            options.export_name = argv[++i];
        }
        else if (arg == "--arg-i32")
        {
            if (i + 1 >= argc)
            {
                throw std::runtime_error("--arg-i32 requires a value");
            }
            options.call_args.push_back(make_value_i32(argv[++i]));
        }
        else if (arg == "--arg-i64")
        {
            if (i + 1 >= argc)
            {
                throw std::runtime_error("--arg-i64 requires a value");
            }
            options.call_args.push_back(make_value_i64(argv[++i]));
        }
        else if (arg == "--arg-f32")
        {
            if (i + 1 >= argc)
            {
                throw std::runtime_error("--arg-f32 requires a value");
            }
            options.call_args.push_back(make_value_f32(argv[++i]));
        }
        else if (arg == "--arg-f64")
        {
            if (i + 1 >= argc)
            {
                throw std::runtime_error("--arg-f64 requires a value");
            }
            options.call_args.push_back(make_value_f64(argv[++i]));
        }
        else if (arg == "--arg-funcref-null")
        {
            options.call_args.push_back(Value::make_funcref_null());
        }
        else if (arg == "--arg-externref-null")
        {
            options.call_args.push_back(Value::make_externref_null());
        }
        else if (arg == "--list-exports")
        {
            options.list_exports = true;
        }
        else if (arg == "--list-imports")
        {
            options.list_imports = true;
        }
        else if (arg == "--no-run")
        {
            options.skip_invoke = true;
        }
        else if (arg == "--wasi-arg")
        {
            if (i + 1 >= argc)
            {
                throw std::runtime_error("--wasi-arg requires a value");
            }
            options.wasi_args.emplace_back(argv[++i]);
        }
        else if (arg == "--wasi-env")
        {
            if (i + 1 >= argc)
            {
                throw std::runtime_error("--wasi-env requires KEY=VALUE");
            }
            options.wasi_env.emplace_back(argv[++i]);
        }
        else if (!arg.empty() && arg.front() == '-')
        {
            throw std::runtime_error("unrecognized option '" + std::string(arg) + "'");
        }
        else
        {
            if (!options.module_path.empty())
            {
                throw std::runtime_error("multiple modules specified: '" + options.module_path + "' and '" +
                                         std::string(arg) + "'");
            }
            options.module_path = std::string(arg);
        }
    }

    if (options.module_path.empty())
    {
        throw std::runtime_error("missing module path");
    }
    return options;
}

struct ModuleIntrospector
{
    explicit ModuleIntrospector(const Module& module)
        : module_(module)
    {
    }

    const FunctionType* function_type(uint32_t index) const
    {
        uint32_t imports_seen = 0;
        for (const auto& import : module_.imports)
        {
            if (import.kind != ExternalKind::Function)
            {
                continue;
            }
            if (index == imports_seen)
            {
                if (import.type_index >= module_.types.size())
                {
                    return nullptr;
                }
                return &module_.types[import.type_index];
            }
            ++imports_seen;
        }
        const uint32_t local_index = index - imports_seen;
        if (local_index >= module_.functions.size())
        {
            return nullptr;
        }
        const uint32_t type_index = module_.functions[local_index];
        if (type_index >= module_.types.size())
        {
            return nullptr;
        }
        return &module_.types[type_index];
    }

    std::optional<FunctionType> export_function_type(const std::string& name) const
    {
        for (const auto& export_entry : module_.exports)
        {
            if (export_entry.kind == ExternalKind::Function && export_entry.name == name)
            {
                const auto* type = function_type(export_entry.index);
                if (type != nullptr)
                {
                    return *type;
                }
            }
        }
        return std::nullopt;
    }

    const Module& module_;
};

std::string join_value_types(const std::vector<ValueType>& types)
{
    if (types.empty())
    {
        return "()";
    }
    std::ostringstream oss;
    oss << "(";
    for (size_t i = 0; i < types.size(); ++i)
    {
        if (i != 0)
        {
            oss << ", ";
        }
        oss << wasm::to_string(types[i]);
    }
    oss << ")";
    return oss.str();
}

std::string describe_function(const FunctionType& type)
{
    std::ostringstream oss;
    oss << join_value_types(type.params) << " -> " << join_value_types(type.results);
    return oss.str();
}

std::string detect_default_export(const Module& module)
{
    for (const auto& export_entry : module.exports)
    {
        if (export_entry.kind == ExternalKind::Function && export_entry.name == "_start")
        {
            return "_start";
        }
    }
    for (const auto& export_entry : module.exports)
    {
        if (export_entry.kind == ExternalKind::Function && export_entry.name == "main")
        {
            return "main";
        }
    }
    for (const auto& export_entry : module.exports)
    {
        if (export_entry.kind == ExternalKind::Function)
        {
            return export_entry.name;
        }
    }
    return {};
}

void print_imports(const Module& module, const ModuleIntrospector& introspector)
{
    if (module.imports.empty())
    {
        std::cout << "Imports: (none)\n";
        return;
    }
    std::cout << "Imports:\n";
    uint32_t func_index = 0;
    for (const auto& import : module.imports)
    {
        std::cout << "  " << import.module << "." << import.name << " : ";
        switch (import.kind)
        {
        case ExternalKind::Function:
        {
            const auto* type = introspector.function_type(func_index);
            ++func_index;
            if (type == nullptr)
            {
                std::cout << "func (unknown signature)\n";
            }
            else
            {
                std::cout << "func " << describe_function(*type) << "\n";
            }
            break;
        }
        case ExternalKind::Memory:
        {
            std::cout << "memory min=" << import.memory_type.limits.min;
            if (import.memory_type.limits.max)
            {
                std::cout << " max=" << *import.memory_type.limits.max;
            }
            std::cout << "\n";
            break;
        }
        case ExternalKind::Table:
        {
            std::cout << "table type=" << (import.table_type.element_type == wasm::RefType::FuncRef ? "funcref"
                                                                                                   : "externref")
                      << " min=" << import.table_type.limits.min;
            if (import.table_type.limits.max)
            {
                std::cout << " max=" << *import.table_type.limits.max;
            }
            std::cout << "\n";
            break;
        }
        case ExternalKind::Global:
        {
            std::cout << "global type=" << wasm::to_string(import.global_type.value_type)
                      << (import.global_type.is_mutable ? " mutable" : " immutable") << "\n";
            break;
        }
        default:
            std::cout << "unknown\n";
            break;
        }
    }
}

void print_exports(const Module& module, const ModuleIntrospector& introspector)
{
    if (module.exports.empty())
    {
        std::cout << "Exports: (none)\n";
        return;
    }
    std::cout << "Exports:\n";
    for (const auto& export_entry : module.exports)
    {
        std::cout << "  " << export_entry.name << " : ";
        switch (export_entry.kind)
        {
        case ExternalKind::Function:
        {
            const auto* type = introspector.function_type(export_entry.index);
            if (type == nullptr)
            {
                std::cout << "func (unknown signature)\n";
            }
            else
            {
                std::cout << "func " << describe_function(*type) << "\n";
            }
            break;
        }
        case ExternalKind::Global:
            std::cout << "global\n";
            break;
        case ExternalKind::Memory:
            std::cout << "memory\n";
            break;
        case ExternalKind::Table:
            std::cout << "table\n";
            break;
        default:
            std::cout << "unknown\n";
            break;
        }
    }
}

std::string value_to_string(const Value& value)
{
    std::ostringstream oss;
    switch (value.type)
    {
    case ValueType::I32:
        oss << value.storage.i32;
        break;
    case ValueType::I64:
        oss << value.storage.i64;
        break;
    case ValueType::F32:
        oss << std::setprecision(7) << value.storage.f32;
        break;
    case ValueType::F64:
        oss << std::setprecision(15) << value.storage.f64;
        break;
    case ValueType::FuncRef:
        if (value.is_null_ref())
        {
            oss << "funcref(null)";
        }
        else
        {
            oss << "funcref(" << value.storage.ref << ")";
        }
        break;
    case ValueType::ExternRef:
        if (value.is_null_ref())
        {
            oss << "externref(null)";
        }
        else
        {
            oss << "externref(" << value.storage.ref << ")";
        }
        break;
    default:
        oss << "<unknown>";
        break;
    }
    return oss.str();
}

std::optional<int> parse_proc_exit_trap(const std::string& trap_message)
{
    constexpr std::string_view prefix = "wasi::proc_exit(";
    if (!trap_message.starts_with(prefix) || trap_message.back() != ')')
    {
        return std::nullopt;
    }
    const auto inner = trap_message.substr(prefix.size(), trap_message.size() - prefix.size() - 1);
    try
    {
        int code = std::stoi(inner, nullptr, 10);
        return code;
    }
    catch (...)
    {
        return 0;
    }
}

struct WasiPreview1Host
{
    WasiPreview1Host(Interpreter& interpreter,
                     std::vector<std::string> args,
                     std::vector<std::string> env)
        : interpreter_(interpreter)
        , args_(std::move(args))
        , env_(std::move(env))
        , preopen_host_path_(std::filesystem::weakly_canonical(std::filesystem::current_path()))
    {
        preopen_host_path_string_ = preopen_host_path_.string();
        if (!preopen_host_path_string_.empty() &&
            preopen_host_path_string_.back() != std::filesystem::path::preferred_separator)
        {
            preopen_host_path_string_.push_back(std::filesystem::path::preferred_separator);
        }
    }

    ~WasiPreview1Host()
    {
        for (auto& [_, handle] : files_)
        {
            if (handle.file != nullptr)
            {
                std::fclose(handle.file);
            }
        }
    }

    void register_all()
    {
        register_args_sizes_get();
        register_args_get();
        register_environ_sizes_get();
        register_environ_get();
        register_clock_time_get();
        register_random_get();
        register_fd_read();
        register_fd_close();
        register_fd_seek();
        register_fd_fdstat_get();
        register_fd_prestat_get();
        register_fd_prestat_dir_name();
        register_path_open();
    }

private:
    static constexpr int32_t kErrnoSuccess = 0;
    static constexpr int32_t kErrnoAcces = 2;
    static constexpr int32_t kErrnoBadf = 8;
    static constexpr int32_t kErrnoFault = 21;
    static constexpr int32_t kErrnoInval = 28;
    static constexpr int32_t kErrnoIo = 29;
    static constexpr int32_t kErrnoNoent = 44;
    static constexpr int32_t kErrnoNosys = 52;
    static constexpr int32_t kErrnoNotcapable = 76;
    static constexpr int32_t kErrnoIsdir = 31;
    static constexpr int32_t kErrnoNotdir = 54;

    static constexpr uint8_t kFiletypeUnknown = 0;
    static constexpr uint8_t kFiletypeCharacterDevice = 2;
    static constexpr uint8_t kFiletypeDirectory = 3;
    static constexpr uint8_t kFiletypeRegularFile = 4;

    static constexpr uint64_t kRightFdRead = 0x0000000000000001ULL;
    static constexpr uint64_t kRightFdWrite = 0x0000000000000002ULL;
    static constexpr uint64_t kRightFdSeek = 0x0000000000000004ULL;
    static constexpr uint64_t kRightFdTell = 0x0000000000000040ULL;
    static constexpr uint64_t kRightPathOpen = 0x0000000000002000ULL;

    struct MemoryAccessor
    {
        wasm::MemoryView view;

        bool store_u32(uint32_t offset, uint32_t value) const
        {
            if (view.data == nullptr)
            {
                return false;
            }
            const uint64_t end = static_cast<uint64_t>(offset) + sizeof(uint32_t);
            if (end > view.size)
            {
                return false;
            }
            std::memcpy(view.data + offset, &value, sizeof(uint32_t));
            return true;
        }

        bool store_u64(uint32_t offset, uint64_t value) const
        {
            if (view.data == nullptr)
            {
                return false;
            }
            const uint64_t end = static_cast<uint64_t>(offset) + sizeof(uint64_t);
            if (end > view.size)
            {
                return false;
            }
            std::memcpy(view.data + offset, &value, sizeof(uint64_t));
            return true;
        }

        bool store_bytes(uint32_t offset, const char* data, uint32_t length) const
        {
            if (view.data == nullptr)
            {
                return false;
            }
            const uint64_t end = static_cast<uint64_t>(offset) + length;
            if (end > view.size)
            {
                return false;
            }
            std::memcpy(view.data + offset, data, length);
            return true;
        }

        bool load_u32(uint32_t offset, uint32_t& value) const
        {
            if (view.data == nullptr)
            {
                return false;
            }
            const uint64_t end = static_cast<uint64_t>(offset) + sizeof(uint32_t);
            if (end > view.size)
            {
                return false;
            }
            std::memcpy(&value, view.data + offset, sizeof(uint32_t));
            return true;
        }

        bool load_u64(uint32_t offset, uint64_t& value) const
        {
            if (view.data == nullptr)
            {
                return false;
            }
            const uint64_t end = static_cast<uint64_t>(offset) + sizeof(uint64_t);
            if (end > view.size)
            {
                return false;
            }
            std::memcpy(&value, view.data + offset, sizeof(uint64_t));
            return true;
        }

        const uint8_t* data(uint32_t offset, uint32_t length) const
        {
            if (view.data == nullptr)
            {
                return nullptr;
            }
            const uint64_t end = static_cast<uint64_t>(offset) + length;
            if (end > view.size)
            {
                return nullptr;
            }
            return view.data + offset;
        }
    };

    MemoryAccessor memory() const
    {
        return MemoryAccessor{interpreter_.memory()};
    }

    ExecutionResult success(int32_t errno_value = kErrnoSuccess) const
    {
        ExecutionResult result;
        result.values.push_back(Value::make<int32_t>(errno_value));
        return result;
    }

    ExecutionResult fault() const
    {
        return success(kErrnoFault);
    }

    ExecutionResult errno_result(int32_t code) const
    {
        return success(code);
    }

    int32_t errno_from_host(int err) const
    {
        switch (err)
        {
        case 0:
            return kErrnoSuccess;
        case EACCES:
            return kErrnoAcces;
        case EISDIR:
            return kErrnoIsdir;
        case ENOENT:
            return kErrnoNoent;
        case ENOTDIR:
            return kErrnoNotdir;
        default:
            return kErrnoIo;
        }
    }

    struct FileHandle
    {
        std::FILE* file{nullptr};
        bool readable{false};
        bool seekable{false};
    };

    FileHandle* find_file(int32_t fd)
    {
        auto it = files_.find(fd);
        if (it == files_.end())
        {
            return nullptr;
        }
        return &it->second;
    }

    int32_t read_fd(int32_t fd, uint8_t* dest, uint32_t length, uint32_t& bytes_read)
    {
        bytes_read = 0;
        if (length == 0)
        {
            return kErrnoSuccess;
        }

        if (fd == 0)
        {
            std::cin.read(reinterpret_cast<char*>(dest), static_cast<std::streamsize>(length));
            std::streamsize got = std::cin.gcount();
            if (got < 0)
            {
                return kErrnoIo;
            }
            bytes_read = static_cast<uint32_t>(got);
            if (std::cin.bad())
            {
                return kErrnoIo;
            }
            std::cin.clear(std::cin.rdstate() & ~std::ios::failbit);
            return kErrnoSuccess;
        }

        auto* handle = find_file(fd);
        if (handle == nullptr || !handle->readable || handle->file == nullptr)
        {
            return kErrnoBadf;
        }

        size_t got = std::fread(dest, 1, length, handle->file);
        if (got < length && std::ferror(handle->file))
        {
            return kErrnoIo;
        }
        bytes_read = static_cast<uint32_t>(got);
        return kErrnoSuccess;
    }

    void register_fd_read()
    {
        interpreter_.register_host_function(
            "wasi_snapshot_preview1",
            "fd_read",
            {ValueType::I32, ValueType::I32, ValueType::I32, ValueType::I32},
            {ValueType::I32},
            [this](std::span<const Value> params) -> ExecutionResult {
                if (params.size() != 4)
                {
                    ExecutionResult result;
                    result.trapped = true;
                    result.trap_message = "wasi::fd_read expects 4 arguments";
                    return result;
                }
                auto mem = memory();
                if (mem.view.data == nullptr)
                {
                    return fault();
                }

                const int32_t fd = params[0].as<int32_t>();
                const uint32_t iovs_ptr = static_cast<uint32_t>(params[1].as<int32_t>());
                const uint32_t iovs_len = static_cast<uint32_t>(params[2].as<int32_t>());
                const uint32_t nread_ptr = static_cast<uint32_t>(params[3].as<int32_t>());

                uint64_t total_read = 0;
                for (uint32_t i = 0; i < iovs_len; ++i)
                {
                    const uint32_t entry_offset = iovs_ptr + i * 8U;
                    uint32_t buf_ptr = 0;
                    uint32_t buf_len = 0;
                    if (!mem.load_u32(entry_offset, buf_ptr) ||
                        !mem.load_u32(entry_offset + 4U, buf_len))
                    {
                        return fault();
                    }
                    if (buf_len == 0)
                    {
                        continue;
                    }
                    auto* dest = mem.view.data + buf_ptr;
                    if (static_cast<uint64_t>(buf_ptr) + buf_len > mem.view.size)
                    {
                        return fault();
                    }

                    uint32_t chunk = 0;
                    const auto err = read_fd(fd, dest, buf_len, chunk);
                    total_read += chunk;
                    if (err != kErrnoSuccess)
                    {
                        if (!mem.store_u32(nread_ptr, static_cast<uint32_t>(total_read)))
                        {
                            return fault();
                        }
                        return errno_result(err);
                    }
                    if (chunk < buf_len)
                    {
                        break;
                    }
                }

                if (!mem.store_u32(nread_ptr, static_cast<uint32_t>(total_read)))
                {
                    return fault();
                }
                return success();
            });
    }

    void register_fd_close()
    {
        interpreter_.register_host_function(
            "wasi_snapshot_preview1",
            "fd_close",
            {ValueType::I32},
            {ValueType::I32},
            [this](std::span<const Value> params) -> ExecutionResult {
                if (params.size() != 1)
                {
                    ExecutionResult result;
                    result.trapped = true;
                    result.trap_message = "wasi::fd_close expects 1 argument";
                    return result;
                }
                const int32_t fd = params[0].as<int32_t>();
                if (fd <= 2)
                {
                    return success();
                }
                auto it = files_.find(fd);
                if (it == files_.end())
                {
                    return errno_result(kErrnoBadf);
                }
                if (it->second.file != nullptr)
                {
                    std::fclose(it->second.file);
                }
                files_.erase(it);
                return success();
            });
    }

    void register_fd_seek()
    {
        interpreter_.register_host_function(
            "wasi_snapshot_preview1",
            "fd_seek",
            {ValueType::I32, ValueType::I64, ValueType::I32, ValueType::I32},
            {ValueType::I32},
            [this](std::span<const Value> params) -> ExecutionResult {
                if (params.size() != 4)
                {
                    ExecutionResult result;
                    result.trapped = true;
                    result.trap_message = "wasi::fd_seek expects 4 arguments";
                    return result;
                }
                auto mem = memory();
                if (mem.view.data == nullptr)
                {
                    return fault();
                }

                const int32_t fd = params[0].as<int32_t>();
                const int64_t offset = params[1].as<int64_t>();
                const int32_t whence = params[2].as<int32_t>();
                const uint32_t result_ptr = static_cast<uint32_t>(params[3].as<int32_t>());

                if (fd <= 2)
                {
                    return errno_result(kErrnoInval);
                }
                auto* handle = find_file(fd);
                if (handle == nullptr || handle->file == nullptr || !handle->seekable)
                {
                    return errno_result(kErrnoBadf);
                }

                int origin = SEEK_SET;
                switch (whence)
                {
                case 0:
                    origin = SEEK_SET;
                    break;
                case 1:
                    origin = SEEK_CUR;
                    break;
                case 2:
                    origin = SEEK_END;
                    break;
                default:
                    return errno_result(kErrnoInval);
                }

                if (std::fseek(handle->file, static_cast<long>(offset), origin) != 0)
                {
                    return errno_result(errno_from_host(errno));
                }
                long position = std::ftell(handle->file);
                if (position < 0)
                {
                    return errno_result(kErrnoIo);
                }
                if (!mem.store_u64(result_ptr, static_cast<uint64_t>(position)))
                {
                    return fault();
                }
                return success();
            });
    }

    void register_fd_fdstat_get()
    {
        interpreter_.register_host_function(
            "wasi_snapshot_preview1",
            "fd_fdstat_get",
            {ValueType::I32, ValueType::I32},
            {ValueType::I32},
            [this](std::span<const Value> params) -> ExecutionResult {
                if (params.size() != 2)
                {
                    ExecutionResult result;
                    result.trapped = true;
                    result.trap_message = "wasi::fd_fdstat_get expects 2 arguments";
                    return result;
                }
                auto mem = memory();
                if (mem.view.data == nullptr)
                {
                    return fault();
                }

                const int32_t fd = params[0].as<int32_t>();
                const uint32_t result_ptr = static_cast<uint32_t>(params[1].as<int32_t>());

                uint8_t filetype = kFiletypeUnknown;
                uint64_t rights_base = 0;
                uint64_t rights_inherit = 0;

                if (fd == 0)
                {
                    filetype = kFiletypeCharacterDevice;
                    rights_base = kRightFdRead;
                    rights_inherit = kRightFdRead;
                }
                else if (fd == 1 || fd == 2)
                {
                    filetype = kFiletypeCharacterDevice;
                    rights_base = kRightFdWrite;
                    rights_inherit = kRightFdWrite;
                }
                else if (fd == preopen_fd_)
                {
                    filetype = kFiletypeDirectory;
                    rights_base = kRightPathOpen;
                    rights_inherit = kRightPathOpen;
                }
                else
                {
                    auto* handle = find_file(fd);
                    if (handle == nullptr || handle->file == nullptr)
                    {
                        return errno_result(kErrnoBadf);
                    }
                    filetype = kFiletypeRegularFile;
                    rights_base = kRightFdRead | kRightFdSeek | kRightFdTell;
                    rights_inherit = rights_base;
                }

                std::array<uint8_t, 24> fdstat{};
                fdstat[0] = filetype;
                uint16_t flags = 0;
                std::memcpy(fdstat.data() + 2, &flags, sizeof(uint16_t));
                std::memcpy(fdstat.data() + 8, &rights_base, sizeof(uint64_t));
                std::memcpy(fdstat.data() + 16, &rights_inherit, sizeof(uint64_t));
                if (!mem.store_bytes(result_ptr,
                                     reinterpret_cast<const char*>(fdstat.data()),
                                     static_cast<uint32_t>(fdstat.size())))
                {
                    return fault();
                }
                return success();
            });
    }

    void register_fd_prestat_get()
    {
        interpreter_.register_host_function(
            "wasi_snapshot_preview1",
            "fd_prestat_get",
            {ValueType::I32, ValueType::I32},
            {ValueType::I32},
            [this](std::span<const Value> params) -> ExecutionResult {
                if (params.size() != 2)
                {
                    ExecutionResult result;
                    result.trapped = true;
                    result.trap_message = "wasi::fd_prestat_get expects 2 arguments";
                    return result;
                }
                auto mem = memory();
                if (mem.view.data == nullptr)
                {
                    return fault();
                }

                const int32_t fd = params[0].as<int32_t>();
                const uint32_t result_ptr = static_cast<uint32_t>(params[1].as<int32_t>());
                if (fd != preopen_fd_)
                {
                    return errno_result(kErrnoBadf);
                }

                std::array<uint8_t, 8> prestat{};
                prestat[0] = 0; // directory
                const uint32_t name_len = static_cast<uint32_t>(preopen_guest_path_.size());
                std::memcpy(prestat.data() + 4, &name_len, sizeof(uint32_t));
                if (!mem.store_bytes(result_ptr,
                                     reinterpret_cast<const char*>(prestat.data()),
                                     static_cast<uint32_t>(prestat.size())))
                {
                    return fault();
                }
                return success();
            });
    }

    void register_fd_prestat_dir_name()
    {
        interpreter_.register_host_function(
            "wasi_snapshot_preview1",
            "fd_prestat_dir_name",
            {ValueType::I32, ValueType::I32, ValueType::I32},
            {ValueType::I32},
            [this](std::span<const Value> params) -> ExecutionResult {
                if (params.size() != 3)
                {
                    ExecutionResult result;
                    result.trapped = true;
                    result.trap_message = "wasi::fd_prestat_dir_name expects 3 arguments";
                    return result;
                }
                auto mem = memory();
                if (mem.view.data == nullptr)
                {
                    return fault();
                }

                const int32_t fd = params[0].as<int32_t>();
                const uint32_t path_ptr = static_cast<uint32_t>(params[1].as<int32_t>());
                const uint32_t path_len = static_cast<uint32_t>(params[2].as<int32_t>());
                if (fd != preopen_fd_)
                {
                    return errno_result(kErrnoBadf);
                }
                const auto& name = preopen_guest_path_;
                if (path_len < name.size())
                {
                    return errno_result(kErrnoInval);
                }
                if (!mem.store_bytes(path_ptr, name.c_str(), static_cast<uint32_t>(name.size())))
                {
                    return fault();
                }
                return success();
            });
    }

    void register_path_open()
    {
        interpreter_.register_host_function(
            "wasi_snapshot_preview1",
            "path_open",
            {ValueType::I32,
             ValueType::I32,
             ValueType::I32,
             ValueType::I32,
             ValueType::I32,
             ValueType::I64,
             ValueType::I64,
             ValueType::I32,
             ValueType::I32},
            {ValueType::I32},
            [this](std::span<const Value> params) -> ExecutionResult {
                if (params.size() != 9)
                {
                    ExecutionResult result;
                    result.trapped = true;
                    result.trap_message = "wasi::path_open expects 9 arguments";
                    return result;
                }
                auto mem = memory();
                if (mem.view.data == nullptr)
                {
                    return fault();
                }

                const int32_t dirfd = params[0].as<int32_t>();
                const uint32_t path_ptr = static_cast<uint32_t>(params[2].as<int32_t>());
                const uint32_t path_len = static_cast<uint32_t>(params[3].as<int32_t>());
                const uint64_t rights_base = static_cast<uint64_t>(params[5].as<int64_t>());
                const uint32_t result_ptr = static_cast<uint32_t>(params[8].as<int32_t>());

                if (dirfd != preopen_fd_)
                {
                    return errno_result(kErrnoBadf);
                }

                const uint8_t* path_bytes = mem.data(path_ptr, path_len);
                if (path_bytes == nullptr)
                {
                    return fault();
                }
                std::string relative_path(reinterpret_cast<const char*>(path_bytes), path_len);
                while (!relative_path.empty() && (relative_path.front() == '/' || relative_path.front() == '\\'))
                {
                    relative_path.erase(relative_path.begin());
                }
                if (relative_path.empty())
                {
                    return errno_result(kErrnoIsdir);
                }

                if ((rights_base & kRightFdRead) == 0)
                {
                    return errno_result(kErrnoNotcapable);
                }

                std::filesystem::path resolved = preopen_host_path_ / std::filesystem::path(relative_path);
                std::error_code ec;
                auto canonical = std::filesystem::weakly_canonical(resolved, ec);
                if (ec)
                {
                    return errno_result(errno_from_host(errno));
                }
                const std::string canonical_string = canonical.string();
                if (canonical_string.compare(0, preopen_host_path_string_.size(), preopen_host_path_string_) != 0)
                {
                    return errno_result(kErrnoNotcapable);
                }

                std::FILE* file = std::fopen(canonical_string.c_str(), "rb");
                if (file == nullptr)
                {
                    return errno_result(errno_from_host(errno));
                }

                const int32_t fd = next_fd_++;
                files_[fd] = FileHandle{file, true, true};

                if (!mem.store_u32(result_ptr, static_cast<uint32_t>(fd)))
                {
                    std::fclose(file);
                    files_.erase(fd);
                    return fault();
                }
                return success();
            });
    }

    void register_args_sizes_get()
    {
        interpreter_.register_host_function(
            "wasi_snapshot_preview1",
            "args_sizes_get",
            {ValueType::I32, ValueType::I32},
            {ValueType::I32},
            [this](std::span<const Value> params) -> ExecutionResult {
                if (params.size() != 2)
                {
                    ExecutionResult result;
                    result.trapped = true;
                    result.trap_message = "wasi::args_sizes_get expects 2 arguments";
                    return result;
                }
                auto mem = memory();
                const uint32_t argc_ptr = static_cast<uint32_t>(params[0].as<int32_t>());
                const uint32_t buf_size_ptr = static_cast<uint32_t>(params[1].as<int32_t>());

                uint32_t total_size = 0;
                for (const auto& arg : args_)
                {
                    if (arg.size() >= std::numeric_limits<uint32_t>::max())
                    {
                        return success(kErrnoInval);
                    }
                    total_size += static_cast<uint32_t>(arg.size()) + 1U;
                }

                if (!mem.store_u32(argc_ptr, static_cast<uint32_t>(args_.size())))
                {
                    return fault();
                }
                if (!mem.store_u32(buf_size_ptr, total_size))
                {
                    return fault();
                }
                return success();
            });
    }

    void register_args_get()
    {
        interpreter_.register_host_function(
            "wasi_snapshot_preview1",
            "args_get",
            {ValueType::I32, ValueType::I32},
            {ValueType::I32},
            [this](std::span<const Value> params) -> ExecutionResult {
                if (params.size() != 2)
                {
                    ExecutionResult result;
                    result.trapped = true;
                    result.trap_message = "wasi::args_get expects 2 arguments";
                    return result;
                }
                auto mem = memory();
                auto mem_view = interpreter_.memory();
                if (mem_view.data == nullptr)
                {
                    return fault();
                }
                const uint32_t argv_ptr = static_cast<uint32_t>(params[0].as<int32_t>());
                uint32_t buf_ptr = static_cast<uint32_t>(params[1].as<int32_t>());

                for (size_t i = 0; i < args_.size(); ++i)
                {
                    const auto& arg = args_[i];
                    if (!mem.store_u32(argv_ptr + static_cast<uint32_t>(i * sizeof(uint32_t)), buf_ptr))
                    {
                        return fault();
                    }
                    const uint32_t length = static_cast<uint32_t>(arg.size());
                    if (!mem.store_bytes(buf_ptr, arg.c_str(), length))
                    {
                        return fault();
                    }
                    if (!mem.store_bytes(buf_ptr + length, "\0", 1))
                    {
                        return fault();
                    }
                    buf_ptr += length + 1U;
                }
                return success();
            });
    }

    void register_environ_sizes_get()
    {
        interpreter_.register_host_function(
            "wasi_snapshot_preview1",
            "environ_sizes_get",
            {ValueType::I32, ValueType::I32},
            {ValueType::I32},
            [this](std::span<const Value> params) -> ExecutionResult {
                if (params.size() != 2)
                {
                    ExecutionResult result;
                    result.trapped = true;
                    result.trap_message = "wasi::environ_sizes_get expects 2 arguments";
                    return result;
                }
                auto mem = memory();
                const uint32_t count_ptr = static_cast<uint32_t>(params[0].as<int32_t>());
                const uint32_t buf_size_ptr = static_cast<uint32_t>(params[1].as<int32_t>());

                uint32_t total_size = 0;
                for (const auto& entry : env_)
                {
                    if (entry.size() >= std::numeric_limits<uint32_t>::max())
                    {
                        return success(kErrnoInval);
                    }
                    total_size += static_cast<uint32_t>(entry.size()) + 1U;
                }

                if (!mem.store_u32(count_ptr, static_cast<uint32_t>(env_.size())))
                {
                    return fault();
                }
                if (!mem.store_u32(buf_size_ptr, total_size))
                {
                    return fault();
                }
                return success();
            });
    }

    void register_environ_get()
    {
        interpreter_.register_host_function(
            "wasi_snapshot_preview1",
            "environ_get",
            {ValueType::I32, ValueType::I32},
            {ValueType::I32},
            [this](std::span<const Value> params) -> ExecutionResult {
                if (params.size() != 2)
                {
                    ExecutionResult result;
                    result.trapped = true;
                    result.trap_message = "wasi::environ_get expects 2 arguments";
                    return result;
                }
                auto mem = memory();
                auto mem_view = interpreter_.memory();
                if (mem_view.data == nullptr)
                {
                    return fault();
                }
                const uint32_t env_ptr = static_cast<uint32_t>(params[0].as<int32_t>());
                uint32_t buf_ptr = static_cast<uint32_t>(params[1].as<int32_t>());

                for (size_t i = 0; i < env_.size(); ++i)
                {
                    const auto& entry = env_[i];
                    if (!mem.store_u32(env_ptr + static_cast<uint32_t>(i * sizeof(uint32_t)), buf_ptr))
                    {
                        return fault();
                    }
                    const uint32_t length = static_cast<uint32_t>(entry.size());
                    if (!mem.store_bytes(buf_ptr, entry.c_str(), length))
                    {
                        return fault();
                    }
                    if (!mem.store_bytes(buf_ptr + length, "\0", 1))
                    {
                        return fault();
                    }
                    buf_ptr += length + 1U;
                }
                return success();
            });
    }

    void register_clock_time_get()
    {
        interpreter_.register_host_function(
            "wasi_snapshot_preview1",
            "clock_time_get",
            {ValueType::I32, ValueType::I64, ValueType::I32},
            {ValueType::I32},
            [this](std::span<const Value> params) -> ExecutionResult {
                if (params.size() != 3)
                {
                    ExecutionResult result;
                    result.trapped = true;
                    result.trap_message = "wasi::clock_time_get expects 3 arguments";
                    return result;
                }
                auto mem = memory();
                const auto clock_id = params[0].as<int32_t>();
                const uint32_t result_ptr = static_cast<uint32_t>(params[2].as<int32_t>());

                uint64_t timestamp = 0;
                switch (clock_id)
                {
                case 0: // realtime
                {
                    const auto now = std::chrono::system_clock::now().time_since_epoch();
                    timestamp = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
                    break;
                }
                case 1: // monotonic
                {
                    const auto now = std::chrono::steady_clock::now().time_since_epoch();
                    timestamp = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
                    break;
                }
                default:
                    return success(kErrnoNosys);
                }

                if (!mem.store_u64(result_ptr, timestamp))
                {
                    return fault();
                }
                return success();
            });
    }

    void register_random_get()
    {
        interpreter_.register_host_function(
            "wasi_snapshot_preview1",
            "random_get",
            {ValueType::I32, ValueType::I32},
            {ValueType::I32},
            [this](std::span<const Value> params) -> ExecutionResult {
                if (params.size() != 2)
                {
                    ExecutionResult result;
                    result.trapped = true;
                    result.trap_message = "wasi::random_get expects 2 arguments";
                    return result;
                }
                auto mem_view = interpreter_.memory();
                if (mem_view.data == nullptr)
                {
                    return fault();
                }
                const uint32_t buf_ptr = static_cast<uint32_t>(params[0].as<int32_t>());
                const uint32_t buf_len = static_cast<uint32_t>(params[1].as<int32_t>());
                const uint64_t end = static_cast<uint64_t>(buf_ptr) + buf_len;
                if (end > mem_view.size)
                {
                    return fault();
                }

                std::mt19937_64 rng(std::random_device{}());
                uint32_t offset = 0;
                while (offset < buf_len)
                {
                    const auto value = rng();
                    const uint32_t remaining = buf_len - offset;
                    const uint32_t chunk = std::min<uint32_t>(remaining, sizeof(value));
                    std::memcpy(mem_view.data + buf_ptr + offset, &value, chunk);
                    offset += chunk;
                }
                return success();
            });
    }

    Interpreter& interpreter_;
    std::vector<std::string> args_;
    std::vector<std::string> env_;
    std::unordered_map<int32_t, FileHandle> files_;
    int32_t next_fd_{4};
    const int32_t preopen_fd_{3};
    std::filesystem::path preopen_host_path_;
    std::string preopen_host_path_string_;
    std::string preopen_guest_path_{ "." };
};

void validate_arguments(const FunctionType& signature, const std::vector<Value>& args)
{
    if (signature.params.size() != args.size())
    {
        std::ostringstream oss;
        oss << "argument count mismatch: function expects " << signature.params.size() << " value(s) but "
            << args.size() << " provided";
        throw std::runtime_error(oss.str());
    }
    for (size_t i = 0; i < signature.params.size(); ++i)
    {
        if (signature.params[i] != args[i].type)
        {
            std::ostringstream oss;
            oss << "argument " << i << " type mismatch: expected " << wasm::to_string(signature.params[i]) << " but got "
                << wasm::to_string(args[i].type);
            throw std::runtime_error(oss.str());
        }
    }
}

} // namespace

int main(int argc, char** argv)
{
    try
    {
        const auto options = parse_options(argc, argv);
        const auto absolute_path = std::filesystem::absolute(options.module_path);
        const auto wasm_bytes = wasm::read_file(absolute_path);
        const auto module = wasm::parse_module(wasm_bytes);
        ModuleIntrospector introspector(module);

        if (options.list_imports)
        {
            print_imports(module, introspector);
        }
        if (options.list_exports)
        {
            print_exports(module, introspector);
        }

        std::string export_to_invoke;
        if (!options.skip_invoke)
        {
            if (options.export_name)
            {
                export_to_invoke = *options.export_name;
            }
            else
            {
                export_to_invoke = detect_default_export(module);
                if (export_to_invoke.empty())
                {
                    throw std::runtime_error("module exports no functions; specify --invoke to run a specific export");
                }
            }
        }

        if (!options.skip_invoke)
        {
            const auto signature_opt = introspector.export_function_type(export_to_invoke);
            if (!signature_opt)
            {
                throw std::runtime_error("export '" + export_to_invoke + "' is not a function");
            }
            validate_arguments(*signature_opt, options.call_args);
        }

        Interpreter interpreter;
        WasiPreview1Host wasi_host(interpreter, options.wasi_args, options.wasi_env);
        wasi_host.register_all();
        interpreter.load(wasm_bytes);

        if (options.skip_invoke)
        {
            return EXIT_SUCCESS;
        }

        const auto signature_opt = introspector.export_function_type(export_to_invoke);
        if (!signature_opt)
        {
            throw std::runtime_error("export '" + export_to_invoke + "' is not callable");
        }

        const auto result = interpreter.invoke(export_to_invoke, std::span<const Value>(options.call_args));
        if (result.trapped)
        {
            if (const auto exit_code = parse_proc_exit_trap(result.trap_message))
            {
                return *exit_code;
            }

            std::cerr << "execution trapped: " << result.trap_message << '\n';
            return EXIT_FAILURE;
        }

        if (!result.values.empty())
        {
            std::cout << "Returned " << result.values.size() << " value(s):\n";
            for (size_t i = 0; i < result.values.size(); ++i)
            {
                std::cout << "  [" << i << "] " << value_to_string(result.values[i]) << '\n';
            }
        }
        return EXIT_SUCCESS;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "error: " << ex.what() << '\n';
        return EXIT_FAILURE;
    }
}
