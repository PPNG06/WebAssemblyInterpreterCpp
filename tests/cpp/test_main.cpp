#include "wasm/interpreter.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#ifndef WASM_INTERP_SOURCE_DIR
#error "WASM_INTERP_SOURCE_DIR must be defined"
#endif

#ifndef WASM_INTERP_BINARY_DIR
#error "WASM_INTERP_BINARY_DIR must be defined"
#endif

namespace
{
int32_t load_i32(const wasm::MemoryView& memory, uint32_t address)
{
    if (memory.data == nullptr || address + sizeof(int32_t) > memory.size)
    {
        throw std::runtime_error("Memory access out of bounds when reading result");
    }
    int32_t value = 0;
    std::memcpy(&value, memory.data + address, sizeof(int32_t));
    return value;
}

constexpr int32_t f32_bits(float value)
{
    return static_cast<int32_t>(std::bit_cast<uint32_t>(value));
}

constexpr int32_t f64_lower32(double value)
{
    return static_cast<int32_t>(std::bit_cast<uint64_t>(value) & 0xFFFFFFFFu);
}

struct TestCase
{
    std::string export_name;
    uint32_t address{0};
    int32_t expected{0};
};

struct ModuleInfo
{
    std::string name;
    std::string wat;
    std::string wasm;
    std::vector<TestCase> cases;
    bool sequential{false};
};

const std::vector<ModuleInfo> kModules = {
    {"01_test",
     "01_test.wat",
     "01_test.wasm",
     {
         {"_test_store", 0, 42},
         {"_test_addition", 0, 15},
         {"_test_shift_right_signed", 0, -4},
         {"_test_select_true", 0, 10},
         {"_test_if_false", 0, 200},
         {"_test_loop_sum", 0, 15},
         {"_test_br_table_case0", 0, 100},
         {"_test_rotl", 0, 16},
         {"_test_global_increment", 0, 1},
         {"_test_load16_32768", 0, 32768},
     }},
    {"02_test_prio1",
     "02_test_prio1.wat",
     "02_test_prio1.wasm",
     {
         {"_test_call_add", 0, 15},
         {"_test_return_early_true", 0, 100},
         {"_test_abs_negative", 0, 42},
         {"_test_factorial", 0, 120},
         {"_test_f32_add", 0, f32_bits(6.0f)},
         {"_test_f32_nearest", 0, f32_bits(4.0f)},
         {"_test_f64_mul", 0, f64_lower32(10.0)},
         {"_test_convert_f32_to_i32_u", 0, 42},
         {"_test_memory_grow", 0, 1},
         {"_test_drop_multiple", 0, 100},
     }},
    {"03_test_prio2",
     "03_test_prio2.wat",
     "03_test_prio2.wasm",
     {
         {"_test_data_read_char_h", 200, 72},
         {"_test_data_read_char_e", 200, 101},
         {"_test_call_indirect_mul", 200, 50},
         {"_test_i64_add", 200, 15},
         {"_test_i64_mul", 200, 42},
         {"_test_i64_rem_s", 200, 2},
         {"_test_i64_convert_to_f64", 200, 0},
         {"_test_i64_load32_s", 200, -2147483648},
         {"_test_i64_large_mul", 200, 1000000000},
         {"_test_combined_all_features", 200, 114},
     }},
    {"04_test_prio3",
     "04_test_prio3.wat",
     "04_test_prio3.wasm",
     {
         {"_test_i32_rem_u", 0, 2},
         {"_test_i64_rem_u_large", 0, 1},
         {"_test_i32_le_u_large", 0, 0},
         {"_test_i32_ge_u_large", 0, 1},
         {"_test_f32_copysign_neg", 0, f32_bits(-3.5f)},
         {"_test_f64_sub", 0, f64_lower32(7.0)},
         {"_test_f32_store_load", 0, f32_bits(3.14159f)},
         {"_test_f64_store_load", 0, f64_lower32(2.718281828)},
         {"_test_f32_arithmetic_with_load", 0, f32_bits(8.0f)},
         {"_test_unreachable_not_reached", 0, 42},
     }},
    {"05_test_complex",
     "05_test_complex.wat",
     "05_test_complex.wasm",
     {
         {"nested_blocks", 0, 42},
         {"block_results", 0, 50},
         {"conditional_nested_0", 0, 100},
         {"conditional_nested_1", 0, 200},
         {"conditional_nested_2", 0, 300},
         {"call_in_block", 0, 42},
         {"loop_with_blocks", 0, 5},
         {"multi_call", 0, 30},
     }},
    {"06_test_fc",
     "06_test_fc.wat",
     "06_test_fc.wasm",
     {
         {"_test_i32_trunc_sat_f32_s_normal", 0, 10},
         {"_test_i32_trunc_sat_f32_s_negative", 0, -5},
         {"_test_i32_trunc_sat_f32_s_nan", 0, 0},
         {"_test_i32_trunc_sat_f32_s_overflow", 0, 2147483647},
         {"_test_i32_trunc_sat_f32_s_underflow", 0, -2147483648},
         {"_test_i32_trunc_sat_f32_u_normal", 0, 42},
         {"_test_i32_trunc_sat_f32_u_nan", 0, 0},
         {"_test_i32_trunc_sat_f32_u_negative", 0, 0},
         {"_test_i32_trunc_sat_f32_u_overflow", 0, -1},
         {"_test_i32_trunc_sat_f64_s_normal", 0, 123},
         {"_test_i32_trunc_sat_f64_s_negative", 0, -99},
         {"_test_i32_trunc_sat_f64_s_nan", 0, 0},
         {"_test_i32_trunc_sat_f64_s_overflow", 0, 2147483647},
         {"_test_i32_trunc_sat_f64_s_underflow", 0, -2147483648},
         {"_test_i32_trunc_sat_f64_u_normal", 0, 255},
         {"_test_i32_trunc_sat_f64_u_nan", 0, 0},
         {"_test_i32_trunc_sat_f64_u_negative", 0, 0},
         {"_test_i32_trunc_sat_f64_u_overflow", 0, -1},
         {"_test_i64_trunc_sat_f32_s_normal", 0, 42},
         {"_test_i64_trunc_sat_f32_s_negative", 0, -7},
         {"_test_i64_trunc_sat_f32_s_nan", 0, 0},
         {"_test_i64_trunc_sat_f32_u_normal", 0, 100},
         {"_test_i64_trunc_sat_f32_u_nan", 0, 0},
         {"_test_i64_trunc_sat_f32_u_negative", 0, 0},
         {"_test_i64_trunc_sat_f64_s_normal", 0, 1234},
         {"_test_i64_trunc_sat_f64_s_negative", 0, -500},
         {"_test_i64_trunc_sat_f64_s_nan", 0, 0},
         {"_test_i64_trunc_sat_f64_u_normal", 0, 9999},
         {"_test_i64_trunc_sat_f64_u_nan", 0, 0},
         {"_test_i64_trunc_sat_f64_u_negative", 0, 0},
         {"_test_zero_f32", 0, 0},
         {"_test_small_f32", 0, 0},
         {"_test_negzero_f64", 0, 0},
         {"_test_large_in_range", 0, 1000000},
     }},
    {"07_test_bulk_memory",
     "07_test_bulk_memory.wat",
     "07_test_bulk_memory.wasm",
     {
         {"_test_fill_basic", 0, 42},
         {"_test_fill_range", 0, 99},
         {"_test_fill_single", 0, 77},
         {"_test_fill_zero", 0, 0},
         {"_test_copy_basic", 0, 1819043144},
         {"_test_copy_single", 0, 65},
         {"_test_copy_block", 0, 170},
         {"_test_copy_overlapping", 0, 1},
         {"_test_init_basic", 0, 72},
         {"_test_init_partial", 0, 87},
         {"_test_init_segment1", 0, 3},
         {"_test_drop_after_use", 0, 72},
         {"_test_combined_fill_copy", 0, 55},
         {"_test_combined_init_copy", 0, 72},
         {"_test_zero_length", 0, 123},
     }},
    {"08_test_post_mvp",
     "08_test_post_mvp.wat",
     "08_test_post_mvp.wasm",
     {
         {"_test_multiret_two", 3000, 42},
         {"_test_multiret_two", 3004, 100},
         {"_test_multiret_three", 3000, 10},
         {"_test_multiret_three", 3004, 20},
         {"_test_multiret_three", 3008, 30},
         {"_test_multiret_swap", 3000, 20},
         {"_test_multiret_swap", 3004, 10},
         {"_test_multiret_divmod", 3000, 3},
         {"_test_multiret_divmod", 3004, 2},
         {"_test_multiret_minmax", 3000, 7},
         {"_test_multiret_minmax", 3004, 15},
         {"_test_multiret_chain", 3000, 100},
         {"_test_multiret_chain", 3004, 42},
         {"_test_multiret_discard", 3000, 42},
         {"_test_bulk_copy_verify_first", 3000, 65},
         {"_test_bulk_copy_verify_third", 3000, 67},
         {"_test_bulk_fill_verify", 3000, 255},
         {"_test_bulk_fill_verify_middle", 3000, 255},
         {"_test_bulk_fill_different", 3000, 0x42},
         {"_test_bulk_copy_overlap", 3000, 1},
         {"_test_bulk_copy_string", 3000, 90},
         {"_test_bulk_fill_range", 3000, 0xAB},
         {"_test_bulk_copy_modify", 3000, 67},
         {"_test_ref_null_func", 3000, 1},
         {"_test_ref_null_extern", 3000, 1},
         {"_test_ref_func_not_null", 3000, 0},
         {"_test_ref_global_store", 3000, 0},
         {"_test_ref_table_set_get", 3000, 0},
         {"_test_ref_table_get_null", 3000, 1},
         {"_test_ref_table_size", 3000, 8},
         {"_test_ref_table_grow", 3000, 8},
         {"_test_ref_table_size_after", 3000, 10},
         {"_test_ref_table_fill", 3000, 1},
         {"_test_ref_table_copy", 3000, 0},
         {"_test_ref_externref_global", 3000, 1},
         {"_test_ref_externref_store", 3000, 1},
         {"_test_ref_externref_table_size", 3000, 4},
         {"_test_combined_multiret_bulk", 3000, 65},
         {"_test_combined_multiret_bulk", 3004, 66},
         {"_test_combined_table_multiret", 3000, 10},
         {"_test_combined_table_multiret", 3004, 1},
         {"_test_combined_fill_copy", 3000, 0x77},
         {"_test_combined_fill_copy", 3004, 0x77},
         {"_test_combined_ref_sizes", 3000, 14},
         {"_test_combined_swap_bulk", 3000, 66},
         {"_test_combined_swap_bulk", 3004, 65},
         {"_test_combined_bulk_pattern", 3000, 1},
         {"_test_combined_table_results", 3000, 10},
         {"_test_combined_table_results", 3004, 0},
     },
     true},
};

const ModuleInfo* find_module(const std::string& name)
{
    auto it = std::find_if(kModules.begin(), kModules.end(), [&](const ModuleInfo& module) {
        return module.name == name;
    });
    if (it == kModules.end())
    {
        return nullptr;
    }
    return &(*it);
}

const TestCase* find_case(const ModuleInfo& module, const std::string& export_name)
{
    auto it = std::find_if(module.cases.begin(), module.cases.end(), [&](const TestCase& test_case) {
        return test_case.export_name == export_name;
    });
    if (it == module.cases.end())
    {
        return nullptr;
    }
    return &(*it);
}

struct RunSummary
{
    int total_runs{0};
    int total_failures{0};
};

bool execute_test_case(const ModuleInfo& module,
                       const TestCase& test_case,
                       wasm::Interpreter& interpreter,
                       bool log_pass = true)
{
    auto result = interpreter.invoke(test_case.export_name);
    if (result.trapped)
    {
        std::cerr << "[FAIL] (" << module.name << ") " << test_case.export_name
                  << ": trapped with message: " << result.trap_message << "\n";
        return false;
    }

    if (module.name == "08_test_post_mvp" && !result.values.empty())
    {
        std::cout << "    return values:";
        for (const auto& value : result.values)
        {
            std::cout << ' ' << wasm::to_string(value.type) << '=';
            switch (value.type)
            {
            case wasm::ValueType::I32:
                std::cout << value.as<int32_t>();
                break;
            case wasm::ValueType::I64:
                std::cout << value.as<int64_t>();
                break;
            case wasm::ValueType::F32:
                std::cout << value.as<float>();
                break;
            case wasm::ValueType::F64:
                std::cout << value.as<double>();
                break;
            case wasm::ValueType::FuncRef:
                std::cout << (value.is_null_ref() ? "null" : std::to_string(value.funcref_index()));
                break;
            case wasm::ValueType::ExternRef:
                std::cout << (value.is_null_ref() ? "null" : "extern");
                break;
            }
        }
        std::cout << "\n";
    }

    auto memory = interpreter.memory();
    auto actual = load_i32(memory, test_case.address);
    if (actual != test_case.expected)
    {
        std::cerr << "[FAIL] (" << module.name << ") " << test_case.export_name << ": expected "
                  << test_case.expected << " at address " << test_case.address << ", got " << actual
                  << "\n";
        return false;
    }

    if (log_pass)
    {
        std::cout << "[PASS] (" << module.name << ") " << test_case.export_name << " -> " << actual
                  << "\n";
    }

    return true;
}

bool run_test_case(const ModuleInfo& module, const TestCase& test_case, const std::vector<uint8_t>& wasm_bytes)
{
    wasm::Interpreter interpreter;
    interpreter.load(wasm_bytes);
    return execute_test_case(module, test_case, interpreter);
}

RunSummary run_module_tests(const ModuleInfo& module, const std::optional<std::string>& case_filter)
{
    RunSummary summary;
    if (case_filter)
    {
        std::cout << "Running module " << module.name << " (filter: " << *case_filter << ")..." << std::endl;
    }
    else
    {
        std::cout << "Running module " << module.name << "..." << std::endl;
    }

    const std::string wasm_path = std::string(WASM_INTERP_BINARY_DIR) + "/generated_wasm/" + module.wasm;
    auto wasm_bytes = wasm::read_file(wasm_path);

    if (module.sequential)
    {
        wasm::Interpreter interpreter;
        interpreter.load(wasm_bytes);

        bool found_target = !case_filter.has_value();
        for (const auto& test_case : module.cases)
        {
            const bool is_target = !case_filter || *case_filter == test_case.export_name;
            const bool log_pass = !case_filter || is_target;
            bool success = execute_test_case(module, test_case, interpreter, log_pass);

            if (is_target || !case_filter)
            {
                ++summary.total_runs;
                if (!success)
                {
                    ++summary.total_failures;
                }
            }
            else if (!success)
            {
                ++summary.total_failures;
            }

            if (is_target && case_filter)
            {
                found_target = true;
                break;
            }
        }
        if (case_filter && !found_target)
        {
            std::cerr << "Unknown test case: " << module.name << "." << *case_filter << "\n";
            summary.total_failures += 1;
        }
    }
    else
    {
        for (const auto& test_case : module.cases)
        {
            if (case_filter && *case_filter != test_case.export_name)
            {
                continue;
            }

            ++summary.total_runs;
            if (!run_test_case(module, test_case, wasm_bytes))
            {
                ++summary.total_failures;
            }
        }
    }

    return summary;
}

void list_available_tests()
{
    for (const auto& module : kModules)
    {
        for (const auto& test_case : module.cases)
        {
            std::cout << module.name << "." << test_case.export_name << "\n";
        }
    }
}

std::optional<std::pair<std::string, std::optional<std::string>>> parse_module_case(const std::string& spec)
{
    if (spec.empty())
    {
        return std::nullopt;
    }

    const auto dot_pos = spec.find('.');
    if (dot_pos == std::string::npos)
    {
        return std::make_pair(spec, std::optional<std::string>{});
    }

    auto module = spec.substr(0, dot_pos);
    auto test = spec.substr(dot_pos + 1);
    if (module.empty() || test.empty())
    {
        return std::nullopt;
    }
    return std::make_pair(module, std::optional<std::string>{test});
}

void print_usage(const std::string& program_name)
{
    std::cerr << "Usage: " << program_name << " [module [case]]\n"
              << "       " << program_name << " module.case\n"
              << "       " << program_name << " --list\n";
}

} // namespace

