;;
;; POST-MVP WebAssembly Features Test Suite
;;
;; This file contains features that are NOT in WebAssembly 1.0 (MVP)
;; These features were added in later proposals (2020+):
;;
;; 1. MULTIPLE RETURN VALUES (multi-value proposal, 2020)
;; 2. BULK MEMORY OPERATIONS (bulk-memory-operations proposal, 2020)
;;    - memory.copy, memory.fill
;; 3. REFERENCE TYPES (reference-types proposal, 2020)
;;    - ref.null, ref.func, ref.is_null
;;    - table.get, table.set, table.size, table.grow, table.fill, table.copy
;;    - externref type
;;
;; These are NOT output by standard Clang/C++ compilation to WASM 1.0
;; Skip this file if you only want pure MVP (1.0) support
;;

(module
  (type (;0;) (func))
  (type (;1;) (func (param i32 i32) (result i32 i32)))  ;; Multiple returns (POST-MVP)
  (type (;2;) (func (result i32 i32)))
  (type (;3;) (func (result i32 i32 i32)))
  (type (;4;) (func (param i32) (result i32 i32)))
  (type (;5;) (func (param funcref)))
  (type (;6;) (func (param externref)))
  (type (;7;) (func (result funcref)))
  (type (;8;) (func (result externref)))
  (type (;9;) (func (param i32) (result i32)))
  
  ;; === REFERENCE TYPES ===
  ;; Function table with funcref
  (table $functable 8 funcref)
  
  ;; External references table
  (table $externrefs 4 externref)
  
  ;; Declare helper functions for ref.func usage
  (elem declare func $helper_add $helper_mul $helper_square)
  
  ;; Global references
  (global $stored_funcref (mut funcref) (ref.null func))
  (global $stored_externref (mut externref) (ref.null extern))
  
  ;; === DATA SEGMENTS FOR BULK OPERATIONS ===
  (data $source (i32.const 1000) "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789")
  (data $pattern (i32.const 2000) "\01\02\03\04\05\06\07\08\09\0a")
  
  ;; === HELPER FUNCTIONS FOR REFERENCE TYPES ===
  
  (func $helper_add (type 9) (param $x i32) (result i32)
    local.get $x
    i32.const 10
    i32.add)
  
  (func $helper_mul (type 9) (param $x i32) (result i32)
    local.get $x
    i32.const 2
    i32.mul)
  
  (func $helper_square (type 9) (param $x i32) (result i32)
    local.get $x
    local.get $x
    i32.mul)
  
  ;; === MULTIPLE RETURN VALUE TESTS ===
  
  ;; Helper: Returns two values
  (func $return_two (type 2) (result i32 i32)
    i32.const 42
    i32.const 100)
  
  ;; Helper: Returns three values
  (func $return_three (type 3) (result i32 i32 i32)
    i32.const 10
    i32.const 20
    i32.const 30)
  
  ;; Helper: Swap two values
  (func $swap (type 1) (param $a i32) (param $b i32) (result i32 i32)
    local.get $b
    local.get $a)
  
  ;; Helper: Divmod - returns quotient and remainder
  (func $divmod (type 1) (param $a i32) (param $b i32) (result i32 i32)
    local.get $a
    local.get $b
    i32.div_s
    local.get $a
    local.get $b
    i32.rem_s)
  
  ;; Helper: Min and max
  (func $minmax (type 1) (param $a i32) (param $b i32) (result i32 i32)
    ;; Return min, max
    local.get $a
    local.get $b
    i32.lt_s
    if (result i32)
      local.get $a
    else
      local.get $b
    end
    
    local.get $a
    local.get $b
    i32.gt_s
    if (result i32)
      local.get $a
    else
      local.get $b
    end)
  
  ;; Test: Receive two return values
  ;; Expected result at address[3000]: 42, at [3004]: 100
  (func (;0;) (type 0)
    (local $first i32)
    (local $second i32)
    
    call $return_two
    local.set $second
    local.set $first
    
    i32.const 3000
    local.get $first
    i32.store
    
    i32.const 3004
    local.get $second
    i32.store)
  
  ;; Test: Receive three return values
  ;; Expected result at address[3000]: 10, [3004]: 20, [3008]: 30
  (func (;1;) (type 0)
    (local $a i32)
    (local $b i32)
    (local $c i32)
    
    call $return_three
    local.set $c
    local.set $b
    local.set $a
    
    i32.const 3000
    local.get $a
    i32.store
    
    i32.const 3004
    local.get $b
    i32.store
    
    i32.const 3008
    local.get $c
    i32.store)
  
  ;; Test: Use swap function
  ;; Expected result at address[3000]: 20 (was 10), [3004]: 10 (was 20)
  (func (;2;) (type 0)
    i32.const 10
    i32.const 20
    call $swap
    
    i32.const 3004
    i32.store
    
    i32.const 3000
    i32.store)
  
  ;; Test: Divmod 17 / 5 = quotient 3, remainder 2
  ;; Expected result at address[3000]: 3, [3004]: 2
  (func (;3;) (type 0)
    i32.const 17
    i32.const 5
    call $divmod
    
    i32.const 3004
    i32.store
    
    i32.const 3000
    i32.store)
  
  ;; Test: Minmax of 15 and 7
  ;; Expected result at address[3000]: 7 (min), [3004]: 15 (max)
  (func (;4;) (type 0)
    i32.const 15
    i32.const 7
    call $minmax
    
    i32.const 3004
    i32.store
    
    i32.const 3000
    i32.store)
  
  ;; Test: Chain multiple return values
  ;; Expected result at address[3000]: 100, [3004]: 42 (swapped)
  (func (;5;) (type 0)
    call $return_two
    call $swap
    
    i32.const 3004
    i32.store
    
    i32.const 3000
    i32.store)
  
  ;; Test: Use one value, discard another
  ;; Expected result at address[3000]: 42
  (func (;6;) (type 0)
    call $return_two
    drop  ;; Discard second value
    
    i32.const 3000
    i32.store)
  
  ;; === BULK MEMORY OPERATIONS ===
  
  ;; Test: memory.copy - copy 10 bytes from source to dest
  ;; Expected result at address[3000]: 'A' (65)
  (func (;7;) (type 0)
    ;; Copy from address 1000 to 3100, 10 bytes
    i32.const 3100
    i32.const 1000
    i32.const 10
    memory.copy
    
    ;; Verify first byte
    i32.const 3000
    i32.const 3100
    i32.load8_u
    i32.store)
  
  ;; Test: memory.copy - verify copied data
  ;; Expected result at address[3000]: 'C' (67)
  (func (;8;) (type 0)
    i32.const 3100
    i32.const 1000
    i32.const 10
    memory.copy
    
    ;; Check third character
    i32.const 3000
    i32.const 3102
    i32.load8_u
    i32.store)
  
  ;; Test: memory.fill - fill region with byte value
  ;; Expected result at address[3000]: 0xFF
  (func (;9;) (type 0)
    ;; Fill 20 bytes at address 3200 with 0xFF
    i32.const 3200
    i32.const 0xFF
    i32.const 20
    memory.fill
    
    ;; Verify
    i32.const 3000
    i32.const 3200
    i32.load8_u
    i32.store)
  
  ;; Test: memory.fill - verify filled region
  ;; Expected result at address[3000]: 0xFF
  (func (;10;) (type 0)
    i32.const 3200
    i32.const 0xFF
    i32.const 20
    memory.fill
    
    ;; Check 10th byte
    i32.const 3000
    i32.const 3209
    i32.load8_u
    i32.store)
  
  ;; Test: memory.fill with different value
  ;; Expected result at address[3000]: 0x42
  (func (;11;) (type 0)
    i32.const 3300
    i32.const 0x42
    i32.const 15
    memory.fill
    
    i32.const 3000
    i32.const 3300
    i32.load8_u
    i32.store)
  
  ;; Test: Copy overlapping regions (forward)
  ;; Expected result at address[3000]: First byte of copied region
  (func (;12;) (type 0)
    ;; Setup source data
    i32.const 3400
    i32.const 0x01
    i32.store8
    
    i32.const 3401
    i32.const 0x02
    i32.store8
    
    i32.const 3402
    i32.const 0x03
    i32.store8
    
    ;; Copy to overlapping region
    i32.const 3402
    i32.const 3400
    i32.const 2
    memory.copy
    
    ;; Verify
    i32.const 3000
    i32.const 3402
    i32.load8_u
    i32.store)
  
  ;; Test: Bulk copy entire string
  ;; Expected result at address[3000]: 'Z' (90)
  (func (;13;) (type 0)
    ;; Copy alphabet
    i32.const 3500
    i32.const 1000
    i32.const 26
    memory.copy
    
    ;; Get 'Z' (26th letter, index 25)
    i32.const 3000
    i32.const 3525
    i32.load8_u
    i32.store)
  
  ;; Test: Fill then verify range
  ;; Expected result at address[3000]: 0xAB
  (func (;14;) (type 0)
    i32.const 3600
    i32.const 0xAB
    i32.const 100
    memory.fill
    
    ;; Check middle of range
    i32.const 3000
    i32.const 3650
    i32.load8_u
    i32.store)
  
  ;; Test: Copy and modify
  ;; Expected result at address[3000]: 'B' + 1 = 'C' (67)
  (func (;15;) (type 0)
    ;; Copy
    i32.const 3700
    i32.const 1000
    i32.const 5
    memory.copy
    
    ;; Modify second byte
    i32.const 3701
    i32.const 3701
    i32.load8_u
    i32.const 1
    i32.add
    i32.store8
    
    ;; Verify
    i32.const 3000
    i32.const 3701
    i32.load8_u
    i32.store)
  
  ;; === REFERENCE TYPES TESTS ===
  
  ;; Test: ref.null and ref.is_null for funcref
  ;; Expected result at address[3000]: 1 (true, is null)
  (func (;16;) (type 0)
    i32.const 3000
    ref.null func
    ref.is_null
    i32.store)
  
  ;; Test: ref.null and ref.is_null for externref
  ;; Expected result at address[3000]: 1 (true, is null)
  (func (;17;) (type 0)
    i32.const 3000
    ref.null extern
    ref.is_null
    i32.store)
  
  ;; Test: ref.func - get function reference
  ;; Expected result at address[3000]: 0 (not null)
  (func (;18;) (type 0)
    i32.const 3000
    ref.func $helper_add
    ref.is_null
    i32.store)
  
  ;; Test: Store and load funcref in global
  ;; Expected result at address[3000]: 0 (not null after storing)
  (func (;19;) (type 0)
    ;; Store function reference
    ref.func $helper_mul
    global.set $stored_funcref
    
    ;; Check if null
    i32.const 3000
    global.get $stored_funcref
    ref.is_null
    i32.store)
  
  ;; Test: table.get and table.set with funcref
  ;; Expected result at address[3000]: 0 (not null)
  (func (;20;) (type 0)
    ;; Set function in table
    i32.const 0
    ref.func $helper_square
    table.set $functable
    
    ;; Get it back
    i32.const 3000
    i32.const 0
    table.get $functable
    ref.is_null
    i32.store)
  
  ;; Test: table.get null slot
  ;; Expected result at address[3000]: 1 (is null)
  (func (;21;) (type 0)
    i32.const 3000
    i32.const 5  ;; Empty slot
    table.get $functable
    ref.is_null
    i32.store)
  
  ;; Test: table.size
  ;; Expected result at address[3000]: 8 (functable size)
  (func (;22;) (type 0)
    i32.const 3000
    table.size $functable
    i32.store)
  
  ;; Test: table.grow
  ;; Expected result at address[3000]: 8 (old size before grow)
  (func (;23;) (type 0)
    i32.const 3000
    ref.null func
    i32.const 2
    table.grow $functable
    i32.store)
  
  ;; Test: table.size after grow
  ;; Expected result at address[3000]: 10 (if previous test grew by 2)
  (func (;24;) (type 0)
    i32.const 3000
    table.size $functable
    i32.store)
  
  ;; Test: table.fill with null
  ;; Expected result at address[3000]: 1 (is null after fill)
  (func (;25;) (type 0)
    ;; Fill slots 0-2 with null
    i32.const 0
    ref.null func
    i32.const 3
    table.fill $functable
    
    ;; Verify slot 1 is null
    i32.const 3000
    i32.const 1
    table.get $functable
    ref.is_null
    i32.store)
  
  ;; Test: table.copy (copy within same table)
  ;; Expected result at address[3000]: 0 (function copied, not null)
  (func (;26;) (type 0)
    ;; Put function in slot 0
    i32.const 0
    ref.func $helper_add
    table.set $functable
    
    ;; Copy slot 0 to slot 2
    i32.const 2  ;; dest
    i32.const 0  ;; src
    i32.const 1  ;; count
    table.copy $functable $functable
    
    ;; Verify slot 2 has function
    i32.const 3000
    i32.const 2
    table.get $functable
    ref.is_null
    i32.store)
  
  ;; Test: externref in global
  ;; Expected result at address[3000]: 1 (is null initially)
  (func (;27;) (type 0)
    i32.const 3000
    global.get $stored_externref
    ref.is_null
    i32.store)
  
  ;; Test: Store externref null in global
  ;; Expected result at address[3000]: 1 (is null)
  (func (;28;) (type 0)
    ref.null extern
    global.set $stored_externref
    
    i32.const 3000
    global.get $stored_externref
    ref.is_null
    i32.store)
  
  ;; Test: externref table operations
  ;; Expected result at address[3000]: 4 (externrefs table size)
  (func (;29;) (type 0)
    i32.const 3000
    table.size $externrefs
    i32.store)
  
  ;; === COMBINED TESTS ===
  
  ;; Test: Multiple returns + bulk memory
  ;; Copy data, return multiple values based on it
  ;; Expected result at address[3000]: 65 ('A'), [3004]: 66 ('B')
  (func (;30;) (type 0)
    ;; Copy data
    i32.const 4000
    i32.const 1000
    i32.const 10
    memory.copy
    
    ;; Load two characters
    i32.const 4000
    i32.load8_u
    
    i32.const 4001
    i32.load8_u
    
    ;; Store them
    i32.const 3004
    i32.store
    
    i32.const 3000
    i32.store)
  
  ;; Test: Table reference + multiple return values
  ;; Expected result at address[3000]: result of operation
  (func (;31;) (type 0)
    (local $is_present i32)
    (local $table_size i32)
    ;; Store function in table
    i32.const 3
    ref.func $helper_add
    table.set $functable
    
    ;; Verify it's there and capture status values
    i32.const 3
    table.get $functable
    ref.is_null
    i32.eqz  ;; 1 if not null
    local.set $is_present
    
    table.size $functable
    local.set $table_size
    
    ;; Store results
    i32.const 3000
    local.get $table_size
    i32.store
    
    i32.const 3004
    local.get $is_present
    i32.store)
  
  ;; Test: Fill memory, copy it, return info about it
  ;; Expected result at address[3000]: 0x77, [3004]: 0x77
  (func (;32;) (type 0)
    ;; Fill region
    i32.const 4100
    i32.const 0x77
    i32.const 50
    memory.fill
    
    ;; Copy to another location
    i32.const 4200
    i32.const 4100
    i32.const 50
    memory.copy
    
    ;; Verify both locations
    i32.const 4100
    i32.load8_u
    
    i32.const 4200
    i32.load8_u
    
    i32.const 3004
    i32.store
    
    i32.const 3000
    i32.store)
  
  ;; Test: Multiple operations with references
  ;; Expected result at address[3000]: Combined result
  (func (;33;) (type 0)
    (local $size1 i32)
    (local $size2 i32)
    
    ;; Get table sizes
    table.size $functable
    local.set $size1
    
    table.size $externrefs
    local.set $size2
    
    ;; Return sum
    i32.const 3000
    local.get $size1
    local.get $size2
    i32.add
    i32.store)
  
  ;; Test: Swap with data from bulk copy
  ;; Expected result at address[3000]: Second char, [3004]: First char
  (func (;34;) (type 0)
    ;; Copy data
    i32.const 4300
    i32.const 1000
    i32.const 2
    memory.copy
    
    ;; Load and swap
    i32.const 4300
    i32.load8_u
    
    i32.const 4301
    i32.load8_u
    
    call $swap
    
    i32.const 3004
    i32.store
    
    i32.const 3000
    i32.store)
  
  ;; Test: Bulk init from passive data segment (if supported)
  ;; Use memory.copy as alternative
  ;; Expected result at address[3000]: Pattern byte 0x01
  (func (;35;) (type 0)
    ;; Copy pattern
    i32.const 4400
    i32.const 2000
    i32.const 10
    memory.copy
    
    i32.const 3000
    i32.const 4400
    i32.load8_u
    i32.store)
  
  ;; Test: Table operations with multiple results
  ;; Expected result at address[3000]: table size, [3004]: 0 or 1 (is null check)
  (func (;36;) (type 0)
    table.size $functable
    
    i32.const 0
    table.get $functable
    ref.is_null
    
    i32.const 3004
    i32.store
    
    i32.const 3000
    i32.store)
  
  (memory (;0;) 10)
  (export "memory" (memory 0))
  (export "functable" (table $functable))
  (export "externrefs" (table $externrefs))
  
  (export "_start" (func 8))
  (export "_test_multiret_two" (func 8))
  (export "_test_multiret_three" (func 9))
  (export "_test_multiret_swap" (func 10))
  (export "_test_multiret_divmod" (func 11))
  (export "_test_multiret_minmax" (func 12))
  (export "_test_multiret_chain" (func 13))
  (export "_test_multiret_discard" (func 14))
  (export "_test_bulk_copy_verify_first" (func 15))
  (export "_test_bulk_copy_verify_third" (func 16))
  (export "_test_bulk_fill_verify" (func 17))
  (export "_test_bulk_fill_verify_middle" (func 18))
  (export "_test_bulk_fill_different" (func 19))
  (export "_test_bulk_copy_overlap" (func 20))
  (export "_test_bulk_copy_string" (func 21))
  (export "_test_bulk_fill_range" (func 22))
  (export "_test_bulk_copy_modify" (func 23))
  (export "_test_ref_null_func" (func 24))
  (export "_test_ref_null_extern" (func 25))
  (export "_test_ref_func_not_null" (func 26))
  (export "_test_ref_global_store" (func 27))
  (export "_test_ref_table_set_get" (func 28))
  (export "_test_ref_table_get_null" (func 29))
  (export "_test_ref_table_size" (func 30))
  (export "_test_ref_table_grow" (func 31))
  (export "_test_ref_table_size_after" (func 32))
  (export "_test_ref_table_fill" (func 33))
  (export "_test_ref_table_copy" (func 34))
  (export "_test_ref_externref_global" (func 35))
  (export "_test_ref_externref_store" (func 36))
  (export "_test_ref_externref_table_size" (func 37))
  (export "_test_combined_multiret_bulk" (func 38))
  (export "_test_combined_table_multiret" (func 39))
  (export "_test_combined_fill_copy" (func 40))
  (export "_test_combined_ref_sizes" (func 41))
  (export "_test_combined_swap_bulk" (func 42))
  (export "_test_combined_bulk_pattern" (func 43))
  (export "_test_combined_table_results" (func 44))
)
