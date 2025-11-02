(module
    ;; wasi_snapshot_preview1!fd_write(file_descriptor, *iovs, iovs_len, nwritten) -> status_code
    (import "wasi_snapshot_preview1" "fd_write" (func $fd_write (param i32 i32 i32 i32) (result i32)))

    (memory 1)
    (export "memory" (memory 0))

    ;; Put a message to linear memory at offset 32
    (data (i32.const 32) "Hello, world!\n")

    (func $main (export "_start")
        ;; Create a new io vector at address 0x4
        (i32.store (i32.const 4) (i32.const 32))  ;; iov.iov_base - pointer to the start of the message
        (i32.store (i32.const 8) (i32.const 14))  ;; iov.iov_len  - length of the message

        (call $fd_write
            (i32.const 1)  ;; file_descriptor - 1 for stdout
            (i32.const 4)  ;; *iovs           - pointer to the io vector
            (i32.const 1)  ;; iovs_len        - count of items in io vector
            (i32.const 20) ;; nwritten        - where to store the number of bytes written
        )
        drop ;; discard the WASI status code
    )
)