int main(int argc, char** argv)
try
{
    const std::string program_name = (argc > 0 && argv ? argv[0] : "wasm_interp_tests");
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i)
    {
        args.emplace_back(argv[i]);
    }

    if (!args.empty() && args[0] == "--list")
    {
        if (args.size() != 1)
        {
            print_usage(program_name);
            return 1;
        }
        list_available_tests();
        return 0;
    }

    std::optional<std::string> module_name;
    std::optional<std::string> case_name;

    if (args.size() == 1)
    {
        auto parsed = parse_module_case(args[0]);
        if (!parsed)
        {
            print_usage(program_name);
            return 1;
        }
        module_name = parsed->first;
        case_name = parsed->second;
    }
    else if (args.size() == 2)
    {
        module_name = args[0];
        case_name = args[1];
    }
    else if (!args.empty())
    {
        print_usage(program_name);
        return 1;
    }

    RunSummary summary;

    if (module_name)
    {
        const ModuleInfo* module = find_module(*module_name);
        if (module == nullptr)
        {
            std::cerr << "Unknown module: " << *module_name << "\n";
            return 1;
        }

        if (case_name && !case_name->empty() && !find_case(*module, *case_name))
        {
            std::cerr << "Unknown test case: " << *module_name << "." << *case_name << "\n";
            return 1;
        }

        summary = run_module_tests(*module, case_name);
        if (summary.total_runs == 0)
        {
            std::cerr << "No tests executed for module " << *module_name << "\n";
            return 1;
        }
    }
    else
    {
        for (const auto& module : kModules)
        {
            auto module_summary = run_module_tests(module, std::nullopt);
            summary.total_runs += module_summary.total_runs;
            summary.total_failures += module_summary.total_failures;
        }
    }

    if (summary.total_runs > 0)
    {
        if (summary.total_failures == 0)
        {
            if (!module_name)
            {
                std::cout << "All module tests passed." << std::endl;
            }
        }
        else
        {
            std::cerr << summary.total_failures << " test(s) failed\n";
        }
    }

    return summary.total_failures == 0 ? 0 : 1;
}
catch (const std::exception& ex)
{
    std::cerr << "Unhandled exception: " << ex.what() << "\n";
    return 1;
}
