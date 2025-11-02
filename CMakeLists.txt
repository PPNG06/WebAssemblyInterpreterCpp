cmake_minimum_required(VERSION 3.20)

project(WasmInterpCpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(WASM_INTERP_ENABLE_TESTS "Build the interpreter test suite" ON)
option(WASM_INTERP_USE_BUNDLED_WABT "Build the vendored WABT to obtain wat2wasm" ON)

set(WAT2WASM_EXECUTABLE "" CACHE FILEPATH "Path to an existing wat2wasm executable")

add_library(wasm_interp STATIC
    src/interpreter.cpp
    src/module_loader.cpp
)

target_include_directories(wasm_interp
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(wasm_interp PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(wasm_interp PRIVATE /W4 /permissive- /Zc:preprocessor /EHsc)
else()
    target_compile_options(wasm_interp PRIVATE -Wall -Wextra -Wpedantic)
endif()

set(WAT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/wat/01_test.wat
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/wat/02_test_prio1.wat
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/wat/03_test_prio2.wat
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/wat/04_test_prio3.wat
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/wat/05_test_complex.wat
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/wat/06_test_fc.wat
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/wat/07_test_bulk_memory.wat
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/wat/08_test_post_mvp.wat
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/custom_tests/custom_imports.wat
)

set(GENERATED_WASM_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated_wasm)
file(MAKE_DIRECTORY ${GENERATED_WASM_DIR})

set(WAT2WASM_COMMAND "")
set(WAT2WASM_TARGET "")

if(WAT2WASM_EXECUTABLE)
    set(WAT2WASM_COMMAND ${WAT2WASM_EXECUTABLE})
elseif(WASM_INTERP_USE_BUNDLED_WABT AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/wabt/CMakeLists.txt")
    set(HAVE_SETJMP_H OFF CACHE BOOL "Disable WABT wasm2c runtime build" FORCE)
    set(BUILD_TESTS OFF CACHE BOOL "Disable WABT gtest suite" FORCE)
    set(BUILD_FUZZ_TOOLS OFF CACHE BOOL "Disable WABT fuzz tooling" FORCE)
    set(WABT_INSTALL_RULES OFF CACHE BOOL "Disable WABT installation rules" FORCE)
    set(WERROR OFF CACHE BOOL "Do not treat WABT warnings as errors" FORCE)
    add_subdirectory(wabt EXCLUDE_FROM_ALL)
    if(TARGET wat2wasm)
        set(WAT2WASM_COMMAND $<TARGET_FILE:wat2wasm>)
        set(WAT2WASM_TARGET wat2wasm)
    endif()
endif()

if(NOT WAT2WASM_COMMAND)
    set(WAT2WASM_CANDIDATES "")
    if(WIN32)
        list(APPEND WAT2WASM_CANDIDATES
            ${CMAKE_CURRENT_SOURCE_DIR}/wabt/build/wat2wasm.exe
            ${CMAKE_CURRENT_SOURCE_DIR}/wabt/bin/wat2wasm.exe
        )
    else()
        list(APPEND WAT2WASM_CANDIDATES
            ${CMAKE_CURRENT_SOURCE_DIR}/wabt/build/wat2wasm
            ${CMAKE_CURRENT_SOURCE_DIR}/wabt/bin/wat2wasm
        )
    endif()

    foreach(candidate IN LISTS WAT2WASM_CANDIDATES)
        if(EXISTS ${candidate})
            set(WAT2WASM_COMMAND ${candidate})
            break()
        endif()
    endforeach()
endif()

if(NOT WAT2WASM_COMMAND)
    message(WARNING "wat2wasm not found; generated wasm artifacts will be skipped")
endif()

set(GENERATED_WASM_FILES)
if(WAT2WASM_COMMAND)
    foreach(wat IN LISTS WAT_SOURCES)
        get_filename_component(name ${wat} NAME_WE)
        set(wasm ${GENERATED_WASM_DIR}/${name}.wasm)
        set(_wat_deps ${wat})
        if(WAT2WASM_TARGET)
            list(APPEND _wat_deps ${WAT2WASM_TARGET})
        endif()
        add_custom_command(
            OUTPUT ${wasm}
            COMMAND ${WAT2WASM_COMMAND} ${wat} -o ${wasm}
            DEPENDS ${_wat_deps}
            COMMENT "Generating ${name}.wasm from ${wat}"
            VERBATIM
        )
        list(APPEND GENERATED_WASM_FILES ${wasm})
    endforeach()

    add_custom_target(generate_wasm ALL DEPENDS ${GENERATED_WASM_FILES})
    if(WAT2WASM_TARGET)
        add_dependencies(generate_wasm ${WAT2WASM_TARGET})
    endif()
endif()

if(WASM_INTERP_ENABLE_TESTS)
    enable_testing()
    add_executable(wasm_interp_tests
        tests/cpp/test_main.cpp
    )
    target_link_libraries(wasm_interp_tests PRIVATE wasm_interp)
    target_compile_definitions(wasm_interp_tests
        PRIVATE
            WASM_INTERP_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
            WASM_INTERP_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}"
    )
    if(TARGET generate_wasm)
        add_dependencies(wasm_interp_tests generate_wasm)
    endif()
    if(MSVC)
        target_compile_options(wasm_interp_tests PRIVATE /W4 /permissive- /Zc:preprocessor /EHsc)
    else()
        target_compile_options(wasm_interp_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()

set(WASM_INTERP_MODULES
    01_test
    02_test_prio1
    03_test_prio2
    04_test_prio3
    05_test_complex
    06_test_fc
    07_test_bulk_memory
    08_test_post_mvp
    custom_imports
)

    set(WASM_INTERP_TEST_CASES
        "01_test:_test_store"
        "01_test:_test_addition"
        "01_test:_test_shift_right_signed"
        "01_test:_test_select_true"
        "01_test:_test_if_false"
        "01_test:_test_loop_sum"
        "01_test:_test_br_table_case0"
        "01_test:_test_rotl"
        "01_test:_test_global_increment"
        "01_test:_test_load16_32768"
        "02_test_prio1:_test_call_add"
        "02_test_prio1:_test_return_early_true"
        "02_test_prio1:_test_abs_negative"
        "02_test_prio1:_test_factorial"
        "02_test_prio1:_test_f32_add"
        "02_test_prio1:_test_f32_nearest"
        "02_test_prio1:_test_f64_mul"
        "02_test_prio1:_test_convert_f32_to_i32_u"
        "02_test_prio1:_test_memory_grow"
        "02_test_prio1:_test_drop_multiple"
        "03_test_prio2:_test_data_read_char_h"
        "03_test_prio2:_test_data_read_char_e"
        "03_test_prio2:_test_call_indirect_mul"
        "03_test_prio2:_test_i64_add"
        "03_test_prio2:_test_i64_mul"
        "03_test_prio2:_test_i64_rem_s"
        "03_test_prio2:_test_i64_convert_to_f64"
        "03_test_prio2:_test_i64_load32_s"
        "03_test_prio2:_test_i64_large_mul"
        "03_test_prio2:_test_combined_all_features"
        "04_test_prio3:_test_i32_rem_u"
        "04_test_prio3:_test_i64_rem_u_large"
        "04_test_prio3:_test_i32_le_u_large"
        "04_test_prio3:_test_i32_ge_u_large"
        "04_test_prio3:_test_f32_copysign_neg"
        "04_test_prio3:_test_f64_sub"
        "04_test_prio3:_test_f32_store_load"
        "04_test_prio3:_test_f64_store_load"
        "04_test_prio3:_test_f32_arithmetic_with_load"
        "04_test_prio3:_test_unreachable_not_reached"
        "05_test_complex:nested_blocks"
        "05_test_complex:block_results"
        "05_test_complex:conditional_nested_0"
        "05_test_complex:conditional_nested_1"
        "05_test_complex:conditional_nested_2"
        "05_test_complex:call_in_block"
        "05_test_complex:loop_with_blocks"
        "05_test_complex:multi_call"
        "06_test_fc:_test_i32_trunc_sat_f32_s_normal"
        "06_test_fc:_test_i32_trunc_sat_f32_s_negative"
        "06_test_fc:_test_i32_trunc_sat_f32_s_nan"
        "06_test_fc:_test_i32_trunc_sat_f32_s_overflow"
        "06_test_fc:_test_i32_trunc_sat_f32_s_underflow"
        "06_test_fc:_test_i32_trunc_sat_f32_u_normal"
        "06_test_fc:_test_i32_trunc_sat_f32_u_nan"
        "06_test_fc:_test_i32_trunc_sat_f32_u_negative"
        "06_test_fc:_test_i32_trunc_sat_f32_u_overflow"
        "06_test_fc:_test_i32_trunc_sat_f64_s_normal"
        "06_test_fc:_test_i32_trunc_sat_f64_s_negative"
        "06_test_fc:_test_i32_trunc_sat_f64_s_nan"
        "06_test_fc:_test_i32_trunc_sat_f64_s_overflow"
        "06_test_fc:_test_i32_trunc_sat_f64_s_underflow"
        "06_test_fc:_test_i32_trunc_sat_f64_u_normal"
        "06_test_fc:_test_i32_trunc_sat_f64_u_nan"
        "06_test_fc:_test_i32_trunc_sat_f64_u_negative"
        "06_test_fc:_test_i32_trunc_sat_f64_u_overflow"
        "06_test_fc:_test_i64_trunc_sat_f32_s_normal"
        "06_test_fc:_test_i64_trunc_sat_f32_s_negative"
        "06_test_fc:_test_i64_trunc_sat_f32_s_nan"
        "06_test_fc:_test_i64_trunc_sat_f32_u_normal"
        "06_test_fc:_test_i64_trunc_sat_f32_u_nan"
        "06_test_fc:_test_i64_trunc_sat_f32_u_negative"
        "06_test_fc:_test_i64_trunc_sat_f64_s_normal"
        "06_test_fc:_test_i64_trunc_sat_f64_s_negative"
        "06_test_fc:_test_i64_trunc_sat_f64_s_nan"
        "06_test_fc:_test_i64_trunc_sat_f64_u_normal"
        "06_test_fc:_test_i64_trunc_sat_f64_u_nan"
        "06_test_fc:_test_i64_trunc_sat_f64_u_negative"
        "06_test_fc:_test_zero_f32"
        "06_test_fc:_test_small_f32"
        "06_test_fc:_test_negzero_f64"
        "06_test_fc:_test_large_in_range"
        "07_test_bulk_memory:_test_fill_basic"
        "07_test_bulk_memory:_test_fill_range"
        "07_test_bulk_memory:_test_fill_single"
        "07_test_bulk_memory:_test_fill_zero"
        "07_test_bulk_memory:_test_copy_basic"
        "07_test_bulk_memory:_test_copy_single"
        "07_test_bulk_memory:_test_copy_block"
        "07_test_bulk_memory:_test_copy_overlapping"
        "07_test_bulk_memory:_test_init_basic"
        "07_test_bulk_memory:_test_init_partial"
        "07_test_bulk_memory:_test_init_segment1"
        "07_test_bulk_memory:_test_drop_after_use"
        "07_test_bulk_memory:_test_combined_fill_copy"
        "07_test_bulk_memory:_test_combined_init_copy"
        "07_test_bulk_memory:_test_zero_length"
        "08_test_post_mvp:_test_multiret_two"
        "08_test_post_mvp:_test_multiret_three"
        "08_test_post_mvp:_test_multiret_swap"
        "08_test_post_mvp:_test_multiret_divmod"
        "08_test_post_mvp:_test_multiret_minmax"
        "08_test_post_mvp:_test_multiret_chain"
        "08_test_post_mvp:_test_multiret_discard"
        "08_test_post_mvp:_test_bulk_copy_verify_first"
        "08_test_post_mvp:_test_bulk_copy_verify_third"
        "08_test_post_mvp:_test_bulk_fill_verify"
        "08_test_post_mvp:_test_bulk_fill_verify_middle"
        "08_test_post_mvp:_test_bulk_fill_different"
        "08_test_post_mvp:_test_bulk_copy_overlap"
        "08_test_post_mvp:_test_bulk_copy_string"
        "08_test_post_mvp:_test_bulk_fill_range"
        "08_test_post_mvp:_test_bulk_copy_modify"
        "08_test_post_mvp:_test_ref_null_func"
        "08_test_post_mvp:_test_ref_null_extern"
        "08_test_post_mvp:_test_ref_func_not_null"
        "08_test_post_mvp:_test_ref_global_store"
        "08_test_post_mvp:_test_ref_table_set_get"
        "08_test_post_mvp:_test_ref_table_get_null"
        "08_test_post_mvp:_test_ref_table_size"
        "08_test_post_mvp:_test_ref_table_grow"
        "08_test_post_mvp:_test_ref_table_size_after"
        "08_test_post_mvp:_test_ref_table_fill"
        "08_test_post_mvp:_test_ref_table_copy"
        "08_test_post_mvp:_test_ref_externref_global"
        "08_test_post_mvp:_test_ref_externref_store"
        "08_test_post_mvp:_test_ref_externref_table_size"
        "08_test_post_mvp:_test_combined_multiret_bulk"
        "08_test_post_mvp:_test_combined_table_multiret"
        "08_test_post_mvp:_test_combined_fill_copy"
        "08_test_post_mvp:_test_combined_ref_sizes"
        "08_test_post_mvp:_test_combined_swap_bulk"
        "08_test_post_mvp:_test_combined_bulk_pattern"
        "08_test_post_mvp:_test_combined_table_results"
        "custom_imports:_start"
        "custom_imports:call_table_again"
        "custom_imports:store_global"
    )

    foreach(module_name IN LISTS WASM_INTERP_MODULES)
        add_test(NAME "module.${module_name}" COMMAND wasm_interp_tests ${module_name})
        set_tests_properties("module.${module_name}" PROPERTIES LABELS "module")
    endforeach()

    foreach(entry IN LISTS WASM_INTERP_TEST_CASES)
        string(REPLACE ":" ";" module_case ${entry})
        list(GET module_case 0 module_name)
        list(GET module_case 1 case_name)
        add_test(NAME "${module_name}.${case_name}" COMMAND wasm_interp_tests ${module_name} ${case_name})
        set_tests_properties("${module_name}.${case_name}" PROPERTIES LABELS "case")
    endforeach()
endif()
