;;
;; WebAssembly Bulk Memory Operations Tests (0xFC prefix 0x08-0x0B)
;;
;; This file tests bulk memory operations from the bulk-memory proposal
;;
;; Coverage: memory.copy, memory.fill, memory.init, data.drop
;;

(module
  (type (;0;) (func))
  
  ;; Data segment for memory.init tests
  (data (;0;) (i32.const 100) "Hello, World!")
  (data (;1;) (i32.const 200) "\00\01\02\03\04\05\06\07\08\09")
  
  ;; === memory.fill TESTS ===
  
  ;; Test: memory.fill - Fill 10 bytes starting at address 0 with value 42
  ;; Expected result at address[0]: 42
  (func (;0;) (type 0)
    ;; memory.fill(dest=0, value=42, size=10)
    i32.const 0     ;; destination
    i32.const 42    ;; value to fill
    i32.const 10    ;; size
    memory.fill
    
    ;; Load and store first byte for validation
    i32.const 0
    i32.const 0
    i32.load8_u
    i32.store)
  
  ;; Test: memory.fill - Verify fill worked across range
  ;; Expected result at address[0]: 42 (byte at offset 5)
  (func (;1;) (type 0)
    ;; Fill 20 bytes at address 10 with value 99
    i32.const 10
    i32.const 99
    i32.const 20
    memory.fill
    
    ;; Load byte from middle of filled region (address 15)
    i32.const 0
    i32.const 15
    i32.load8_u
    i32.store)
  
  ;; Test: memory.fill - Fill single byte
  ;; Expected result at address[0]: 77
  (func (;2;) (type 0)
    i32.const 50
    i32.const 77
    i32.const 1
    memory.fill
    
    i32.const 0
    i32.const 50
    i32.load8_u
    i32.store)
  
  ;; Test: memory.fill - Fill with zero
  ;; Expected result at address[0]: 0
  (func (;3;) (type 0)
    ;; First set some non-zero values
    i32.const 60
    i32.const 88
    i32.store
    
    ;; Now fill with zeros
    i32.const 60
    i32.const 0
    i32.const 4
    memory.fill
    
    ;; Verify it's zero
    i32.const 0
    i32.const 60
    i32.load
    i32.store)
  
  ;; === memory.copy TESTS ===
  
  ;; Test: memory.copy - Copy 4 bytes from address 100 to address 0
  ;; Expected result at address[0]: 1819043144 (0x6C6C6548 = "lleH" in little-endian)
  (func (;4;) (type 0)
    ;; First ensure source has data: "Hell"
    i32.const 100
    i32.const 0x6C6C6548  ;; "lleH" in little-endian (will be "Hell" when read as string)
    i32.store
    
    ;; memory.copy(dest=0, src=100, size=4)
    i32.const 0     ;; destination
    i32.const 100   ;; source
    i32.const 4     ;; size
    memory.copy
    
    ;; Load copied value
    i32.const 0
    i32.const 0
    i32.load
    i32.store)
  
  ;; Test: memory.copy - Copy single byte
  ;; Expected result at address[0]: 65 (ASCII 'A')
  (func (;5;) (type 0)
    ;; Set source
    i32.const 110
    i32.const 65  ;; 'A'
    i32.store8
    
    ;; Copy
    i32.const 0
    i32.const 110
    i32.const 1
    memory.copy
    
    ;; Verify
    i32.const 0
    i32.const 0
    i32.load8_u
    i32.store)
  
  ;; Test: memory.copy - Copy larger block
  ;; Expected result at address[0]: 170 (0xAA)
  (func (;6;) (type 0)
    ;; Fill source with pattern
    i32.const 120
    i32.const 0xAA
    i32.const 16
    memory.fill
    
    ;; Copy to destination
    i32.const 0
    i32.const 120
    i32.const 16
    memory.copy
    
    ;; Verify first byte
    i32.const 0
    i32.const 0
    i32.load8_u
    i32.store)
  
  ;; Test: memory.copy - Overlapping copy (forward)
  ;; Expected result at address[0]: 1 (first byte of pattern)
  (func (;7;) (type 0)
    ;; Set up pattern at 150
    i32.const 150
    i32.const 1
    i32.store8
    i32.const 151
    i32.const 2
    i32.store8
    i32.const 152
    i32.const 3
    i32.store8
    i32.const 153
    i32.const 4
    i32.store8
    
    ;; Copy overlapping forward (150->152, should behave like memmove)
    i32.const 152
    i32.const 150
    i32.const 3
    memory.copy
    
    ;; Result at 152 should be 1 (copied from 150)
    i32.const 0
    i32.const 152
    i32.load8_u
    i32.store)
  
  ;; === memory.init TESTS ===
  
  ;; Test: memory.init - Copy from data segment 0 to memory
  ;; Expected result at address[0]: 72 (ASCII 'H' from "Hello")
  (func (;8;) (type 0)
    ;; memory.init(segment=0, dest=0, offset=0, size=5)
    i32.const 0     ;; destination in memory
    i32.const 0     ;; offset in data segment
    i32.const 5     ;; size to copy
    memory.init 0   ;; data segment index
    
    ;; Load first byte (should be 'H' = 72)
    i32.const 0
    i32.const 0
    i32.load8_u
    i32.store)
  
  ;; Test: memory.init - Copy partial data from segment
  ;; Expected result at address[0]: 87 (ASCII 'W' from "World")
  (func (;9;) (type 0)
    ;; Copy "World" part from "Hello, World!" (offset 7, size 5)
    i32.const 0
    i32.const 7     ;; offset in data segment (skip "Hello, ")
    i32.const 5     ;; size
    memory.init 0
    
    ;; Load first byte (should be 'W' = 87)
    i32.const 0
    i32.const 0
    i32.load8_u
    i32.store)
  
  ;; Test: memory.init - Copy from second data segment
  ;; Expected result at address[0]: 3 (byte from segment 1)
  (func (;10;) (type 0)
    ;; Copy from data segment 1
    i32.const 0
    i32.const 3     ;; offset in segment
    i32.const 1     ;; size
    memory.init 1
    
    i32.const 0
    i32.const 0
    i32.load8_u
    i32.store)
  
  ;; === data.drop TESTS ===
  
  ;; Test: data.drop - Drop segment after use
  ;; Expected result at address[0]: 72 (copied before drop)
  (func (;11;) (type 0)
    ;; Copy data first
    i32.const 0
    i32.const 0
    i32.const 5
    memory.init 0
    
    ;; Drop the segment (frees memory)
    data.drop 0
    
    ;; Verify the data is still in memory (drop only affects segment, not copied data)
    i32.const 0
    i32.const 0
    i32.load8_u
    i32.store)
  
  ;; === COMBINED TESTS ===
  
  ;; Test: Combined - Fill, then copy
  ;; Expected result at address[0]: 55
  (func (;12;) (type 0)
    ;; Fill area with 55
    i32.const 300
    i32.const 55
    i32.const 10
    memory.fill
    
    ;; Copy to result location
    i32.const 0
    i32.const 300
    i32.const 1
    memory.copy
    
    i32.const 0
    i32.const 0
    i32.load8_u
    i32.store)
  
  ;; Test: Combined - Init, then copy
  ;; Expected result at address[0]: 72 ('H')
  (func (;13;) (type 0)
    ;; Init from data segment to temp location
    i32.const 400
    i32.const 0
    i32.const 5
    memory.init 0
    
    ;; Copy to final location
    i32.const 0
    i32.const 400
    i32.const 1
    memory.copy
    
    i32.const 0
    i32.const 0
    i32.load8_u
    i32.store)
  
  ;; Test: Zero-length operations
  ;; Expected result at address[0]: 123 (unchanged)
  (func (;14;) (type 0)
    ;; Set initial value
    i32.const 0
    i32.const 123
    i32.store
    
    ;; Zero-length fill (should be no-op)
    i32.const 0
    i32.const 99
    i32.const 0
    memory.fill
    
    ;; Zero-length copy (should be no-op)
    i32.const 0
    i32.const 100
    i32.const 0
    memory.copy
    
    ;; Verify unchanged
    i32.const 0
    i32.const 0
    i32.load
    i32.store)
  
  (memory (;0;) 1)
  (export "memory" (memory 0))
  
  (export "_start" (func 0))
  (export "_test_fill_basic" (func 0))
  (export "_test_fill_range" (func 1))
  (export "_test_fill_single" (func 2))
  (export "_test_fill_zero" (func 3))
  (export "_test_copy_basic" (func 4))
  (export "_test_copy_single" (func 5))
  (export "_test_copy_block" (func 6))
  (export "_test_copy_overlapping" (func 7))
  (export "_test_init_basic" (func 8))
  (export "_test_init_partial" (func 9))
  (export "_test_init_segment1" (func 10))
  (export "_test_drop_after_use" (func 11))
  (export "_test_combined_fill_copy" (func 12))
  (export "_test_combined_init_copy" (func 13))
  (export "_test_zero_length" (func 14))
)


