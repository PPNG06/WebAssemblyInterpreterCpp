#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "wasm/types.hpp"
#include "wasm/module.hpp"

namespace wasm
{
class Module;

struct ExecutionResult
{
    bool trapped{false};
    std::string trap_message;
    std::vector<Value> values;
};

struct MemoryView
{
    uint8_t* data{nullptr};
    size_t size{0};
};

class Interpreter
{
public:
    using HostFunction = std::function<ExecutionResult(std::span<const Value>)>;

    Interpreter();
    ~Interpreter();

    Interpreter(const Interpreter&) = delete;
    Interpreter& operator=(const Interpreter&) = delete;
    Interpreter(Interpreter&&) noexcept;
    Interpreter& operator=(Interpreter&&) noexcept;

    void load(const std::vector<uint8_t>& wasm_binary);
    void load(std::span<const uint8_t> wasm_binary);

    [[nodiscard]] ExecutionResult invoke(std::string_view export_name,
                                         std::span<const Value> args = {});

    [[nodiscard]] MemoryView memory();

    [[nodiscard]] const Module& module() const;

    void register_host_function(const std::string& module,
                                const std::string& name,
                                std::vector<ValueType> params,
                                std::vector<ValueType> results,
                                HostFunction callback);

    void register_host_memory(const std::string& module,
                              const std::string& name,
                              MemoryType type,
                              std::vector<uint8_t> data = {});

    void register_host_table(const std::string& module,
                             const std::string& name,
                             TableType type,
                             std::vector<Value> elements = {});

    void register_host_global(const std::string& module,
                              const std::string& name,
                              GlobalType type,
                              Value value);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

std::vector<uint8_t> read_file(const std::string& path);

} // namespace wasm
