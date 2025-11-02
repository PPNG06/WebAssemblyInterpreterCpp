;;
;; WebAssembly Nontrapping Float-to-Int Conversion Tests (0xFC prefix)
;;
;; This file tests the saturating truncation instructions (nontrapping-fptoint proposal)
;; These instructions convert floating-point to integer with saturation instead of trapping
;;
;; Coverage: i32.trunc_sat_f32_s/u, i32.trunc_sat_f64_s/u, i64.trunc_sat_f32_s/u, i64.trunc_sat_f64_s/u
;;

(module
  (type (;0;) (func))
  
  ;; === i32.trunc_sat_f32_s TESTS ===
  
  ;; Test: i32.trunc_sat_f32_s - Normal conversion (10.5 -> 10)
  ;; Expected result at address[0]: 10
  (func (;0;) (type 0)
    i32.const 0
    f32.const 10.5
    i32.trunc_sat_f32_s
    i32.store)
  
  ;; Test: i32.trunc_sat_f32_s - Negative value (-5.8 -> -5)
  ;; Expected result at address[0]: -5
  (func (;1;) (type 0)
    i32.const 0
    f32.const -5.8
    i32.trunc_sat_f32_s
    i32.store)
  
  ;; Test: i32.trunc_sat_f32_s - NaN saturates to 0
  ;; Expected result at address[0]: 0
  (func (;2;) (type 0)
    i32.const 0
    f32.const nan
    i32.trunc_sat_f32_s
    i32.store)
  
  ;; Test: i32.trunc_sat_f32_s - Overflow saturates to INT32_MAX (2147483647)
  ;; Expected result at address[0]: 2147483647
  (func (;3;) (type 0)
    i32.const 0
    f32.const 3e9    ;; 3 billion, exceeds INT32_MAX
    i32.trunc_sat_f32_s
    i32.store)
  
  ;; Test: i32.trunc_sat_f32_s - Underflow saturates to INT32_MIN (-2147483648)
  ;; Expected result at address[0]: -2147483648
  (func (;4;) (type 0)
    i32.const 0
    f32.const -3e9   ;; -3 billion, below INT32_MIN
    i32.trunc_sat_f32_s
    i32.store)
  
  ;; === i32.trunc_sat_f32_u TESTS ===
  
  ;; Test: i32.trunc_sat_f32_u - Normal conversion (42.7 -> 42)
  ;; Expected result at address[0]: 42
  (func (;5;) (type 0)
    i32.const 0
    f32.const 42.7
    i32.trunc_sat_f32_u
    i32.store)
  
  ;; Test: i32.trunc_sat_f32_u - NaN saturates to 0
  ;; Expected result at address[0]: 0
  (func (;6;) (type 0)
    i32.const 0
    f32.const nan
    i32.trunc_sat_f32_u
    i32.store)
  
  ;; Test: i32.trunc_sat_f32_u - Negative saturates to 0
  ;; Expected result at address[0]: 0
  (func (;7;) (type 0)
    i32.const 0
    f32.const -5.5
    i32.trunc_sat_f32_u
    i32.store)
  
  ;; Test: i32.trunc_sat_f32_u - Overflow saturates to UINT32_MAX (4294967295)
  ;; Expected result at address[0]: 4294967295 (0xFFFFFFFF = -1 as signed)
  (func (;8;) (type 0)
    i32.const 0
    f32.const 5e9    ;; 5 billion, exceeds UINT32_MAX
    i32.trunc_sat_f32_u
    i32.store)
  
  ;; === i32.trunc_sat_f64_s TESTS ===
  
  ;; Test: i32.trunc_sat_f64_s - Normal conversion (123.456 -> 123)
  ;; Expected result at address[0]: 123
  (func (;9;) (type 0)
    i32.const 0
    f64.const 123.456
    i32.trunc_sat_f64_s
    i32.store)
  
  ;; Test: i32.trunc_sat_f64_s - Negative value (-99.99 -> -99)
  ;; Expected result at address[0]: -99
  (func (;10;) (type 0)
    i32.const 0
    f64.const -99.99
    i32.trunc_sat_f64_s
    i32.store)
  
  ;; Test: i32.trunc_sat_f64_s - NaN saturates to 0
  ;; Expected result at address[0]: 0
  (func (;11;) (type 0)
    i32.const 0
    f64.const nan
    i32.trunc_sat_f64_s
    i32.store)
  
  ;; Test: i32.trunc_sat_f64_s - Overflow saturates to INT32_MAX (2147483647)
  ;; Expected result at address[0]: 2147483647
  (func (;12;) (type 0)
    i32.const 0
    f64.const 1e100  ;; Way beyond INT32_MAX
    i32.trunc_sat_f64_s
    i32.store)
  
  ;; Test: i32.trunc_sat_f64_s - Underflow saturates to INT32_MIN (-2147483648)
  ;; Expected result at address[0]: -2147483648
  (func (;13;) (type 0)
    i32.const 0
    f64.const -1e100 ;; Way below INT32_MIN
    i32.trunc_sat_f64_s
    i32.store)
  
  ;; === i32.trunc_sat_f64_u TESTS ===
  
  ;; Test: i32.trunc_sat_f64_u - Normal conversion (255.9 -> 255)
  ;; Expected result at address[0]: 255
  (func (;14;) (type 0)
    i32.const 0
    f64.const 255.9
    i32.trunc_sat_f64_u
    i32.store)
  
  ;; Test: i32.trunc_sat_f64_u - NaN saturates to 0
  ;; Expected result at address[0]: 0
  (func (;15;) (type 0)
    i32.const 0
    f64.const nan
    i32.trunc_sat_f64_u
    i32.store)
  
  ;; Test: i32.trunc_sat_f64_u - Negative saturates to 0
  ;; Expected result at address[0]: 0
  (func (;16;) (type 0)
    i32.const 0
    f64.const -123.456
    i32.trunc_sat_f64_u
    i32.store)
  
  ;; Test: i32.trunc_sat_f64_u - Overflow saturates to UINT32_MAX (4294967295)
  ;; Expected result at address[0]: 4294967295 (0xFFFFFFFF = -1 as signed)
  (func (;17;) (type 0)
    i32.const 0
    f64.const 1e100  ;; Way beyond UINT32_MAX
    i32.trunc_sat_f64_u
    i32.store)
  
  ;; === i64.trunc_sat_f32_s TESTS ===
  
  ;; Test: i64.trunc_sat_f32_s - Normal conversion (42.5 -> 42)
  ;; Expected result at address[0]: 42 (stored as i64, read as i32 for testing)
  (func (;18;) (type 0)
    i32.const 0
    f32.const 42.5
    i64.trunc_sat_f32_s
    i64.store
    
    ;; Also store low 32 bits at address 0 for test validation
    i32.const 0
    i32.const 0
    i64.load
    i32.wrap_i64
    i32.store)
  
  ;; Test: i64.trunc_sat_f32_s - Negative value (-7.3 -> -7)
  ;; Expected result at address[0]: -7
  (func (;19;) (type 0)
    i32.const 0
    f32.const -7.3
    i64.trunc_sat_f32_s
    i64.store
    
    i32.const 0
    i32.const 0
    i64.load
    i32.wrap_i64
    i32.store)
  
  ;; Test: i64.trunc_sat_f32_s - NaN saturates to 0
  ;; Expected result at address[0]: 0
  (func (;20;) (type 0)
    i32.const 0
    f32.const nan
    i64.trunc_sat_f32_s
    i64.store
    
    i32.const 0
    i32.const 0
    i64.load
    i32.wrap_i64
    i32.store)
  
  ;; === i64.trunc_sat_f32_u TESTS ===
  
  ;; Test: i64.trunc_sat_f32_u - Normal conversion (100.9 -> 100)
  ;; Expected result at address[0]: 100
  (func (;21;) (type 0)
    i32.const 0
    f32.const 100.9
    i64.trunc_sat_f32_u
    i64.store
    
    i32.const 0
    i32.const 0
    i64.load
    i32.wrap_i64
    i32.store)
  
  ;; Test: i64.trunc_sat_f32_u - NaN saturates to 0
  ;; Expected result at address[0]: 0
  (func (;22;) (type 0)
    i32.const 0
    f32.const nan
    i64.trunc_sat_f32_u
    i64.store
    
    i32.const 0
    i32.const 0
    i64.load
    i32.wrap_i64
    i32.store)
  
  ;; Test: i64.trunc_sat_f32_u - Negative saturates to 0
  ;; Expected result at address[0]: 0
  (func (;23;) (type 0)
    i32.const 0
    f32.const -42.5
    i64.trunc_sat_f32_u
    i64.store
    
    i32.const 0
    i32.const 0
    i64.load
    i32.wrap_i64
    i32.store)
  
  ;; === i64.trunc_sat_f64_s TESTS ===
  
  ;; Test: i64.trunc_sat_f64_s - Normal conversion (1234.567 -> 1234)
  ;; Expected result at address[0]: 1234
  (func (;24;) (type 0)
    i32.const 0
    f64.const 1234.567
    i64.trunc_sat_f64_s
    i64.store
    
    i32.const 0
    i32.const 0
    i64.load
    i32.wrap_i64
    i32.store)
  
  ;; Test: i64.trunc_sat_f64_s - Negative value (-500.5 -> -500)
  ;; Expected result at address[0]: -500
  (func (;25;) (type 0)
    i32.const 0
    f64.const -500.5
    i64.trunc_sat_f64_s
    i64.store
    
    i32.const 0
    i32.const 0
    i64.load
    i32.wrap_i64
    i32.store)
  
  ;; Test: i64.trunc_sat_f64_s - NaN saturates to 0
  ;; Expected result at address[0]: 0
  (func (;26;) (type 0)
    i32.const 0
    f64.const nan
    i64.trunc_sat_f64_s
    i64.store
    
    i32.const 0
    i32.const 0
    i64.load
    i32.wrap_i64
    i32.store)
  
  ;; === i64.trunc_sat_f64_u TESTS ===
  
  ;; Test: i64.trunc_sat_f64_u - Normal conversion (9999.1 -> 9999)
  ;; Expected result at address[0]: 9999
  (func (;27;) (type 0)
    i32.const 0
    f64.const 9999.1
    i64.trunc_sat_f64_u
    i64.store
    
    i32.const 0
    i32.const 0
    i64.load
    i32.wrap_i64
    i32.store)
  
  ;; Test: i64.trunc_sat_f64_u - NaN saturates to 0
  ;; Expected result at address[0]: 0
  (func (;28;) (type 0)
    i32.const 0
    f64.const nan
    i64.trunc_sat_f64_u
    i64.store
    
    i32.const 0
    i32.const 0
    i64.load
    i32.wrap_i64
    i32.store)
  
  ;; Test: i64.trunc_sat_f64_u - Negative saturates to 0
  ;; Expected result at address[0]: 0
  (func (;29;) (type 0)
    i32.const 0
    f64.const -999.9
    i64.trunc_sat_f64_u
    i64.store
    
    i32.const 0
    i32.const 0
    i64.load
    i32.wrap_i64
    i32.store)
  
  ;; === EDGE CASE TESTS ===
  
  ;; Test: Zero conversion f32 -> i32
  ;; Expected result at address[0]: 0
  (func (;30;) (type 0)
    i32.const 0
    f32.const 0.0
    i32.trunc_sat_f32_s
    i32.store)
  
  ;; Test: Very small positive f32 -> i32 (0.1 -> 0)
  ;; Expected result at address[0]: 0
  (func (;31;) (type 0)
    i32.const 0
    f32.const 0.1
    i32.trunc_sat_f32_s
    i32.store)
  
  ;; Test: Negative zero f64 -> i32
  ;; Expected result at address[0]: 0
  (func (;32;) (type 0)
    i32.const 0
    f64.const -0.0
    i32.trunc_sat_f64_s
    i32.store)
  
  ;; Test: Large positive value within range (1000000.5 -> 1000000)
  ;; Expected result at address[0]: 1000000
  (func (;33;) (type 0)
    i32.const 0
    f64.const 1000000.5
    i32.trunc_sat_f64_s
    i32.store)
  
  (memory (;0;) 1)
  (export "memory" (memory 0))
  
  (export "_start" (func 0))
  (export "_test_i32_trunc_sat_f32_s_normal" (func 0))
  (export "_test_i32_trunc_sat_f32_s_negative" (func 1))
  (export "_test_i32_trunc_sat_f32_s_nan" (func 2))
  (export "_test_i32_trunc_sat_f32_s_overflow" (func 3))
  (export "_test_i32_trunc_sat_f32_s_underflow" (func 4))
  (export "_test_i32_trunc_sat_f32_u_normal" (func 5))
  (export "_test_i32_trunc_sat_f32_u_nan" (func 6))
  (export "_test_i32_trunc_sat_f32_u_negative" (func 7))
  (export "_test_i32_trunc_sat_f32_u_overflow" (func 8))
  (export "_test_i32_trunc_sat_f64_s_normal" (func 9))
  (export "_test_i32_trunc_sat_f64_s_negative" (func 10))
  (export "_test_i32_trunc_sat_f64_s_nan" (func 11))
  (export "_test_i32_trunc_sat_f64_s_overflow" (func 12))
  (export "_test_i32_trunc_sat_f64_s_underflow" (func 13))
  (export "_test_i32_trunc_sat_f64_u_normal" (func 14))
  (export "_test_i32_trunc_sat_f64_u_nan" (func 15))
  (export "_test_i32_trunc_sat_f64_u_negative" (func 16))
  (export "_test_i32_trunc_sat_f64_u_overflow" (func 17))
  (export "_test_i64_trunc_sat_f32_s_normal" (func 18))
  (export "_test_i64_trunc_sat_f32_s_negative" (func 19))
  (export "_test_i64_trunc_sat_f32_s_nan" (func 20))
  (export "_test_i64_trunc_sat_f32_u_normal" (func 21))
  (export "_test_i64_trunc_sat_f32_u_nan" (func 22))
  (export "_test_i64_trunc_sat_f32_u_negative" (func 23))
  (export "_test_i64_trunc_sat_f64_s_normal" (func 24))
  (export "_test_i64_trunc_sat_f64_s_negative" (func 25))
  (export "_test_i64_trunc_sat_f64_s_nan" (func 26))
  (export "_test_i64_trunc_sat_f64_u_normal" (func 27))
  (export "_test_i64_trunc_sat_f64_u_nan" (func 28))
  (export "_test_i64_trunc_sat_f64_u_negative" (func 29))
  (export "_test_zero_f32" (func 30))
  (export "_test_small_f32" (func 31))
  (export "_test_negzero_f64" (func 32))
  (export "_test_large_in_range" (func 33))
)


