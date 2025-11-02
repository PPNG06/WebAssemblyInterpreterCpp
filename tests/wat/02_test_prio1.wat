;;
;; WebAssembly 1.0 (MVP) Test Suite - Functions, Floats, Conversions
;;
;; This file contains ONLY WebAssembly 1.0 MVP features
;; Compatible with all WASM runtimes and standard Clang output
;;
;; Coverage: function calls, recursion, f32/f64 operations, type conversions,
;;           return instruction, drop, nop, memory.size, memory.grow
;;

(module
  (type (;0;) (func))
  (type (;1;) (func (param i32) (result i32)))
  (type (;2;) (func (param i32 i32) (result i32)))
  (type (;3;) (func (param f32 f32) (result f32)))
  (type (;4;) (func (param f64 f64) (result f64)))
  (type (;5;) (func (result i32)))
  (type (;6;) (func (param i32 i32 i32) (result i32)))
  
  ;; Global variables for testing
  (global $test_result (mut i32) (i32.const 0))
  
  ;; Memory
  (memory (;0;) 1)
  
  ;; Exports (forward references to functions defined below)
  (export "memory" (memory 0))
  (export "test_result" (global $test_result))
  (export "_start" (func 0))
  (export "_test_call_add" (func 0))
  (export "_test_call_composition" (func 1))
  (export "_test_call_square" (func 2))
  (export "_test_call_multiple" (func 3))
  (export "_test_return_early_true" (func 4))
  (export "_test_return_early_false" (func 5))
  (export "_test_abs_negative" (func 6))
  (export "_test_abs_positive" (func 7))
  (export "_test_factorial" (func 8))
  (export "_test_fibonacci" (func 9))
  (export "_test_f32_add" (func 10))
  (export "_test_f32_sub" (func 11))
  (export "_test_f32_mul" (func 12))
  (export "_test_f32_div" (func 13))
  (export "_test_f32_min" (func 14))
  (export "_test_f32_max" (func 15))
  (export "_test_f32_abs" (func 16))
  (export "_test_f32_neg" (func 17))
  (export "_test_f32_sqrt" (func 18))
  (export "_test_f32_ceil" (func 19))
  (export "_test_f32_floor" (func 20))
  (export "_test_f32_trunc" (func 21))
  (export "_test_f32_nearest" (func 22))
  (export "_test_f32_eq" (func 23))
  (export "_test_f32_ne" (func 24))
  (export "_test_f32_lt" (func 25))
  (export "_test_f32_gt" (func 26))
  (export "_test_f32_le" (func 27))
  (export "_test_f32_ge" (func 28))
  (export "_test_f32_call" (func 29))
  (export "_test_f64_add" (func 30))
  (export "_test_f64_mul" (func 31))
  (export "_test_f64_sqrt" (func 32))
  (export "_test_f64_gt" (func 33))
  (export "_test_convert_i32_to_f32_s" (func 34))
  (export "_test_convert_i32_to_f32_u" (func 35))
  (export "_test_convert_f32_to_i32_s" (func 36))
  (export "_test_convert_f32_to_i32_u" (func 37))
  (export "_test_convert_i32_to_f64_s" (func 38))
  (export "_test_convert_f64_to_i32_s" (func 39))
  (export "_test_promote_f32_to_f64" (func 40))
  (export "_test_demote_f64_to_f32" (func 41))
  (export "_test_reinterpret_f32_to_i32" (func 42))
  (export "_test_reinterpret_i32_to_f32" (func 43))
  (export "_test_drop_simple" (func 44))
  (export "_test_drop_multiple" (func 45))
  (export "_test_nop" (func 46))
  (export "_test_drop_in_computation" (func 47))
  (export "_test_memory_size" (func 48))
  (export "_test_memory_grow" (func 49))
  (export "_test_memory_size_after_grow" (func 50))
  (export "_test_memory_grow_multiple" (func 51))
  (export "_test_memory_write_grown" (func 52))
  (export "_test_combined_functions" (func 53))
  (export "_test_combined_float_convert" (func 54))
  
  ;; === FUNCTION CALL TESTS ===
  
  ;; Test: Call function with parameters and return value - add(10, 5) = 15
  ;; Expected result at address[0]: 15
  (func (;0;) (type 0)
    i32.const 0
    i32.const 10
    i32.const 5
    call $add_i32
    i32.store)
  
  ;; Test: Call function composition - mul(add(3, 4), 5) = 35
  ;; Expected result at address[0]: 35
  (func (;1;) (type 0)
    i32.const 0
    i32.const 3
    i32.const 4
    call $add_i32
    i32.const 5
    call $mul_i32
    i32.store)
  
  ;; Test: Call square function - square(7) = 49
  ;; Expected result at address[0]: 49
  (func (;2;) (type 0)
    i32.const 0
    i32.const 7
    call $square
    i32.store)
  
  ;; Test: Multiple calls - square(3) + square(4) = 9 + 16 = 25
  ;; Expected result at address[0]: 25
  (func (;3;) (type 0)
    i32.const 0
    i32.const 3
    call $square
    i32.const 4
    call $square
    call $add_i32
    i32.store)
  
  ;; === RETURN INSTRUCTION TESTS ===
  
  ;; Test: Early return - returns 100 if x > 10, else 200
  ;; Expected result at address[0]: 100 (input is 15)
  (func (;4;) (type 0)
    (local $x i32)
    i32.const 15
    local.set $x
    
    local.get $x
    i32.const 10
    i32.gt_s
    if
      i32.const 0
      i32.const 100
      i32.store
      return
    end
    i32.const 0
    i32.const 200
    i32.store)
  
  ;; Test: Function with conditional return
  ;; Expected result at address[0]: 200 (input is 5)
  (func (;5;) (type 0)
    (local $x i32)
    i32.const 5
    local.set $x
    
    local.get $x
    i32.const 10
    i32.gt_s
    if
      i32.const 0
      i32.const 100
      i32.store
      return
    end
    i32.const 0
    i32.const 200
    i32.store)
  
  ;; Test: Call abs with negative - abs(-42) = 42
  ;; Expected result at address[0]: 42
  (func (;6;) (type 0)
    i32.const 0
    i32.const -42
    call $abs
    i32.store)
  
  ;; Test: Call abs with positive - abs(42) = 42
  ;; Expected result at address[0]: 42
  (func (;7;) (type 0)
    i32.const 0
    i32.const 42
    call $abs
    i32.store)
  
  ;; Test: Factorial of 5 = 120
  ;; Expected result at address[0]: 120
  (func (;8;) (type 0)
    i32.const 0
    i32.const 5
    call $factorial
    i32.store)
  
  ;; Test: Fibonacci of 7 = 13
  ;; Expected result at address[0]: 13
  (func (;9;) (type 0)
    i32.const 0
    i32.const 7
    call $fibonacci
    i32.store)
  
  ;; === FLOATING POINT TESTS (F32) ===
  
  ;; Test: F32 addition - 3.5 + 2.5 = 6.0
  ;; Expected result at address[0]: 6.0 (as f32 bit pattern)
  (func (;10;) (type 0)
    i32.const 0
    f32.const 3.5
    f32.const 2.5
    f32.add
    i32.reinterpret_f32
    i32.store)
  
  ;; Test: F32 subtraction - 10.5 - 3.5 = 7.0
  ;; Expected result at address[0]: 7.0 (as f32 bit pattern)
  (func (;11;) (type 0)
    i32.const 0
    f32.const 10.5
    f32.const 3.5
    f32.sub
    i32.reinterpret_f32
    i32.store)
  
  ;; Test: F32 multiplication - 4.0 * 2.5 = 10.0
  ;; Expected result at address[0]: 10.0 (as f32 bit pattern)
  (func (;12;) (type 0)
    i32.const 0
    f32.const 4.0
    f32.const 2.5
    f32.mul
    i32.reinterpret_f32
    i32.store)
  
  ;; Test: F32 division - 10.0 / 4.0 = 2.5
  ;; Expected result at address[0]: 2.5 (as f32 bit pattern)
  (func (;13;) (type 0)
    i32.const 0
    f32.const 10.0
    f32.const 4.0
    f32.div
    i32.reinterpret_f32
    i32.store)
  
  ;; Test: F32 min - min(3.5, 2.1) = 2.1
  ;; Expected result at address[0]: 2.1 (as f32 bit pattern)
  (func (;14;) (type 0)
    i32.const 0
    f32.const 3.5
    f32.const 2.1
    f32.min
    i32.reinterpret_f32
    i32.store)
  
  ;; Test: F32 max - max(3.5, 2.1) = 3.5
  ;; Expected result at address[0]: 3.5 (as f32 bit pattern)
  (func (;15;) (type 0)
    i32.const 0
    f32.const 3.5
    f32.const 2.1
    f32.max
    i32.reinterpret_f32
    i32.store)
  
  ;; Test: F32 abs - abs(-3.5) = 3.5
  ;; Expected result at address[0]: 3.5 (as f32 bit pattern)
  (func (;16;) (type 0)
    i32.const 0
    f32.const -3.5
    f32.abs
    i32.reinterpret_f32
    i32.store)
  
  ;; Test: F32 neg - neg(3.5) = -3.5
  ;; Expected result at address[0]: -3.5 (as f32 bit pattern)
  (func (;17;) (type 0)
    i32.const 0
    f32.const 3.5
    f32.neg
    i32.reinterpret_f32
    i32.store)
  
  ;; Test: F32 sqrt - sqrt(16.0) = 4.0
  ;; Expected result at address[0]: 4.0 (as f32 bit pattern)
  (func (;18;) (type 0)
    i32.const 0
    f32.const 16.0
    f32.sqrt
    i32.reinterpret_f32
    i32.store)
  
  ;; Test: F32 ceil - ceil(3.2) = 4.0
  ;; Expected result at address[0]: 4.0 (as f32 bit pattern)
  (func (;19;) (type 0)
    i32.const 0
    f32.const 3.2
    f32.ceil
    i32.reinterpret_f32
    i32.store)
  
  ;; Test: F32 floor - floor(3.8) = 3.0
  ;; Expected result at address[0]: 3.0 (as f32 bit pattern)
  (func (;20;) (type 0)
    i32.const 0
    f32.const 3.8
    f32.floor
    i32.reinterpret_f32
    i32.store)
  
  ;; Test: F32 trunc - trunc(3.8) = 3.0
  ;; Expected result at address[0]: 3.0 (as f32 bit pattern)
  (func (;21;) (type 0)
    i32.const 0
    f32.const 3.8
    f32.trunc
    i32.reinterpret_f32
    i32.store)
  
  ;; Test: F32 nearest - nearest(3.5) = 4.0 (round to even)
  ;; Expected result at address[0]: 4.0 (as f32 bit pattern)
  (func (;22;) (type 0)
    i32.const 0
    f32.const 3.5
    f32.nearest
    i32.reinterpret_f32
    i32.store)
  
  ;; Test: F32 comparison eq - 3.0 == 3.0 = 1 (true)
  ;; Expected result at address[0]: 1
  (func (;23;) (type 0)
    i32.const 0
    f32.const 3.0
    f32.const 3.0
    f32.eq
    i32.store)
  
  ;; Test: F32 comparison ne - 3.0 != 2.0 = 1 (true)
  ;; Expected result at address[0]: 1
  (func (;24;) (type 0)
    i32.const 0
    f32.const 3.0
    f32.const 2.0
    f32.ne
    i32.store)
  
  ;; Test: F32 comparison lt - 2.0 < 3.0 = 1 (true)
  ;; Expected result at address[0]: 1
  (func (;25;) (type 0)
    i32.const 0
    f32.const 2.0
    f32.const 3.0
    f32.lt
    i32.store)
  
  ;; Test: F32 comparison gt - 3.0 > 2.0 = 1 (true)
  ;; Expected result at address[0]: 1
  (func (;26;) (type 0)
    i32.const 0
    f32.const 3.0
    f32.const 2.0
    f32.gt
    i32.store)
  
  ;; Test: F32 comparison le - 3.0 <= 3.0 = 1 (true)
  ;; Expected result at address[0]: 1
  (func (;27;) (type 0)
    i32.const 0
    f32.const 3.0
    f32.const 3.0
    f32.le
    i32.store)
  
  ;; Test: F32 comparison ge - 3.0 >= 3.0 = 1 (true)
  ;; Expected result at address[0]: 1
  (func (;28;) (type 0)
    i32.const 0
    f32.const 3.0
    f32.const 3.0
    f32.ge
    i32.store)
  
  ;; Test: Call F32 function - add_f32(1.5, 2.5) = 4.0
  ;; Expected result at address[0]: 4.0 (as f32 bit pattern)
  (func (;29;) (type 0)
    i32.const 0
    f32.const 1.5
    f32.const 2.5
    call $add_f32
    i32.reinterpret_f32
    i32.store)
  
  ;; === FLOATING POINT TESTS (F64) ===
  
  ;; Test: F64 addition - 3.5 + 2.5 = 6.0
  ;; Expected result at address[0]: Lower 32 bits of 6.0 as f64
  (func (;30;) (type 0)
    i32.const 0
    f64.const 3.5
    f64.const 2.5
    f64.add
    i64.reinterpret_f64
    i32.wrap_i64
    i32.store)
  
  ;; Test: F64 multiplication - 4.0 * 2.5 = 10.0
  ;; Expected result at address[0]: Lower 32 bits of 10.0 as f64
  (func (;31;) (type 0)
    i32.const 0
    f64.const 4.0
    f64.const 2.5
    f64.mul
    i64.reinterpret_f64
    i32.wrap_i64
    i32.store)
  
  ;; Test: F64 sqrt - sqrt(16.0) = 4.0
  ;; Expected result at address[0]: Lower 32 bits of 4.0 as f64
  (func (;32;) (type 0)
    i32.const 0
    f64.const 16.0
    f64.sqrt
    i64.reinterpret_f64
    i32.wrap_i64
    i32.store)
  
  ;; Test: F64 comparison - 3.0 > 2.0 = 1
  ;; Expected result at address[0]: 1
  (func (;33;) (type 0)
    i32.const 0
    f64.const 3.0
    f64.const 2.0
    f64.gt
    i32.store)
  
  ;; === TYPE CONVERSION TESTS ===
  
  ;; Test: i32 to f32 conversion - convert_i32_s(42) = 42.0
  ;; Expected result at address[0]: 42.0 (as f32 bit pattern)
  (func (;34;) (type 0)
    i32.const 0
    i32.const 42
    f32.convert_i32_s
    i32.reinterpret_f32
    i32.store)
  
  ;; Test: i32 to f32 conversion unsigned - convert_i32_u(42) = 42.0
  ;; Expected result at address[0]: 42.0 (as f32 bit pattern)
  (func (;35;) (type 0)
    i32.const 0
    i32.const 42
    f32.convert_i32_u
    i32.reinterpret_f32
    i32.store)
  
  ;; Test: f32 to i32 conversion truncate signed - trunc_f32_s(42.7) = 42
  ;; Expected result at address[0]: 42
  (func (;36;) (type 0)
    i32.const 0
    f32.const 42.7
    i32.trunc_f32_s
    i32.store)
  
  ;; Test: f32 to i32 conversion truncate unsigned - trunc_f32_u(42.7) = 42
  ;; Expected result at address[0]: 42
  (func (;37;) (type 0)
    i32.const 0
    f32.const 42.7
    i32.trunc_f32_u
    i32.store)
  
  ;; Test: i32 to f64 conversion - convert_i32_s(100) = 100.0
  ;; Expected result at address[0]: Lower 32 bits of 100.0 as f64
  (func (;38;) (type 0)
    i32.const 0
    i32.const 100
    f64.convert_i32_s
    i64.reinterpret_f64
    i32.wrap_i64
    i32.store)
  
  ;; Test: f64 to i32 conversion - trunc_f64_s(100.9) = 100
  ;; Expected result at address[0]: 100
  (func (;39;) (type 0)
    i32.const 0
    f64.const 100.9
    i32.trunc_f64_s
    i32.store)
  
  ;; Test: f32 to f64 promotion - promote_f32(3.5) = 3.5
  ;; Expected result at address[0]: Lower 32 bits of 3.5 as f64
  (func (;40;) (type 0)
    i32.const 0
    f32.const 3.5
    f64.promote_f32
    i64.reinterpret_f64
    i32.wrap_i64
    i32.store)
  
  ;; Test: f64 to f32 demotion - demote_f64(3.5) = 3.5
  ;; Expected result at address[0]: 3.5 (as f32 bit pattern)
  (func (;41;) (type 0)
    i32.const 0
    f64.const 3.5
    f32.demote_f64
    i32.reinterpret_f32
    i32.store)
  
  ;; Test: Reinterpret f32 as i32 - reinterpret(1.0) returns bit pattern
  ;; Expected result at address[0]: 0x3F800000 (bit pattern of 1.0f)
  (func (;42;) (type 0)
    i32.const 0
    f32.const 1.0
    i32.reinterpret_f32
    i32.store)
  
  ;; Test: Reinterpret i32 as f32 - reinterpret(0x40400000) = 3.0
  ;; Expected result at address[0]: 0x40400000 (we convert back to verify)
  (func (;43;) (type 0)
    i32.const 0
    i32.const 0x40400000  ;; Bit pattern for 3.0f
    f32.reinterpret_i32
    i32.reinterpret_f32
    i32.store)
  
  ;; === DROP AND NOP TESTS ===
  
  ;; Test: Drop value from stack - calculate 5+3, drop it, return 42
  ;; Expected result at address[0]: 42
  (func (;44;) (type 0)
    i32.const 0
    i32.const 5
    i32.const 3
    i32.add
    drop          ;; Drop the 8
    i32.const 42
    i32.store)
  
  ;; Test: Multiple drops
  ;; Expected result at address[0]: 100
  (func (;45;) (type 0)
    i32.const 0
    i32.const 1
    i32.const 2
    i32.const 3
    drop
    drop
    drop
    i32.const 100
    i32.store)
  
  ;; Test: Nop instructions (should have no effect)
  ;; Expected result at address[0]: 42
  (func (;46;) (type 0)
    i32.const 0
    nop
    i32.const 42
    nop
    nop
    i32.store
    nop)
  
  ;; Test: Drop in computation - (10 + 20, drop) then store 50
  ;; Expected result at address[0]: 50
  (func (;47;) (type 0)
    i32.const 10
    i32.const 20
    i32.add
    drop
    i32.const 0
    i32.const 50
    i32.store)
  
  ;; === MEMORY SIZE AND GROW TESTS ===
  
  ;; Test: Get memory size in pages (should be 1 initially)
  ;; Expected result at address[0]: 1
  (func (;48;) (type 0)
    i32.const 0
    memory.size
    i32.store)
  
  ;; Test: Grow memory by 1 page, returns old size
  ;; Expected result at address[0]: 1 (old size before grow)
  (func (;49;) (type 0)
    i32.const 0
    i32.const 1
    memory.grow
    i32.store)
  
  ;; Test: Get memory size after grow (should be 2 if previous test ran)
  ;; Expected result at address[0]: 2 (if grow succeeded)
  (func (;50;) (type 0)
    ;; First grow the memory
    i32.const 1
    memory.grow
    drop
    
    ;; Now check the size
    i32.const 0
    memory.size
    i32.store)
  
  ;; Test: Grow memory by 2 pages
  ;; Expected result at address[0]: Previous size
  (func (;51;) (type 0)
    i32.const 0
    i32.const 2
    memory.grow
    i32.store)
  
  ;; Test: Write to newly grown memory (address 65536 = start of page 1)
  ;; Expected result at address[0]: 999
  (func (;52;) (type 0)
    ;; First ensure we have at least 2 pages
    i32.const 1
    memory.grow
    drop
    
    ;; Write to address 65536 (page 1)
    i32.const 65536
    i32.const 999
    i32.store
    
    ;; Read it back and store at address 0
    i32.const 0
    i32.const 65536
    i32.load
    i32.store)
  
  ;; === COMBINED TESTS ===
  
  ;; Test: Complex expression with multiple function calls
  ;; factorial(3) + fibonacci(5) = 6 + 5 = 11
  ;; Expected result at address[0]: 11
  (func (;53;) (type 0)
    i32.const 0
    i32.const 3
    call $factorial
    i32.const 5
    call $fibonacci
    i32.add
    i32.store)
  
  ;; Test: Float arithmetic with conversion
  ;; convert(42) / 2.0 = 21.0, then truncate = 21
  ;; Expected result at address[0]: 21
  (func (;54;) (type 0)
    i32.const 0
    i32.const 42
    f32.convert_i32_s
    f32.const 2.0
    f32.div
    i32.trunc_f32_s
    i32.store)
  
  ;; === HELPER FUNCTIONS (not exported, defined after tests) ===
  
  ;; Add two i32 values
  (func $add_i32 (type 2) (param $a i32) (param $b i32) (result i32)
    local.get $a
    local.get $b
    i32.add)
  
  ;; Multiply two i32 values
  (func $mul_i32 (type 2) (param $a i32) (param $b i32) (result i32)
    local.get $a
    local.get $b
    i32.mul)
  
  ;; Square an i32 value
  (func $square (type 1) (param $x i32) (result i32)
    local.get $x
    local.get $x
    i32.mul)
  
  ;; Add two f32 values
  (func $add_f32 (type 3) (param $a f32) (param $b f32) (result f32)
    local.get $a
    local.get $b
    f32.add)
  
  ;; Add two f64 values
  (func $add_f64 (type 4) (param $a f64) (param $b f64) (result f64)
    local.get $a
    local.get $b
    f64.add)
  
  ;; Absolute value
  (func $abs (type 1) (param $x i32) (result i32)
    local.get $x
    i32.const 0
    i32.lt_s
    if (result i32)
      i32.const 0
      local.get $x
      i32.sub
    else
      local.get $x
    end)
  
  ;; Factorial (recursive)
  (func $factorial (type 1) (param $n i32) (result i32)
    local.get $n
    i32.const 2
    i32.lt_s
    if (result i32)
      i32.const 1
    else
      local.get $n
      local.get $n
      i32.const 1
      i32.sub
      call $factorial
      i32.mul
    end)
  
  ;; Fibonacci (recursive)
  (func $fibonacci (type 1) (param $n i32) (result i32)
    local.get $n
    i32.const 2
    i32.lt_s
    if (result i32)
      local.get $n
    else
      local.get $n
      i32.const 1
      i32.sub
      call $fibonacci
      local.get $n
      i32.const 2
      i32.sub
      call $fibonacci
      i32.add
    end)
)

