;;
;; WebAssembly 1.0 (MVP) Test Suite - Missing Features
;;
;; This file contains additional MVP features that were missing from the main tests
;; All features here are part of the original WASM 1.0 specification
;;
;; Coverage: unsigned remainders, unsigned comparisons (le_u, ge_u),
;;           f32/f64 copysign, float memory operations, unreachable instruction
;;

(module
  (type (;0;) (func))
  
  ;; === UNSIGNED REMAINDER OPERATIONS ===
  
  ;; Test: i32.rem_u - 20 % 3 = 2 (unsigned)
  ;; Expected result at address[0]: 2
  (func (;0;) (type 0)
    i32.const 0
    i32.const 20
    i32.const 3
    i32.rem_u
    i32.store)
  
  ;; Test: i32.rem_u - Test with large unsigned value
  ;; Expected result at address[0]: 1 (0xFFFFFFFF % 2 = 1)
  (func (;1;) (type 0)
    i32.const 0
    i32.const 0xFFFFFFFF
    i32.const 2
    i32.rem_u
    i32.store)
  
  ;; Test: i64.rem_u - 20 % 3 = 2 (unsigned)
  ;; Expected result at address[0]: 2
  (func (;2;) (type 0)
    i32.const 0
    i64.const 20
    i64.const 3
    i64.rem_u
    i32.wrap_i64
    i32.store)
  
  ;; Test: i64.rem_u - Large value
  ;; Expected result at address[0]: 1
  (func (;3;) (type 0)
    i32.const 0
    i64.const 0xFFFFFFFFFFFFFFFF
    i64.const 2
    i64.rem_u
    i32.wrap_i64
    i32.store)
  
  ;; === UNSIGNED COMPARISON OPERATIONS (i32) ===
  
  ;; Test: i32.le_u - 5 <= 10 = 1 (true)
  ;; Expected result at address[0]: 1
  (func (;4;) (type 0)
    i32.const 0
    i32.const 5
    i32.const 10
    i32.le_u
    i32.store)
  
  ;; Test: i32.le_u - 10 <= 10 = 1 (true)
  ;; Expected result at address[0]: 1
  (func (;5;) (type 0)
    i32.const 0
    i32.const 10
    i32.const 10
    i32.le_u
    i32.store)
  
  ;; Test: i32.le_u - Large unsigned comparison
  ;; Expected result at address[0]: 0 (false, 0xFFFFFFFF > 100 as unsigned)
  (func (;6;) (type 0)
    i32.const 0
    i32.const 0xFFFFFFFF
    i32.const 100
    i32.le_u
    i32.store)
  
  ;; Test: i32.ge_u - 10 >= 5 = 1 (true)
  ;; Expected result at address[0]: 1
  (func (;7;) (type 0)
    i32.const 0
    i32.const 10
    i32.const 5
    i32.ge_u
    i32.store)
  
  ;; Test: i32.ge_u - 10 >= 10 = 1 (true)
  ;; Expected result at address[0]: 1
  (func (;8;) (type 0)
    i32.const 0
    i32.const 10
    i32.const 10
    i32.ge_u
    i32.store)
  
  ;; Test: i32.ge_u - Large unsigned comparison
  ;; Expected result at address[0]: 1 (true, 0xFFFFFFFF >= 100 as unsigned)
  (func (;9;) (type 0)
    i32.const 0
    i32.const 0xFFFFFFFF
    i32.const 100
    i32.ge_u
    i32.store)
  
  ;; === UNSIGNED COMPARISON OPERATIONS (i64) ===
  
  ;; Test: i64.le_s - 5 <= 10 (signed)
  ;; Expected result at address[0]: 1
  (func (;10;) (type 0)
    i32.const 0
    i64.const 5
    i64.const 10
    i64.le_s
    i32.store)
  
  ;; Test: i64.le_s - -5 <= 10 (signed)
  ;; Expected result at address[0]: 1
  (func (;11;) (type 0)
    i32.const 0
    i64.const -5
    i64.const 10
    i64.le_s
    i32.store)
  
  ;; Test: i64.le_u - 5 <= 10 (unsigned)
  ;; Expected result at address[0]: 1
  (func (;12;) (type 0)
    i32.const 0
    i64.const 5
    i64.const 10
    i64.le_u
    i32.store)
  
  ;; Test: i64.le_u - Large unsigned comparison
  ;; Expected result at address[0]: 0 (false)
  (func (;13;) (type 0)
    i32.const 0
    i64.const 0xFFFFFFFFFFFFFFFF
    i64.const 100
    i64.le_u
    i32.store)
  
  ;; Test: i64.ge_s - 10 >= 5 (signed)
  ;; Expected result at address[0]: 1
  (func (;14;) (type 0)
    i32.const 0
    i64.const 10
    i64.const 5
    i64.ge_s
    i32.store)
  
  ;; Test: i64.ge_s - -5 >= -10 (signed)
  ;; Expected result at address[0]: 1
  (func (;15;) (type 0)
    i32.const 0
    i64.const -5
    i64.const -10
    i64.ge_s
    i32.store)
  
  ;; Test: i64.ge_u - 10 >= 5 (unsigned)
  ;; Expected result at address[0]: 1
  (func (;16;) (type 0)
    i32.const 0
    i64.const 10
    i64.const 5
    i64.ge_u
    i32.store)
  
  ;; Test: i64.ge_u - Large unsigned comparison
  ;; Expected result at address[0]: 1 (true)
  (func (;17;) (type 0)
    i32.const 0
    i64.const 0xFFFFFFFFFFFFFFFF
    i64.const 100
    i64.ge_u
    i32.store)
  
  ;; === F32 COPYSIGN ===
  
  ;; Test: f32.copysign - Copy sign from negative to positive
  ;; copysign(3.5, -1.0) = -3.5
  ;; Expected result at address[0]: -3.5 (as f32 bit pattern)
  (func (;18;) (type 0)
    i32.const 0
    f32.const 3.5
    f32.const -1.0
    f32.copysign
    i32.reinterpret_f32
    i32.store)
  
  ;; Test: f32.copysign - Copy sign from positive to negative
  ;; copysign(-3.5, 1.0) = 3.5
  ;; Expected result at address[0]: 3.5 (as f32 bit pattern)
  (func (;19;) (type 0)
    i32.const 0
    f32.const -3.5
    f32.const 1.0
    f32.copysign
    i32.reinterpret_f32
    i32.store)
  
  ;; Test: f32.copysign - Both positive
  ;; copysign(3.5, 2.0) = 3.5
  ;; Expected result at address[0]: 3.5 (as f32 bit pattern)
  (func (;20;) (type 0)
    i32.const 0
    f32.const 3.5
    f32.const 2.0
    f32.copysign
    i32.reinterpret_f32
    i32.store)
  
  ;; === F64 COPYSIGN ===
  
  ;; Test: f64.copysign - Copy sign from negative to positive
  ;; copysign(3.5, -1.0) = -3.5
  ;; Expected result at address[0]: Lower 32 bits of -3.5 as f64
  (func (;21;) (type 0)
    i32.const 0
    f64.const 3.5
    f64.const -1.0
    f64.copysign
    i64.reinterpret_f64
    i32.wrap_i64
    i32.store)
  
  ;; Test: f64.copysign - Copy sign from positive to negative
  ;; copysign(-3.5, 1.0) = 3.5
  ;; Expected result at address[0]: Lower 32 bits of 3.5 as f64
  (func (;22;) (type 0)
    i32.const 0
    f64.const -3.5
    f64.const 1.0
    f64.copysign
    i64.reinterpret_f64
    i32.wrap_i64
    i32.store)
  
  ;; === F64 MISSING OPERATIONS ===
  
  ;; Test: f64.sub - 10.5 - 3.5 = 7.0
  ;; Expected result at address[0]: Lower 32 bits of 7.0 as f64
  (func (;23;) (type 0)
    i32.const 0
    f64.const 10.5
    f64.const 3.5
    f64.sub
    i64.reinterpret_f64
    i32.wrap_i64
    i32.store)
  
  ;; Test: f64.div - 10.0 / 4.0 = 2.5
  ;; Expected result at address[0]: Lower 32 bits of 2.5 as f64
  (func (;24;) (type 0)
    i32.const 0
    f64.const 10.0
    f64.const 4.0
    f64.div
    i64.reinterpret_f64
    i32.wrap_i64
    i32.store)
  
  ;; Test: f64.min - min(3.5, 2.1) = 2.1
  ;; Expected result at address[0]: Lower 32 bits of 2.1 as f64
  (func (;25;) (type 0)
    i32.const 0
    f64.const 3.5
    f64.const 2.1
    f64.min
    i64.reinterpret_f64
    i32.wrap_i64
    i32.store)
  
  ;; Test: f64.max - max(3.5, 2.1) = 3.5
  ;; Expected result at address[0]: Lower 32 bits of 3.5 as f64
  (func (;26;) (type 0)
    i32.const 0
    f64.const 3.5
    f64.const 2.1
    f64.max
    i64.reinterpret_f64
    i32.wrap_i64
    i32.store)
  
  ;; Test: f64.abs - abs(-3.5) = 3.5
  ;; Expected result at address[0]: Lower 32 bits of 3.5 as f64
  (func (;27;) (type 0)
    i32.const 0
    f64.const -3.5
    f64.abs
    i64.reinterpret_f64
    i32.wrap_i64
    i32.store)
  
  ;; Test: f64.neg - neg(3.5) = -3.5
  ;; Expected result at address[0]: Lower 32 bits of -3.5 as f64
  (func (;28;) (type 0)
    i32.const 0
    f64.const 3.5
    f64.neg
    i64.reinterpret_f64
    i32.wrap_i64
    i32.store)
  
  ;; Test: f64.ceil - ceil(3.2) = 4.0
  ;; Expected result at address[0]: Lower 32 bits of 4.0 as f64
  (func (;29;) (type 0)
    i32.const 0
    f64.const 3.2
    f64.ceil
    i64.reinterpret_f64
    i32.wrap_i64
    i32.store)
  
  ;; Test: f64.floor - floor(3.8) = 3.0
  ;; Expected result at address[0]: Lower 32 bits of 3.0 as f64
  (func (;30;) (type 0)
    i32.const 0
    f64.const 3.8
    f64.floor
    i64.reinterpret_f64
    i32.wrap_i64
    i32.store)
  
  ;; Test: f64.trunc - trunc(3.8) = 3.0
  ;; Expected result at address[0]: Lower 32 bits of 3.0 as f64
  (func (;31;) (type 0)
    i32.const 0
    f64.const 3.8
    f64.trunc
    i64.reinterpret_f64
    i32.wrap_i64
    i32.store)
  
  ;; Test: f64.nearest - nearest(3.5) = 4.0 (round to even)
  ;; Expected result at address[0]: Lower 32 bits of 4.0 as f64
  (func (;32;) (type 0)
    i32.const 0
    f64.const 3.5
    f64.nearest
    i64.reinterpret_f64
    i32.wrap_i64
    i32.store)
  
  ;; === F64 COMPARISONS ===
  
  ;; Test: f64.le - 2.0 <= 3.0 = 1
  ;; Expected result at address[0]: 1
  (func (;33;) (type 0)
    i32.const 0
    f64.const 2.0
    f64.const 3.0
    f64.le
    i32.store)
  
  ;; Test: f64.ge - 3.0 >= 2.0 = 1
  ;; Expected result at address[0]: 1
  (func (;34;) (type 0)
    i32.const 0
    f64.const 3.0
    f64.const 2.0
    f64.ge
    i32.store)
  
  ;; === FLOAT MEMORY OPERATIONS ===
  
  ;; Test: f32.store and f32.load
  ;; Store 3.14, load it back
  ;; Expected result at address[0]: 3.14 (as f32 bit pattern)
  (func (;35;) (type 0)
    ;; Store f32 at address 100
    i32.const 100
    f32.const 3.14159
    f32.store
    
    ;; Load it back
    i32.const 0
    i32.const 100
    f32.load
    i32.reinterpret_f32
    i32.store)
  
  ;; Test: f32.store with alignment - Store and load negative float
  ;; Expected result at address[0]: -2.5 (as f32 bit pattern)
  (func (;36;) (type 0)
    i32.const 104
    f32.const -2.5
    f32.store
    
    i32.const 0
    i32.const 104
    f32.load
    i32.reinterpret_f32
    i32.store)
  
  ;; Test: f64.store and f64.load
  ;; Store 2.718281828, load it back
  ;; Expected result at address[0]: Lower 32 bits of 2.718281828 as f64
  (func (;37;) (type 0)
    ;; Store f64 at address 200
    i32.const 200
    f64.const 2.718281828
    f64.store
    
    ;; Load it back
    i32.const 0
    i32.const 200
    f64.load
    i64.reinterpret_f64
    i32.wrap_i64
    i32.store)
  
  ;; Test: f64.store with large value
  ;; Expected result at address[0]: Lower 32 bits
  (func (;38;) (type 0)
    i32.const 208
    f64.const 123456.789
    f64.store
    
    i32.const 0
    i32.const 208
    f64.load
    i64.reinterpret_f64
    i32.wrap_i64
    i32.store)
  
  ;; Test: Store and load with arithmetic
  ;; Store 5.5, load, add 2.5, verify = 8.0
  ;; Expected result at address[0]: 8.0 (as f32 bit pattern)
  (func (;39;) (type 0)
    ;; Store
    i32.const 300
    f32.const 5.5
    f32.store
    
    ;; Load, add, store result
    i32.const 0
    i32.const 300
    f32.load
    f32.const 2.5
    f32.add
    i32.reinterpret_f32
    i32.store)
  
  ;; === UNREACHABLE INSTRUCTION ===
  
  ;; Test: Branch before unreachable
  ;; Expected result at address[0]: 42 (unreachable not reached)
  (func (;40;) (type 0)
    i32.const 1
    if
      i32.const 0
      i32.const 42
      i32.store
      return
    end
    unreachable)
  
  ;; Test: Conditional unreachable (not reached)
  ;; Expected result at address[0]: 100 (unreachable not executed)
  (func (;41;) (type 0)
    i32.const 1
    if
      i32.const 0
      i32.const 100
      i32.store
    else
      unreachable
    end)
  
  ;; Test: Unreachable in else branch (not taken)
  ;; Expected result at address[0]: 50
  (func (;42;) (type 0)
    i32.const 0
    i32.eqz
    if
      i32.const 0
      i32.const 50
      i32.store
    else
      unreachable
    end)
  
  (memory (;0;) 2)
  (export "memory" (memory 0))
  
  (export "_start" (func 0))
  (export "_test_i32_rem_u" (func 0))
  (export "_test_i32_rem_u_large" (func 1))
  (export "_test_i64_rem_u" (func 2))
  (export "_test_i64_rem_u_large" (func 3))
  (export "_test_i32_le_u" (func 4))
  (export "_test_i32_le_u_equal" (func 5))
  (export "_test_i32_le_u_large" (func 6))
  (export "_test_i32_ge_u" (func 7))
  (export "_test_i32_ge_u_equal" (func 8))
  (export "_test_i32_ge_u_large" (func 9))
  (export "_test_i64_le_s" (func 10))
  (export "_test_i64_le_s_negative" (func 11))
  (export "_test_i64_le_u" (func 12))
  (export "_test_i64_le_u_large" (func 13))
  (export "_test_i64_ge_s" (func 14))
  (export "_test_i64_ge_s_negative" (func 15))
  (export "_test_i64_ge_u" (func 16))
  (export "_test_i64_ge_u_large" (func 17))
  (export "_test_f32_copysign_neg" (func 18))
  (export "_test_f32_copysign_pos" (func 19))
  (export "_test_f32_copysign_both_pos" (func 20))
  (export "_test_f64_copysign_neg" (func 21))
  (export "_test_f64_copysign_pos" (func 22))
  (export "_test_f64_sub" (func 23))
  (export "_test_f64_div" (func 24))
  (export "_test_f64_min" (func 25))
  (export "_test_f64_max" (func 26))
  (export "_test_f64_abs" (func 27))
  (export "_test_f64_neg" (func 28))
  (export "_test_f64_ceil" (func 29))
  (export "_test_f64_floor" (func 30))
  (export "_test_f64_trunc" (func 31))
  (export "_test_f64_nearest" (func 32))
  (export "_test_f64_le" (func 33))
  (export "_test_f64_ge" (func 34))
  (export "_test_f32_store_load" (func 35))
  (export "_test_f32_store_load_negative" (func 36))
  (export "_test_f64_store_load" (func 37))
  (export "_test_f64_store_load_large" (func 38))
  (export "_test_f32_arithmetic_with_load" (func 39))
  (export "_test_unreachable_not_reached" (func 40))
  (export "_test_unreachable_in_branch" (func 41))
  (export "_test_unreachable_in_else" (func 42))
)
