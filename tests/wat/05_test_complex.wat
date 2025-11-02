;; Complex nesting and control flow tests for MicroWASM
;; Tests deeply nested blocks, function calls within blocks, and label stack management

(module
  (memory (export "memory") 1)
  
  ;; Test 1: Deeply nested blocks with branches (expect: 42)
  (func $nested_blocks (export "nested_blocks") (result i32)
    (local $result i32)
    (block (result i32)  ;; block 0
      (block (result i32)  ;; block 1
        (block (result i32)  ;; block 2
          (block (result i32)  ;; block 3
            (block (result i32)  ;; block 4
              (block (result i32)  ;; block 5
                i32.const 42
                br 5  ;; branch to block 0 with value 42
              )
              unreachable
            )
            unreachable
          )
          unreachable
        )
        unreachable
      )
      unreachable
    )
    local.tee $result  ;; save to local and keep on stack
    ;; Store in memory
    i32.const 0  ;; address
    local.get $result  ;; value
    i32.store
    ;; Return the value (still on stack from local.tee)
  )

  ;; Test 2: Blocks with different result types (expect: 50)
  (func $block_results (export "block_results") (result i32)
    (local $result i32)
    (block (result i32)
      (block (result i32)
        i32.const 10
        i32.const 20
        br 0  ;; leaves 20 on stack, pops 10
      )
      i32.const 30
      i32.add  ;; 20 + 30 = 50
    )
    local.tee $result  ;; save to local and keep on stack
    ;; Store in memory
    i32.const 0  ;; address
    local.get $result  ;; value
    i32.store
    ;; Return the value (still on stack from local.tee)
  )

  ;; Test 3: Conditional branches in nested blocks (param 0 -> 100, param 1 -> 200, else -> 300)
  (func $conditional_nested (param i32) (result i32)
    (local $result i32)
    (block (result i32)  ;; block 0
      (block (result i32)  ;; block 1
        (block (result i32)  ;; block 2
          local.get 0
          i32.const 0
          i32.eq
          (if (result i32)
            (then
              i32.const 100
              br 2  ;; branch to block 0 with 100
            )
            (else
              local.get 0
              i32.const 1
              i32.eq
              (if (result i32)
                (then
                  i32.const 200
                  br 2  ;; branch to block 1 with 200
                )
                (else
                  i32.const 300  ;; default case
                )
              )
            )
          )
        )
        ;; unreachable - all paths branch out
      )
      ;; unreachable - all paths branch out
    )
    local.tee $result  ;; save to local and keep on stack
    ;; Store in memory
    i32.const 0  ;; address
    local.get $result  ;; value
    i32.store
    ;; Return the value (still on stack from local.tee)
  )
  
  ;; Wrapper functions for parameterized tests
  (func $conditional_nested_0 (export "conditional_nested_0") (result i32)
    i32.const 0
    call $conditional_nested
  )
  
  (func $conditional_nested_1 (export "conditional_nested_1") (result i32)
    i32.const 1
    call $conditional_nested
  )
  
  (func $conditional_nested_2 (export "conditional_nested_2") (result i32)
    i32.const 2
    call $conditional_nested
  )

  ;; Test 4: Function call within nested blocks
  (func $helper (param i32) (result i32)
    local.get 0
    i32.const 2
    i32.mul
  )

  (func $call_in_block (export "call_in_block") (result i32)
    (local $result i32)
    (block (result i32)
      (block (result i32)
        i32.const 21
        call $helper  ;; returns 42
        br 1  ;; branch to outer block with 42
      )
      unreachable
    )
    local.tee $result  ;; save to local and keep on stack
    ;; Store in memory
    i32.const 0  ;; address
    local.get $result  ;; value
    i32.store
    ;; Return the value (still on stack from local.tee)
  )

  ;; Test 5: Loop with nested blocks (expect: 5)
  (func $loop_with_blocks (export "loop_with_blocks") (result i32)
    (local $counter i32)
    (block $exit
      (loop $continue
        local.get $counter
        i32.const 1
        i32.add
        local.set $counter
        
        local.get $counter
        i32.const 5
        i32.ge_s
        br_if $exit  ;; exit if counter >= 5
        
        br $continue  ;; continue loop
      )
    )
    local.get $counter
    local.tee 0  ;; save to local and keep on stack
    ;; Store in memory
    i32.const 0  ;; address
    local.get 0  ;; value
    i32.store
    ;; Return the value (still on stack from local.tee)
  )

  ;; Test 6: Multiple function calls with label preservation
  (func $func_a (result i32)
    (block (result i32)
      i32.const 10
      call $func_b
      i32.add
    )
  )

  (func $func_b (result i32)
    (block (result i32)
      (block (result i32)
        i32.const 5
        br 1
      )
      unreachable
    )
  )

  (func $multi_call (export "multi_call") (result i32)
    (local $result i32)
    (block (result i32)
      call $func_a  ;; returns 15 (10 + 5)
      call $func_a  ;; returns 15
      i32.add  ;; 30
    )
    local.tee $result  ;; save to local and keep on stack
    ;; Store in memory
    i32.const 0  ;; address
    local.get $result  ;; value
    i32.store
    ;; Return the value (still on stack from local.tee)
  )

  ;; Test 7: br_table with nested blocks (param 0->400, 1->300, 2->200, else->100)
  (func $br_table_nested (param i32) (result i32)
    (local $result i32)
    (block $b0  ;; block 0 (default)
      (block $b1  ;; block 1
        (block $b2  ;; block 2
          (block $b3  ;; block 3
            local.get 0
            br_table $b3 $b2 $b1 $b0  ;; index 0->b3, 1->b2, 2->b1, else->b0
          )
          ;; br_table case 0: exits to here
          i32.const 400
          local.set $result
          br $b0
        )
        ;; br_table case 1: exits to here
        i32.const 300
        local.set $result
        br $b0
      )
      ;; br_table case 2: exits to here
      i32.const 200
      local.set $result
      br $b0
    )
    ;; br_table default/exit: get result or default
    local.get $result
    i32.eqz
    (if
      (then
        i32.const 100
        local.set $result
      )
    )
    local.get $result
    local.tee $result  ;; save to local and keep on stack
    ;; Store in memory
    i32.const 0  ;; address
    local.get $result  ;; value
    i32.store
    ;; Return the value (still on stack from local.tee)
  )
  
  ;; Wrapper functions for br_table_nested
  (func $br_table_nested_0 (export "br_table_nested_0") (result i32)
    i32.const 0
    call $br_table_nested
  )
  
  (func $br_table_nested_1 (export "br_table_nested_1") (result i32)
    i32.const 1
    call $br_table_nested
  )
  
  (func $br_table_nested_2 (export "br_table_nested_2") (result i32)
    i32.const 2
    call $br_table_nested
  )
  
  (func $br_table_nested_3 (export "br_table_nested_3") (result i32)
    i32.const 3
    call $br_table_nested
  )

  ;; Test 8: Complex control flow with value stack manipulation (expect: 25)
  (func $complex_stack (export "complex_stack") (result i32)
    (local $result i32)
    (local $temp i32)
    i32.const 1
    local.set $temp
    
    i32.const 2
    i32.const 3
    i32.const 4
    i32.add  ;; 3 + 4 = 7
    i32.const 5
    i32.add  ;; 7 + 5 = 12
    i32.mul  ;; 2 * 12 = 24
    local.get $temp
    i32.add  ;; 1 + 24 = 25
    
    local.tee $result  ;; save to local and keep on stack
    ;; Store in memory
    i32.const 0  ;; address
    local.get $result  ;; value
    i32.store
    ;; Return the value (still on stack from local.tee)
  )

  ;; Test 9: Recursive calls with blocks (factorial - param 5 -> 120)
  (func $recursive (param i32) (result i32)
    (local $result i32)
    (block (result i32)
      local.get 0
      i32.const 0
      i32.le_s
      (if (result i32)
        (then i32.const 1)
        (else
          local.get 0
          local.get 0
          i32.const 1
          i32.sub
          call $recursive
          i32.mul
        )
      )
    )
    local.tee $result  ;; save to local and keep on stack
    ;; Store in memory
    i32.const 0  ;; address
    local.get $result  ;; value
    i32.store
    ;; Return the value (still on stack from local.tee)
  )
  
  ;; Wrapper function for recursive factorial(5)
  (func $recursive_5 (export "recursive_5") (result i32)
    i32.const 5
    call $recursive
  )

  ;; Test 10: Empty blocks and void blocks (expect: 42)
  (func $empty_blocks (export "empty_blocks") (result i32)
    (local $result i32)
    (block)  ;; void block
    (block)  ;; another void block
    (block (result i32)
      (block)  ;; void block inside
      i32.const 42
    )
    local.tee $result  ;; save to local and keep on stack
    ;; Store in memory
    i32.const 0  ;; address
    local.get $result  ;; value
    i32.store
    ;; Return the value (still on stack from local.tee)
  )
  
  ;; Test 11: Loop with nested blocks and label cleanup (tests the loop label bug)
  ;; This test ensures labels are properly cleaned up when branching back to loop
  (func $loop_label_cleanup (export "loop_label_cleanup") (result i32)
    (local $counter i32)
    (local $sum i32)
    (block $exit
      (loop $continue
        ;; Create some nested blocks inside the loop
        (block $inner1
          (block $inner2
            (block $inner3
              ;; Increment counter
              local.get $counter
              i32.const 1
              i32.add
              local.set $counter
              
              ;; Add counter to sum
              local.get $sum
              local.get $counter
              i32.add
              local.set $sum
              
              ;; Check if we should exit
              local.get $counter
              i32.const 10
              i32.ge_s
              br_if $exit  ;; Exit if counter >= 10
              
              ;; Continue loop - this will clean up inner1/2/3 labels!
              br $continue
            )
          )
        )
        ;; Should never reach here after first iteration
        unreachable
      )
    )
    ;; Sum of 1+2+3+4+5+6+7+8+9+10 = 55
    local.get $sum
    local.tee 0
    i32.const 0
    local.get 0
    i32.store
  )
  
  ;; Wrapper function
  (func $loop_label_cleanup_test (export "loop_label_cleanup_test") (result i32)
    call $loop_label_cleanup
  )
)

