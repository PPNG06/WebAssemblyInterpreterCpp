(module
  (type (func (param i32 i32) (result i32)))
  (import "env" "memory" (memory 1 1))
  (import "env" "table" (table 1 1 funcref))
  (import "env" "g_counter" (global (mut i32)))
  (import "env" "host_add" (func $host_add (type 0)))

  (func $bridge (type 0) (param i32 i32) (result i32)
    local.get 0
    local.get 1
    call $host_add)

  (func (export "_start")
    (local $loaded i32)
    (local $sum i32)
    (local.set $loaded (i32.load (i32.const 4)))
    (local.set $sum (global.get 0))
    (local.set $sum (i32.add (local.get $sum) (local.get $loaded)))
    (local.set $sum
      (call_indirect (type 0)
        (local.get $sum)
        (i32.const 3)
        (i32.const 0)))
    (i32.store (i32.const 0) (local.get $sum))
    (global.set 0 (local.get $sum)))

  (func (export "call_table_again")
    (local $result i32)
    (local.set $result
      (call_indirect (type 0)
        (i32.const 10)
        (i32.const 20)
        (i32.const 0)))
    (i32.store (i32.const 4) (local.get $result)))

  (func (export "store_global")
    (i32.store (i32.const 8) (global.get 0)))
)
