(module
  (type (func (param i32 i32 i32 i32) (result i32)))
  (type (func (param i32 i32 i32 i32) (result i32))) ;; duplicate for clarity though same
  (import "wasi_snapshot_preview1" "fd_write" (func $fd_write (type 0)))
  (import "wasi_snapshot_preview1" "fd_read" (func $fd_read (type 0)))
  (import "wasi_snapshot_preview1" "fd_close" (func $fd_close (param i32) (result i32)))
  (import "wasi_snapshot_preview1" "path_open"
    (func $path_open (param i32 i32 i32 i32 i32 i64 i64 i32 i32) (result i32)))

  (memory 1)
  (export "memory" (memory 0))

  ;; Offsets:
  ;; 0x010 - storage for opened fd
  ;; 0x030 - iovec array for reads
  ;; 0x038 - nread
  ;; 0x040 - iovec array for writes
  ;; 0x050 - nwritten
  ;; 0x100 - path string
  ;; 0x200 - prefix string
  ;; 0x300 - read buffer

  (data (i32.const 0x100) "projects/sample_input.txt")
  (data (i32.const 0x200) "File contents:\0a")

  (func (export "_start")
    (local $fd i32)
    (local $nread i32)
    (local $result i32)

    ;; Open the file via path_open on preopened directory fd 3.
    (local.set $result
      (call $path_open
        (i32.const 3)                 ;; dirfd (preopened '.')
        (i32.const 0)                 ;; lookupflags
        (i32.const 0x100)             ;; path pointer
        (i32.const 25)                ;; path length
        (i32.const 0)                 ;; oflags
        (i64.const 1)                 ;; rights_base (fd_read)
        (i64.const 0)                 ;; rights_inheriting
        (i32.const 0)                 ;; fdflags
        (i32.const 0x010)             ;; result fd pointer
      )
    )

    ;; If open failed, bail out quietly by returning.
    (if (i32.ne (local.get $result) (i32.const 0))
      (then (return)))

    (local.set $fd
      (i32.load (i32.const 0x010)))

    ;; Prepare read iovec: buffer at 0x300 with length 256.
    (i32.store (i32.const 0x030) (i32.const 0x300))
    (i32.store (i32.const 0x034) (i32.const 256))

    ;; Zero nread
    (i32.store (i32.const 0x038) (i32.const 0))

    (local.set $result
      (call $fd_read
        (local.get $fd)
        (i32.const 0x030)
        (i32.const 1)
        (i32.const 0x038)
      )
    )

    ;; Close the file descriptor regardless of read result.
    (drop (call $fd_close (local.get $fd)))

    ;; If read failed, stop.
    (if (i32.ne (local.get $result) (i32.const 0))
      (then (return)))

    (local.set $nread
      (i32.load (i32.const 0x038)))

    ;; Build write iovecs: prefix + contents read.
    (i32.store (i32.const 0x040) (i32.const 0x200))
    (i32.store (i32.const 0x044) (i32.const 15)) ;; "File contents:\n"
    (i32.store (i32.const 0x048) (i32.const 0x300))
    (i32.store (i32.const 0x04C) (local.get $nread))

    ;; Clear nwritten
    (i32.store (i32.const 0x050) (i32.const 0))

    (drop
      (call $fd_write
        (i32.const 1)         ;; stdout
        (i32.const 0x040)     ;; iovec array
        (i32.const 2)         ;; iovec count
        (i32.const 0x050)     ;; nwritten ptr
      )
    )
  )
)
