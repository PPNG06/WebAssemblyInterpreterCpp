;;
;; WebAssembly 1.0 (MVP) Test Suite - Basic i32 Operations
;;
;; This file contains ONLY WebAssembly 1.0 MVP features
;; Compatible with all WASM runtimes and standard Clang output
;;
;; Coverage: i32 arithmetic, bitwise, comparisons, control flow, locals, globals
;;

(module
  (type (;0;) (func))
  
  ;; Global variables for testing
  (global $counter (mut i32) (i32.const 0))
  (global $constant i32 (i32.const 100))
  
  ;; === BASIC ARITHMETIC TESTS ===
  
  ;; Test: Store value 42 at address 0
  ;; Expected result at address[0]: 42
  (func (;0;) (type 0)
    i32.const 0
    i32.const 42
    i32.store)

  ;; Test: Addition - 10 + 5 = 15, store at address 0
  ;; Expected result at address[0]: 15
  (func (;1;) (type 0)
    i32.const 0
    i32.const 10
    i32.const 5
    i32.add
    i32.store)

  ;; Test: Subtraction - 20 - 8 = 12, store at address 0
  ;; Expected result at address[0]: 12
  (func (;2;) (type 0)
    i32.const 0
    i32.const 20
    i32.const 8
    i32.sub
    i32.store)

  ;; Test: Multiplication - 6 * 7 = 42, store at address 0
  ;; Expected result at address[0]: 42
  (func (;3;) (type 0)
    i32.const 0
    i32.const 6
    i32.const 7
    i32.mul
    i32.store)

  ;; Test: Division (signed) - 20 / 4 = 5, store at address 0
  ;; Expected result at address[0]: 5
  (func (;4;) (type 0)
    i32.const 0
    i32.const 20
    i32.const 4
    i32.div_s
    i32.store)

  ;; Test: Division (unsigned) - 20 / 3 = 6, store at address 0
  ;; Expected result at address[0]: 6
  (func (;5;) (type 0)
    i32.const 0
    i32.const 20
    i32.const 3
    i32.div_u
    i32.store)

  ;; Test: Remainder (signed) - 20 % 3 = 2, store at address 0
  ;; Expected result at address[0]: 2
  (func (;6;) (type 0)
    i32.const 0
    i32.const 20
    i32.const 3
    i32.rem_s
    i32.store)

  ;; Test: Bitwise AND - 0b1111 & 0b1010 = 0b1010 (15 & 10 = 10), store at address 0
  ;; Expected result at address[0]: 10
  (func (;7;) (type 0)
    i32.const 0
    i32.const 15
    i32.const 10
    i32.and
    i32.store)

  ;; Test: Bitwise OR - 0b1100 | 0b1010 = 0b1110 (12 | 10 = 14), store at address 0
  ;; Expected result at address[0]: 14
  (func (;8;) (type 0)
    i32.const 0
    i32.const 12
    i32.const 10
    i32.or
    i32.store)

  ;; Test: Bitwise XOR - 0b1100 ^ 0b1010 = 0b0110 (12 ^ 10 = 6), store at address 0
  ;; Expected result at address[0]: 6
  (func (;9;) (type 0)
    i32.const 0
    i32.const 12
    i32.const 10
    i32.xor
    i32.store)

  ;; Test: Left shift - 5 << 2 = 20, store at address 0
  ;; Expected result at address[0]: 20
  (func (;10;) (type 0)
    i32.const 0
    i32.const 5
    i32.const 2
    i32.shl
    i32.store)

  ;; Test: Right shift (signed) - -8 >> 1 = -4, store at address 0
  ;; Expected result at address[0]: -4 (0xFFFFFFFC)
  (func (;11;) (type 0)
    i32.const 0
    i32.const -8
    i32.const 1
    i32.shr_s
    i32.store)

  ;; Test: Right shift (unsigned) - 16 >> 2 = 4, store at address 0
  ;; Expected result at address[0]: 4
  (func (;12;) (type 0)
    i32.const 0
    i32.const 16
    i32.const 2
    i32.shr_u
    i32.store)

  ;; === MEMORY LOAD/STORE TESTS ===

  ;; Test: Store and Load - Store 99 at address 4, load it back and store at address 0
  ;; Expected result at address[0]: 99
  (func (;13;) (type 0)
    ;; First store 99 at address 4
    i32.const 4
    i32.const 99
    i32.store
    
    ;; Load from address 4 and store at address 0
    i32.const 0
    i32.const 4
    i32.load
    i32.store)

  ;; Test: Store byte and load unsigned - Store 255 as byte, load as i32, store at address 0
  ;; Expected result at address[0]: 255
  (func (;14;) (type 0)
    ;; Store byte 255 at address 4
    i32.const 4
    i32.const 255
    i32.store8
    
    ;; Load unsigned byte and store as i32 at address 0
    i32.const 0
    i32.const 4
    i32.load8_u
    i32.store)

  ;; Test: Store byte and load signed - Store -1 (0xFF) as byte, load as signed i32
  ;; Expected result at address[0]: -1 (0xFFFFFFFF)
  (func (;15;) (type 0)
    ;; Store byte -1 at address 4
    i32.const 4
    i32.const -1
    i32.store8
    
    ;; Load signed byte (will be -1) and store at address 0
    i32.const 0
    i32.const 4
    i32.load8_s
    i32.store)

  ;; === LOCAL VARIABLE TESTS ===

  ;; Test: Local variables - Use locals to compute (a + b) * c, store at address 0
  ;; Expected result at address[0]: 35
  (func (;16;) (type 0)
    (local $a i32)
    (local $b i32)
    (local $c i32)
    (local $result i32)
    
    ;; Set local variables
    i32.const 3
    local.set $a
    
    i32.const 4
    local.set $b
    
    i32.const 5
    local.set $c
    
    ;; Compute (a + b) * c = (3 + 4) * 5 = 35
    local.get $a
    local.get $b
    i32.add
    local.get $c
    i32.mul
    local.set $result
    
    ;; Store result at address 0
    i32.const 0
    local.get $result
    i32.store)

  ;; Test: Local variable tee - Use tee to set and use value in one operation
  ;; Expected result at address[0]: 15
  (func (;17;) (type 0)
    (local $x i32)
    (local $y i32)
    
    ;; Set x to 10 and immediately add 5, store result in y
    i32.const 10
    local.tee $x    ;; Sets $x to 10 and keeps 10 on stack
    i32.const 5
    i32.add
    local.set $y    ;; y = 15
    
    ;; Store final result (y = 15) at address 0
    i32.const 0
    local.get $y
    i32.store)

  ;; === GLOBAL VARIABLE TESTS ===

  ;; Test: Global get/set - Increment counter and store at address 0
  ;; Expected result at address[0]: 1 (counter starts at 0, incremented to 1)
  (func (;18;) (type 0)
    ;; Get current counter value, add 1, set back
    global.get $counter
    i32.const 1
    i32.add
    global.set $counter
    
    ;; Store updated counter at address 0
    i32.const 0
    global.get $counter
    i32.store)

  ;; Test: Read constant global - Store constant value at address 0
  ;; Expected result at address[0]: 100
  (func (;19;) (type 0)
    i32.const 0
    global.get $constant
    i32.store)

  ;; Test: Multiple global operations - Increment counter multiple times
  ;; Expected result at address[0]: 10 (counter starts at 0, incremented by 10)
  (func (;20;) (type 0)
    ;; Increment counter by 10
    global.get $counter
    i32.const 10
    i32.add
    global.set $counter
    
    ;; Store at address 0
    i32.const 0
    global.get $counter
    i32.store)

  ;; === COMPLEX TEST ===

  ;; Test: Combined operations - Use locals, globals, memory ops together
  ;; Expected result at address[0]: 142 (42 + 100)
  (func (;21;) (type 0)
    (local $temp i32)
    
    ;; Load value from address 4 (use temp storage to avoid overwriting during test)
    ;; First store 42 at address 4 for this test
    i32.const 4
    i32.const 42
    i32.store
    
    ;; Load it back
    i32.const 4
    i32.load
    local.set $temp
    
    ;; Add global constant to it
    local.get $temp
    global.get $constant
    i32.add
    local.set $temp    ;; temp = 42 + 100 = 142
    
    ;; Store at address 0
    i32.const 0
    local.get $temp
    i32.store)

  ;; === COMPARISON OPERATIONS ===

  ;; Test: Equal - 10 == 10 = 1 (true), store at address 0
  ;; Expected result at address[0]: 1
  (func (;22;) (type 0)
    i32.const 0
    i32.const 10
    i32.const 10
    i32.eq
    i32.store)

  ;; Test: Not equal - 10 != 5 = 1 (true), store at address 0
  ;; Expected result at address[0]: 1
  (func (;23;) (type 0)
    i32.const 0
    i32.const 10
    i32.const 5
    i32.ne
    i32.store)

  ;; Test: Less than signed - 5 < 10 = 1 (true), store at address 0
  ;; Expected result at address[0]: 1
  (func (;24;) (type 0)
    i32.const 0
    i32.const 5
    i32.const 10
    i32.lt_s
    i32.store)

  ;; Test: Less than unsigned - 5 < 10 = 1 (true), store at address 0
  ;; Expected result at address[0]: 1
  (func (;25;) (type 0)
    i32.const 0
    i32.const 5
    i32.const 10
    i32.lt_u
    i32.store)

  ;; Test: Greater than signed - 15 > 10 = 1 (true), store at address 0
  ;; Expected result at address[0]: 1
  (func (;26;) (type 0)
    i32.const 0
    i32.const 15
    i32.const 10
    i32.gt_s
    i32.store)

  ;; Test: Greater than unsigned - 15 > 10 = 1 (true), store at address 0
  ;; Expected result at address[0]: 1
  (func (;27;) (type 0)
    i32.const 0
    i32.const 15
    i32.const 10
    i32.gt_u
    i32.store)

  ;; Test: Less than or equal signed - 10 <= 10 = 1 (true), store at address 0
  ;; Expected result at address[0]: 1
  (func (;28;) (type 0)
    i32.const 0
    i32.const 10
    i32.const 10
    i32.le_s
    i32.store)

  ;; Test: Greater than or equal signed - 10 >= 10 = 1 (true), store at address 0
  ;; Expected result at address[0]: 1
  (func (;29;) (type 0)
    i32.const 0
    i32.const 10
    i32.const 10
    i32.ge_s
    i32.store)

  ;; Test: Equal zero - eqz(0) = 1 (true), store at address 0
  ;; Expected result at address[0]: 1
  (func (;30;) (type 0)
    i32.const 0
    i32.const 0
    i32.eqz
    i32.store)

  ;; Test: Equal zero with non-zero - eqz(5) = 0 (false), store at address 0
  ;; Expected result at address[0]: 0
  (func (;31;) (type 0)
    i32.const 0
    i32.const 5
    i32.eqz
    i32.store)

  ;; === UNARY OPERATIONS ===

  ;; Test: Count leading zeros - clz(0b00001000) = clz(8) = 28, store at address 0
  ;; Expected result at address[0]: 28
  (func (;32;) (type 0)
    i32.const 0
    i32.const 8
    i32.clz
    i32.store)

  ;; Test: Count trailing zeros - ctz(0b00001000) = ctz(8) = 3, store at address 0
  ;; Expected result at address[0]: 3
  (func (;33;) (type 0)
    i32.const 0
    i32.const 8
    i32.ctz
    i32.store)

  ;; Test: Population count - popcnt(0b10110) = popcnt(22) = 3, store at address 0
  ;; Expected result at address[0]: 3
  (func (;34;) (type 0)
    i32.const 0
    i32.const 22
    i32.popcnt
    i32.store)

  ;; Test: Population count all bits - popcnt(0xFFFFFFFF) = 32, store at address 0
  ;; Expected result at address[0]: 32
  (func (;35;) (type 0)
    i32.const 0
    i32.const -1
    i32.popcnt
    i32.store)

  ;; === ROTATE OPERATIONS ===

  ;; Test: Rotate left - rotl(0b00000001, 4) = rotl(1, 4) = 16, store at address 0
  ;; Expected result at address[0]: 16
  (func (;36;) (type 0)
    i32.const 0
    i32.const 1
    i32.const 4
    i32.rotl
    i32.store)

  ;; Test: Rotate right - rotr(0b00010000, 4) = rotr(16, 4) = 1, store at address 0
  ;; Expected result at address[0]: 1
  (func (;37;) (type 0)
    i32.const 0
    i32.const 16
    i32.const 4
    i32.rotr
    i32.store)

  ;; Test: Rotate left with wrap - rotl(0x80000000, 1) wraps to 1
  ;; Expected result at address[0]: 1
  (func (;38;) (type 0)
    i32.const 0
    i32.const 0x80000000
    i32.const 1
    i32.rotl
    i32.store)

  ;; === 16-BIT MEMORY OPERATIONS ===

  ;; Test: Store and load 16-bit unsigned - Store 65535 as i16, load back
  ;; Expected result at address[0]: 65535
  (func (;39;) (type 0)
    ;; Store 16-bit value at address 4
    i32.const 4
    i32.const 65535
    i32.store16
    
    ;; Load unsigned 16-bit and store at address 0
    i32.const 0
    i32.const 4
    i32.load16_u
    i32.store)

  ;; Test: Store and load 16-bit signed - Store -1 as i16, load as signed i32
  ;; Expected result at address[0]: -1 (0xFFFFFFFF)
  (func (;40;) (type 0)
    ;; Store -1 as 16-bit at address 4
    i32.const 4
    i32.const -1
    i32.store16
    
    ;; Load signed 16-bit (will be -1) and store at address 0
    i32.const 0
    i32.const 4
    i32.load16_s
    i32.store)

  ;; Test: Store 16-bit value 32768 and load unsigned = 32768
  ;; Expected result at address[0]: 32768
  (func (;41;) (type 0)
    i32.const 4
    i32.const 32768
    i32.store16
    
    i32.const 0
    i32.const 4
    i32.load16_u
    i32.store)

  ;; === CONTROL FLOW - SELECT ===

  ;; Test: Select instruction - select(10, 20, 1) = 10 (first value when condition is true)
  ;; Expected result at address[0]: 10
  (func (;42;) (type 0)
    i32.const 0
    i32.const 10     ;; Value if condition is true
    i32.const 20     ;; Value if condition is false
    i32.const 1      ;; Condition (1 = true)
    select
    i32.store)

  ;; Test: Select with false condition - select(10, 20, 0) = 20
  ;; Expected result at address[0]: 20
  (func (;43;) (type 0)
    i32.const 0
    i32.const 10     ;; Value if condition is true
    i32.const 20     ;; Value if condition is false
    i32.const 0      ;; Condition (0 = false)
    select
    i32.store)

  ;; === CONTROL FLOW - IF/ELSE ===

  ;; Test: If/else with true condition - returns 100
  ;; Expected result at address[0]: 100
  (func (;44;) (type 0)
    i32.const 0
    i32.const 1      ;; Condition
    if (result i32)
      i32.const 100  ;; Value if true
    else
      i32.const 200  ;; Value if false
    end
    i32.store)

  ;; Test: If/else with false condition - returns 200
  ;; Expected result at address[0]: 200
  (func (;45;) (type 0)
    i32.const 0
    i32.const 0      ;; Condition
    if (result i32)
      i32.const 100  ;; Value if true
    else
      i32.const 200  ;; Value if false
    end
    i32.store)

  ;; Test: If without else - only executes if condition is true
  ;; Expected result at address[0]: 50
  (func (;46;) (type 0)
    ;; Pre-set value to 0
    i32.const 0
    i32.const 0
    i32.store
    
    ;; If condition is true, change to 50
    i32.const 1
    if
      i32.const 0
      i32.const 50
      i32.store
    end)

  ;; Test: Nested if/else - Complex conditional logic
  ;; Expected result at address[0]: 1 (x=15 is between 10 and 20)
  (func (;47;) (type 0)
    (local $x i32)
    i32.const 15
    local.set $x
    
    i32.const 0
    local.get $x
    i32.const 10
    i32.gt_s    ;; Is x > 10?
    if (result i32)
      local.get $x
      i32.const 20
      i32.lt_s  ;; Is x < 20?
      if (result i32)
        i32.const 1  ;; x is between 10 and 20
      else
        i32.const 2  ;; x is >= 20
      end
    else
      i32.const 0    ;; x is <= 10
    end
    i32.store)

  ;; === CONTROL FLOW - BLOCK AND BR ===

  ;; Test: Block with break - Break out of block early
  ;; Expected result at address[0]: 10
  (func (;48;) (type 0)
    i32.const 0
    block (result i32)
      i32.const 10
      i32.const 1
      br_if 0        ;; Break if condition is true
      drop           ;; This won't execute
      i32.const 20
    end
    i32.store)

  ;; Test: Block without break - Execute all instructions
  ;; Expected result at address[0]: 20
  (func (;49;) (type 0)
    i32.const 0
    block (result i32)
      i32.const 10
      i32.const 0
      br_if 0        ;; Won't break (condition is false)
      drop           ;; Drop the 10
      i32.const 20   ;; Return 20 instead
    end
    i32.store)

  ;; === CONTROL FLOW - LOOP ===

  ;; Test: Loop to sum 1+2+3+4+5 = 15
  ;; Expected result at address[0]: 15
  (func (;50;) (type 0)
    (local $i i32)
    (local $sum i32)
    
    i32.const 0
    local.set $sum
    i32.const 1
    local.set $i
    
    block $break
      loop $continue
        ;; Add i to sum
        local.get $sum
        local.get $i
        i32.add
        local.set $sum
        
        ;; Increment i
        local.get $i
        i32.const 1
        i32.add
        local.set $i
        
        ;; Continue if i <= 5
        local.get $i
        i32.const 6
        i32.lt_s
        br_if $continue
      end
    end
    
    ;; Store result
    i32.const 0
    local.get $sum
    i32.store)

  ;; Test: Loop with early break - Sum until value > 10
  ;; Expected result at address[0]: 15 (1+2+3+4+5=15, breaks when sum>10)
  (func (;51;) (type 0)
    (local $i i32)
    (local $sum i32)
    
    i32.const 0
    local.set $sum
    i32.const 1
    local.set $i
    
    block $break
      loop $continue
        ;; Add i to sum
        local.get $sum
        local.get $i
        i32.add
        local.set $sum
        
        ;; Break if sum > 10
        local.get $sum
        i32.const 10
        i32.gt_s
        br_if $break
        
        ;; Increment i
        local.get $i
        i32.const 1
        i32.add
        local.set $i
        
        ;; Continue loop
        br $continue
      end
    end
    
    ;; Store result
    i32.const 0
    local.get $sum
    i32.store)

  ;; === CONTROL FLOW - BR_TABLE (Switch/Case) ===

  ;; Test: Branch table - Switch statement (input: 0)
  ;; Expected result at address[0]: 100
  (func (;52;) (type 0)
    (local $result i32)
    
    block $case_default
      block $case_2
        block $case_1
          block $case_0
            i32.const 0      ;; Index for br_table
            br_table $case_0 $case_1 $case_2 $case_default
          end
          ;; case 0
          i32.const 100
          local.set $result
          br $case_default
        end
        ;; case 1
        i32.const 101
        local.set $result
        br $case_default
      end
      ;; case 2
      i32.const 102
      local.set $result
    end
    
    ;; Store result
    i32.const 0
    local.get $result
    i32.store)

  ;; Test: Branch table - Switch statement (input: 2)
  ;; Expected result at address[0]: 102
  (func (;53;) (type 0)
    (local $result i32)
    
    block $case_default
      block $case_2
        block $case_1
          block $case_0
            i32.const 2      ;; Index for br_table
            br_table $case_0 $case_1 $case_2 $case_default
          end
          ;; case 0
          i32.const 100
          local.set $result
          br $case_default
        end
        ;; case 1
        i32.const 101
        local.set $result
        br $case_default
      end
      ;; case 2
      i32.const 102
      local.set $result
    end
    
    ;; Store result
    i32.const 0
    local.get $result
    i32.store)

  (memory (;0;) 1)
  (export "memory" (memory 0))
  (export "counter" (global $counter))
  (export "constant" (global $constant))
  
  (export "_start" (func 0))
  (export "_test_store" (func 0))
  (export "_test_addition" (func 1))
  (export "_test_subtraction" (func 2))
  (export "_test_multiplication" (func 3))
  (export "_test_division_signed" (func 4))
  (export "_test_division_unsigned" (func 5))
  (export "_test_remainder" (func 6))
  (export "_test_and" (func 7))
  (export "_test_or" (func 8))
  (export "_test_xor" (func 9))
  (export "_test_shift_left" (func 10))
  (export "_test_shift_right_signed" (func 11))
  (export "_test_shift_right_unsigned" (func 12))
  (export "_test_store_load" (func 13))
  (export "_test_store_load_byte_unsigned" (func 14))
  (export "_test_store_load_byte_signed" (func 15))
  (export "_test_locals_arithmetic" (func 16))
  (export "_test_locals_tee" (func 17))
  (export "_test_global_increment" (func 18))
  (export "_test_global_constant" (func 19))
  (export "_test_global_multiple" (func 20))
  (export "_test_combined" (func 21))
  (export "_test_eq" (func 22))
  (export "_test_ne" (func 23))
  (export "_test_lt_s" (func 24))
  (export "_test_lt_u" (func 25))
  (export "_test_gt_s" (func 26))
  (export "_test_gt_u" (func 27))
  (export "_test_le_s" (func 28))
  (export "_test_ge_s" (func 29))
  (export "_test_eqz_zero" (func 30))
  (export "_test_eqz_nonzero" (func 31))
  (export "_test_clz" (func 32))
  (export "_test_ctz" (func 33))
  (export "_test_popcnt" (func 34))
  (export "_test_popcnt_all" (func 35))
  (export "_test_rotl" (func 36))
  (export "_test_rotr" (func 37))
  (export "_test_rotl_wrap" (func 38))
  (export "_test_load16_u" (func 39))
  (export "_test_load16_s" (func 40))
  (export "_test_load16_32768" (func 41))
  (export "_test_select_true" (func 42))
  (export "_test_select_false" (func 43))
  (export "_test_if_true" (func 44))
  (export "_test_if_false" (func 45))
  (export "_test_if_no_else" (func 46))
  (export "_test_nested_if" (func 47))
  (export "_test_block_break" (func 48))
  (export "_test_block_no_break" (func 49))
  (export "_test_loop_sum" (func 50))
  (export "_test_loop_early_break" (func 51))
  (export "_test_br_table_case0" (func 52))
  (export "_test_br_table_case2" (func 53))
)