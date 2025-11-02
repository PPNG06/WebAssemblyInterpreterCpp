(module
  (type (func (param i32 i32 i32 i32) (result i32)))
  (import "wasi_snapshot_preview1" "fd_write" (func $fd_write (type 0)))

  (memory 1)
  (export "memory" (memory 0))

  (data (i32.const 0x200) "Hello from projects!\0a")

  (func (export "_start")
    (local $iovs i32)
    (local $nwritten_ptr i32)
    (local.set $iovs (i32.const 0x100))
    (local.set $nwritten_ptr (i32.const 0x120))

    ;; iovec[0].buf = 0x200
    (i32.store (local.get $iovs) (i32.const 0x200))
    ;; iovec[0].len = 21 bytes
    (i32.store (i32.add (local.get $iovs) (i32.const 4)) (i32.const 21))

    (drop
      (call $fd_write
        (i32.const 1)              ;; fd = stdout
        (local.get $iovs)          ;; iovs pointer
        (i32.const 1)              ;; iovs_len
        (local.get $nwritten_ptr)  ;; nwritten pointer
      )
    )
  )
)
