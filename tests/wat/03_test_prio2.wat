;;
;; WebAssembly 1.0 (MVP) Test Suite - i64, Data Segments, call_indirect
;;
;; This file contains ONLY WebAssembly 1.0 MVP features
;; Compatible with all WASM runtimes and standard Clang output
;;
;; Coverage: i64 operations, data segments, function tables, call_indirect,
;;           trap conditions and prevention patterns
;;

(module
  (type (;0;) (func))
  (type (;1;) (func (param i32) (result i32)))
  (type (;2;) (func (param i32 i32) (result i32)))
  (type (;3;) (func (param i64) (result i64)))
  (type (;4;) (func (param i64 i64) (result i64)))
  
  ;; === DATA SEGMENTS ===
  ;; Pre-initialized memory with various data
  (data (i32.const 0) "Hello, WASM!")           ;; String at address 0
  (data (i32.const 16) "\2a\00\00\00")          ;; i32 value 42 at address 16
  (data (i32.const 20) "\ff\00\00\00")          ;; i32 value 255 at address 20
  (data (i32.const 100) "Test Data Segment")    ;; Another string at address 100
  
  ;; === FUNCTION TABLE FOR CALL_INDIRECT ===
  (table 4 funcref)
  
  ;; Table element initialization - maps indices to functions
  (elem (i32.const 0) $table_add $table_sub $table_mul $table_div)
  
  ;; === HELPER FUNCTIONS FOR TABLE ===
  
  ;; Function 0 in table: Add
  (func $table_add (type 2) (param $a i32) (param $b i32) (result i32)
    local.get $a
    local.get $b
    i32.add)
  
  ;; Function 1 in table: Subtract
  (func $table_sub (type 2) (param $a i32) (param $b i32) (result i32)
    local.get $a
    local.get $b
    i32.sub)
  
  ;; Function 2 in table: Multiply
  (func $table_mul (type 2) (param $a i32) (param $b i32) (result i32)
    local.get $a
    local.get $b
    i32.mul)
  
  ;; Function 3 in table: Divide
  (func $table_div (type 2) (param $a i32) (param $b i32) (result i32)
    local.get $a
    local.get $b
    i32.div_s)
  
  ;; === I64 HELPER FUNCTIONS ===
  
  (func $add_i64 (type 4) (param $a i64) (param $b i64) (result i64)
    local.get $a
    local.get $b
    i64.add)
  
  (func $mul_i64 (type 4) (param $a i64) (param $b i64) (result i64)
    local.get $a
    local.get $b
    i64.mul)
  
  ;; === DATA SEGMENT TESTS ===
  
  ;; Test: Read string from data segment - 'H' = 0x48
  ;; Expected result at address[200]: 72 (ASCII 'H')
  (func (;0;) (type 0)
    i32.const 200
    i32.const 0
    i32.load8_u
    i32.store)
  
  ;; Test: Read second character 'e' = 0x65
  ;; Expected result at address[200]: 101 (ASCII 'e')
  (func (;1;) (type 0)
    i32.const 200
    i32.const 1
    i32.load8_u
    i32.store)
  
  ;; Test: Read i32 from data segment at address 16
  ;; Expected result at address[200]: 42
  (func (;2;) (type 0)
    i32.const 200
    i32.const 16
    i32.load
    i32.store)
  
  ;; Test: Read i32 from data segment at address 20
  ;; Expected result at address[200]: 255
  (func (;3;) (type 0)
    i32.const 200
    i32.const 20
    i32.load
    i32.store)
  
  ;; Test: Read from second string 'T' = 0x54
  ;; Expected result at address[200]: 84 (ASCII 'T')
  (func (;4;) (type 0)
    i32.const 200
    i32.const 100
    i32.load8_u
    i32.store)
  
  ;; Test: Verify null terminator or character
  ;; Expected result at address[200]: 33 (ASCII '!')
  (func (;5;) (type 0)
    i32.const 200
    i32.const 11  ;; Position of '!' in "Hello, WASM!"
    i32.load8_u
    i32.store)
  
  ;; === CALL_INDIRECT TESTS ===
  
  ;; Test: call_indirect to add function (index 0) - 10 + 5 = 15
  ;; Expected result at address[200]: 15
  (func (;6;) (type 0)
    i32.const 200
    i32.const 10
    i32.const 5
    i32.const 0  ;; Table index for add
    call_indirect (type 2)
    i32.store)
  
  ;; Test: call_indirect to subtract function (index 1) - 10 - 5 = 5
  ;; Expected result at address[200]: 5
  (func (;7;) (type 0)
    i32.const 200
    i32.const 10
    i32.const 5
    i32.const 1  ;; Table index for sub
    call_indirect (type 2)
    i32.store)
  
  ;; Test: call_indirect to multiply function (index 2) - 10 * 5 = 50
  ;; Expected result at address[200]: 50
  (func (;8;) (type 0)
    i32.const 200
    i32.const 10
    i32.const 5
    i32.const 2  ;; Table index for mul
    call_indirect (type 2)
    i32.store)
  
  ;; Test: call_indirect to divide function (index 3) - 20 / 4 = 5
  ;; Expected result at address[200]: 5
  (func (;9;) (type 0)
    i32.const 200
    i32.const 20
    i32.const 4
    i32.const 3  ;; Table index for div
    call_indirect (type 2)
    i32.store)
  
  ;; Test: Dynamic dispatch - use variable to select function
  ;; Expected result at address[200]: 50 (10 * 5)
  (func (;10;) (type 0)
    (local $func_idx i32)
    i32.const 2
    local.set $func_idx
    
    i32.const 200
    i32.const 10
    i32.const 5
    local.get $func_idx
    call_indirect (type 2)
    i32.store)
  
  ;; Test: Loop through table functions
  ;; Expected result at address[200]: 8 (result of div: 20 / 2 + 10 / 5)
  (func (;11;) (type 0)
    (local $result i32)
    
    ;; Call function at index 3 (div): 20 / 4 = 5
    i32.const 20
    i32.const 4
    i32.const 3
    call_indirect (type 2)
    local.set $result
    
    ;; Call function at index 0 (add): 5 + 3 = 8
    local.get $result
    i32.const 3
    i32.const 0
    call_indirect (type 2)
    local.set $result
    
    i32.const 200
    local.get $result
    i32.store)
  
  ;; === I64 ARITHMETIC TESTS ===
  
  ;; Test: i64 addition - 10 + 5 = 15
  ;; Expected result at address[200]: 15 (lower 32 bits)
  (func (;12;) (type 0)
    i32.const 200
    i64.const 10
    i64.const 5
    i64.add
    i32.wrap_i64
    i32.store)
  
  ;; Test: i64 subtraction - 20 - 8 = 12
  ;; Expected result at address[200]: 12
  (func (;13;) (type 0)
    i32.const 200
    i64.const 20
    i64.const 8
    i64.sub
    i32.wrap_i64
    i32.store)
  
  ;; Test: i64 multiplication - 6 * 7 = 42
  ;; Expected result at address[200]: 42
  (func (;14;) (type 0)
    i32.const 200
    i64.const 6
    i64.const 7
    i64.mul
    i32.wrap_i64
    i32.store)
  
  ;; Test: i64 division signed - 20 / 4 = 5
  ;; Expected result at address[200]: 5
  (func (;15;) (type 0)
    i32.const 200
    i64.const 20
    i64.const 4
    i64.div_s
    i32.wrap_i64
    i32.store)
  
  ;; Test: i64 division unsigned
  ;; Expected result at address[200]: 6
  (func (;16;) (type 0)
    i32.const 200
    i64.const 20
    i64.const 3
    i64.div_u
    i32.wrap_i64
    i32.store)
  
  ;; Test: i64 remainder
  ;; Expected result at address[200]: 2
  (func (;17;) (type 0)
    i32.const 200
    i64.const 20
    i64.const 3
    i64.rem_s
    i32.wrap_i64
    i32.store)
  
  ;; Test: i64 bitwise AND
  ;; Expected result at address[200]: 10
  (func (;18;) (type 0)
    i32.const 200
    i64.const 15
    i64.const 10
    i64.and
    i32.wrap_i64
    i32.store)
  
  ;; Test: i64 bitwise OR
  ;; Expected result at address[200]: 15
  (func (;19;) (type 0)
    i32.const 200
    i64.const 5
    i64.const 10
    i64.or
    i32.wrap_i64
    i32.store)
  
  ;; Test: i64 bitwise XOR
  ;; Expected result at address[200]: 15
  (func (;20;) (type 0)
    i32.const 200
    i64.const 5
    i64.const 10
    i64.xor
    i32.wrap_i64
    i32.store)
  
  ;; Test: i64 left shift
  ;; Expected result at address[200]: 20
  (func (;21;) (type 0)
    i32.const 200
    i64.const 5
    i64.const 2
    i64.shl
    i32.wrap_i64
    i32.store)
  
  ;; Test: i64 right shift signed
  ;; Expected result at address[200]: Lower 32 bits of -4
  (func (;22;) (type 0)
    i32.const 200
    i64.const -8
    i64.const 1
    i64.shr_s
    i32.wrap_i64
    i32.store)
  
  ;; Test: i64 right shift unsigned
  ;; Expected result at address[200]: 4
  (func (;23;) (type 0)
    i32.const 200
    i64.const 16
    i64.const 2
    i64.shr_u
    i32.wrap_i64
    i32.store)
  
  ;; Test: i64 rotate left
  ;; Expected result at address[200]: 16
  (func (;24;) (type 0)
    i32.const 200
    i64.const 1
    i64.const 4
    i64.rotl
    i32.wrap_i64
    i32.store)
  
  ;; Test: i64 rotate right
  ;; Expected result at address[200]: 1
  (func (;25;) (type 0)
    i32.const 200
    i64.const 16
    i64.const 4
    i64.rotr
    i32.wrap_i64
    i32.store)
  
  ;; Test: i64 count leading zeros
  ;; Expected result at address[200]: 60
  (func (;26;) (type 0)
    i32.const 200
    i64.const 8
    i64.clz
    i32.wrap_i64
    i32.store)
  
  ;; Test: i64 count trailing zeros
  ;; Expected result at address[200]: 3
  (func (;27;) (type 0)
    i32.const 200
    i64.const 8
    i64.ctz
    i32.wrap_i64
    i32.store)
  
  ;; Test: i64 population count
  ;; Expected result at address[200]: 3
  (func (;28;) (type 0)
    i32.const 200
    i64.const 22
    i64.popcnt
    i32.wrap_i64
    i32.store)
  
  ;; === I64 COMPARISON TESTS ===
  
  ;; Test: i64 equal
  ;; Expected result at address[200]: 1
  (func (;29;) (type 0)
    i32.const 200
    i64.const 10
    i64.const 10
    i64.eq
    i32.store)
  
  ;; Test: i64 not equal
  ;; Expected result at address[200]: 1
  (func (;30;) (type 0)
    i32.const 200
    i64.const 10
    i64.const 5
    i64.ne
    i32.store)
  
  ;; Test: i64 less than signed
  ;; Expected result at address[200]: 1
  (func (;31;) (type 0)
    i32.const 200
    i64.const 5
    i64.const 10
    i64.lt_s
    i32.store)
  
  ;; Test: i64 greater than signed
  ;; Expected result at address[200]: 1
  (func (;32;) (type 0)
    i32.const 200
    i64.const 10
    i64.const 5
    i64.gt_s
    i32.store)
  
  ;; Test: i64 equal zero
  ;; Expected result at address[200]: 1
  (func (;33;) (type 0)
    i32.const 200
    i64.const 0
    i64.eqz
    i32.store)
  
  ;; === I64 CONVERSION TESTS ===
  
  ;; Test: i32 to i64 extend signed
  ;; Expected result at address[200]: -1 (lower 32 bits of -1 as i64)
  (func (;34;) (type 0)
    i32.const 200
    i32.const -1
    i64.extend_i32_s
    i32.wrap_i64
    i32.store)
  
  ;; Test: i32 to i64 extend unsigned
  ;; Expected result at address[200]: 255
  (func (;35;) (type 0)
    i32.const 200
    i32.const 255
    i64.extend_i32_u
    i32.wrap_i64
    i32.store)
  
  ;; Test: i64 to i32 wrap
  ;; Expected result at address[200]: 255 (wraps high bits)
  (func (;36;) (type 0)
    i32.const 200
    i64.const 0x100000000FF  ;; Large i64 value
    i32.wrap_i64
    i32.store)
  
  ;; Test: f32 to i64 conversion
  ;; Expected result at address[200]: 42
  (func (;37;) (type 0)
    i32.const 200
    f32.const 42.7
    i64.trunc_f32_s
    i32.wrap_i64
    i32.store)
  
  ;; Test: f64 to i64 conversion
  ;; Expected result at address[200]: 100
  (func (;38;) (type 0)
    i32.const 200
    f64.const 100.9
    i64.trunc_f64_s
    i32.wrap_i64
    i32.store)
  
  ;; Test: i64 to f32 conversion
  ;; Expected result at address[200]: 42.0 as f32 bits
  (func (;39;) (type 0)
    i32.const 200
    i64.const 42
    f32.convert_i64_s
    i32.reinterpret_f32
    i32.store)
  
  ;; Test: i64 to f64 conversion (store lower 32 bits)
  ;; Expected result at address[200]: Lower bits of 100.0 as f64
  (func (;40;) (type 0)
    i32.const 200
    i64.const 100
    f64.convert_i64_s
    i64.reinterpret_f64
    i32.wrap_i64
    i32.store)
  
  ;; === I64 MEMORY OPERATIONS ===
  
  ;; Test: Store and load i64
  ;; Expected result at address[200]: 255 (lower 32 bits)
  (func (;41;) (type 0)
    ;; Store i64 at address 300
    i32.const 300
    i64.const 0x12345678000000FF
    i64.store
    
    ;; Load and store lower 32 bits
    i32.const 200
    i32.const 300
    i64.load
    i32.wrap_i64
    i32.store)
  
  ;; Test: i64 load32 unsigned
  ;; Expected result at address[200]: 240 (0xF0)
  (func (;42;) (type 0)
    i32.const 300
    i64.const 0x12345678000000F0
    i64.store
    
    i32.const 200
    i32.const 300
    i64.load32_u
    i32.wrap_i64
    i32.store)
  
  ;; Test: i64 load32 signed
  ;; Expected result at address[200]: Sign-extended value
  (func (;43;) (type 0)
    i32.const 300
    i64.const 0x80000000  ;; Negative in signed 32-bit
    i64.store
    
    i32.const 200
    i32.const 300
    i64.load32_s
    i32.wrap_i64
    i32.store)
  
  ;; Test: Call i64 function
  ;; Expected result at address[200]: 42
  (func (;44;) (type 0)
    i32.const 200
    i64.const 30
    i64.const 12
    call $add_i64
    i32.wrap_i64
    i32.store)
  
  ;; === LARGE I64 VALUE TESTS ===
  
  ;; Test: Large i64 value multiplication
  ;; Expected result at address[200]: Lower 32 bits of result
  (func (;45;) (type 0)
    i32.const 200
    i64.const 1000000
    i64.const 1000
    i64.mul
    i32.wrap_i64
    i32.store)
  
  ;; Test: i64 with bit pattern
  ;; Expected result at address[200]: 0xFFFFFFFF
  (func (;46;) (type 0)
    i32.const 200
    i64.const 0xFFFFFFFF
    i32.wrap_i64
    i32.store)
  
  ;; === TRAP HANDLING TESTS ===
  ;; Note: These tests demonstrate operations that SHOULD trap
  ;; In a proper executor, you'd catch these and verify they trap correctly
  
  ;; Test: Safe division - 20 / 4 = 5 (no trap)
  ;; Expected result at address[200]: 5
  (func (;47;) (type 0)
    i32.const 200
    i32.const 20
    i32.const 4
    i32.div_s
    i32.store)
  
  ;; Test: Division by zero test setup - store divisor
  ;; Expected result at address[200]: 0 (the divisor we're checking)
  (func (;48;) (type 0)
    (local $divisor i32)
    i32.const 0
    local.set $divisor
    
    i32.const 200
    local.get $divisor
    i32.store)
  
  ;; Test: Check for zero before division
  ;; Expected result at address[200]: -1 (error code for would-be division by zero)
  (func (;49;) (type 0)
    (local $divisor i32)
    i32.const 0
    local.set $divisor
    
    i32.const 200
    local.get $divisor
    i32.eqz
    if (result i32)
      i32.const -1  ;; Return error code
    else
      i32.const 100
      local.get $divisor
      i32.div_s
    end
    i32.store)
  
  ;; Test: Out of bounds memory check - verify address is in bounds
  ;; Expected result at address[200]: 1 (true, address is valid)
  (func (;50;) (type 0)
    (local $address i32)
    i32.const 1000
    local.set $address
    
    i32.const 200
    local.get $address
    memory.size
    i32.const 65536
    i32.mul  ;; Convert pages to bytes
    i32.lt_u  ;; Check if address < memory size
    i32.store)
  
  ;; Test: Check if address would be out of bounds
  ;; Expected result at address[200]: 0 (false, would be out of bounds)
  (func (;51;) (type 0)
    (local $address i32)
    i32.const 200000  ;; Beyond initial 2 pages (2*65536=131072)
    local.set $address
    
    i32.const 200
    local.get $address
    memory.size
    i32.const 65536
    i32.mul
    i32.lt_u
    i32.store)
  
  ;; Test: Integer overflow detection (INT_MIN / -1 should trap)
  ;; Expected result at address[200]: 1 (detected would-overflow condition)
  (func (;52;) (type 0)
    (local $dividend i32)
    (local $divisor i32)
    i32.const 0x80000000  ;; INT_MIN
    local.set $dividend
    i32.const -1
    local.set $divisor
    
    i32.const 200
    ;; Check for overflow condition: INT_MIN / -1
    local.get $dividend
    i32.const 0x80000000
    i32.eq
    local.get $divisor
    i32.const -1
    i32.eq
    i32.and
    i32.store)
  
  ;; Test: Modulo by zero check
  ;; Expected result at address[200]: -1 (error code)
  (func (;53;) (type 0)
    (local $divisor i32)
    i32.const 0
    local.set $divisor
    
    i32.const 200
    local.get $divisor
    i32.eqz
    if (result i32)
      i32.const -1
    else
      i32.const 100
      local.get $divisor
      i32.rem_s
    end
    i32.store)
  
  ;; Test: i64 division by zero check
  ;; Expected result at address[200]: -1 (error code)
  (func (;54;) (type 0)
    (local $divisor i64)
    i64.const 0
    local.set $divisor
    
    i32.const 200
    local.get $divisor
    i64.eqz
    if (result i32)
      i32.const -1
    else
      i64.const 100
      local.get $divisor
      i64.div_s
      i32.wrap_i64
    end
    i32.store)
  
  ;; === COMBINED TESTS ===
  
  ;; Test: Read from data segment and use with i64
  ;; Expected result at address[200]: 84 (42 * 2)
  (func (;55;) (type 0)
    i32.const 200
    i32.const 16
    i32.load
    i64.extend_i32_s
    i64.const 2
    i64.mul
    i32.wrap_i64
    i32.store)
  
  ;; Test: Call_indirect with result used in i64 operation
  ;; Expected result at address[200]: 30 (15 * 2)
  (func (;56;) (type 0)
    i32.const 200
    i32.const 10
    i32.const 5
    i32.const 0  ;; add function
    call_indirect (type 2)
    i64.extend_i32_s
    i64.const 2
    i64.mul
    i32.wrap_i64
    i32.store)
  
  ;; Test: Complex expression with all features
  ;; (data[16] + call_indirect(10,5,add)) * i64(2)
  ;; Expected result at address[200]: 114 ((42 + 15) * 2)
  (func (;57;) (type 0)
    i32.const 200
    
    ;; Load from data segment (42)
    i32.const 16
    i32.load
    
    ;; Add via call_indirect (10 + 5 = 15)
    i32.const 10
    i32.const 5
    i32.const 0
    call_indirect (type 2)
    
    ;; Sum them (42 + 15 = 57)
    i32.add
    i64.extend_i32_s
    
    ;; Multiply by 2 (57 * 2 = 114)
    i64.const 2
    i64.mul
    
    i32.wrap_i64
    i32.store)
  
  (memory (;0;) 1)
  (export "memory" (memory 0))
  (export "table" (table 0))
  
  (export "_start" (func 6))
  (export "_test_data_read_char_h" (func 6))
  (export "_test_data_read_char_e" (func 7))
  (export "_test_data_read_i32_42" (func 8))
  (export "_test_data_read_i32_255" (func 9))
  (export "_test_data_read_char_t" (func 10))
  (export "_test_data_read_exclaim" (func 11))
  (export "_test_call_indirect_add" (func 12))
  (export "_test_call_indirect_sub" (func 13))
  (export "_test_call_indirect_mul" (func 14))
  (export "_test_call_indirect_div" (func 15))
  (export "_test_call_indirect_dynamic" (func 16))
  (export "_test_call_indirect_loop" (func 17))
  (export "_test_i64_add" (func 18))
  (export "_test_i64_sub" (func 19))
  (export "_test_i64_mul" (func 20))
  (export "_test_i64_div_s" (func 21))
  (export "_test_i64_div_u" (func 22))
  (export "_test_i64_rem_s" (func 23))
  (export "_test_i64_and" (func 24))
  (export "_test_i64_or" (func 25))
  (export "_test_i64_xor" (func 26))
  (export "_test_i64_shl" (func 27))
  (export "_test_i64_shr_s" (func 28))
  (export "_test_i64_shr_u" (func 29))
  (export "_test_i64_rotl" (func 30))
  (export "_test_i64_rotr" (func 31))
  (export "_test_i64_clz" (func 32))
  (export "_test_i64_ctz" (func 33))
  (export "_test_i64_popcnt" (func 34))
  (export "_test_i64_eq" (func 35))
  (export "_test_i64_ne" (func 36))
  (export "_test_i64_lt_s" (func 37))
  (export "_test_i64_gt_s" (func 38))
  (export "_test_i64_eqz" (func 39))
  (export "_test_i64_extend_i32_s" (func 40))
  (export "_test_i64_extend_i32_u" (func 41))
  (export "_test_i64_wrap" (func 42))
  (export "_test_i64_trunc_f32_s" (func 43))
  (export "_test_i64_trunc_f64_s" (func 44))
  (export "_test_i64_convert_to_f32" (func 45))
  (export "_test_i64_convert_to_f64" (func 46))
  (export "_test_i64_store_load" (func 47))
  (export "_test_i64_load32_u" (func 48))
  (export "_test_i64_load32_s" (func 49))
  (export "_test_i64_call_function" (func 50))
  (export "_test_i64_large_mul" (func 51))
  (export "_test_i64_bit_pattern" (func 52))
  (export "_test_trap_safe_div" (func 53))
  (export "_test_trap_divisor_zero" (func 54))
  (export "_test_trap_check_div_zero" (func 55))
  (export "_test_trap_check_mem_valid" (func 56))
  (export "_test_trap_check_mem_invalid" (func 57))
  (export "_test_trap_check_overflow" (func 58))
  (export "_test_trap_check_rem_zero" (func 59))
  (export "_test_trap_check_i64_div_zero" (func 60))
  (export "_test_combined_data_i64" (func 61))
  (export "_test_combined_indirect_i64" (func 62))
  (export "_test_combined_all_features" (func 63))
)

