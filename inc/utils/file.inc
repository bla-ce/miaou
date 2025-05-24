section .data
  ; file constants
  O_RDONLY        equ 0
  O_WRONLY        equ 1
  O_RDWR          equ 2
  O_CREAT         equ 0x40
  O_APPEND        equ 0x400
  O_TMPFILE       equ 0x410000
  O_DIRECTORY     equ 65536
  FILE_PATH_LEN   equ 0x40
  FILE_MAX_LEN    equ 0x2000
  DIR_ENTRIES_LEN equ 0x800
  DT_DIR          equ 0x4
  DT_REG          equ 0x8
  
  ; file permissions
  S_IRWXU equ 0x1C0
  S_IRUSR equ 0x100
  S_IWUSR equ 0x080
  S_IXUSR equ 0x040
  S_IRWXG equ 0x038
  S_IRGRP equ 0x020
  S_IWGRP equ 0x010
  S_IXGRP equ 0x008
  S_IRWXO equ 0x007
  S_IROTH equ 0x004
  S_IWOTH equ 0x002
  S_IXOTH equ 0x001

  SEEK_SET equ 0x0
  SEEK_END equ 0x2

section .text
; writes to a file
; @param  rdi: file descriptor
; @param  rsi: pointer to the string
; @param  rdx: count
; @return rax: return value
write_file:
  cmp   rdi, 0
  jl    .error

  cmp   rsi, 0
  jle   .error

  mov   rax, SYS_WRITE
  ; rdi, rsi and rdx already populated
  syscall

  jmp   .return

.error:
  mov   rax, FAILURE_CODE

.return:
  ret

; opens a file and return the file descriptor
; @param  rdi: pointer to file_path
; @param  rsi: flags
; @param  rdx: mode
; @return rax: file descriptor or -1 if error
open_file:
  mov   rax, SYS_OPEN
  ; rdi, rsi and rdx already populated
  syscall

  ret

; copies data between infd to outfd
; @param  rdi: outfd
; @param  rsi: infd
; @param  rdx: count
; @return rax: number of bytes read
send_file:
  mov   r9, rdx
  xor   rdx, rdx
  ; rest of the registers are already populated
  syscall

  ret

; reads count bytes from file descriptor fd
; @param  rdi: fd
; @param  rsi: pointer to the buffer
; @param  rdx: count
; @return rax: bytes read
read_file:
  cmp   rdi, 0
  jl    .error

  cmp   rsi, 0
  jl    .error

  cmp   rdx, 0
  jl    .error

  mov   rax, SYS_READ
  syscall

  jmp   .return

.error:
  mov   rax, FAILURE_CODE

.return:
  ret

; returns the length of a file
; @param  rdi: file descriptor
; @return rax: length of the file
get_file_length:
  sub   rsp, 0x10

  cmp   rdi, 0
  jl    .error

  mov   [rsp], rdi

  mov   rax, SYS_LSEEK
  mov   rdi, [rsp]
  mov   rsi, 0
  mov   rdx, SEEK_SET
  syscall
  
  mov   rax, SYS_LSEEK
  mov   rdi, [rsp]
  mov   rsi, 0
  mov   rdx, SEEK_END
  syscall

  mov   [rsp+0x8], rax

  ; put cursor back to the beginning of the file
  mov   rax, SYS_LSEEK
  mov   rdi, [rsp]
  mov   rsi, 0
  mov   rdx, SEEK_SET
  syscall

  cmp   rax, 0
  jl    .error

  mov   rax, [rsp+0x8]
  jmp   .return

.error:
  mov   rax, FAILURE_CODE

.return:
  add   rsp, 0x10
  ret

; closes file description
; @param  rdi: fd
; @return rax: code
close_file:
  mov   rax, SYS_CLOSE
  syscall

  ret

; receives a message from a file descriptor
; @param  rdi: file descriptor
; @param  rsi: buffer
; @param  rdx: buffer max length
; @return rax: number of bytes received
receive_from_file:
  mov   rax, SYS_RECVFROM
  ; rdi, rsi and rdx already populated
  xor   r10, r10
  xor   r9, r9
  xor   r8, r8
  syscall
  
  ret

; connects the socket file descriptor to the address specified in rsi
; @param  rdi: file descriptor
; @param  rsi: pointer to sockaddr struct
; @return rax: return code
connect_fd:
  cmp   rdi, 0
  jl    .error

  cmp   rsi, 0
  jl    .error

  mov   rax, SYS_CONNECT
  ; rdi and rsi are already populated
  mov   rdx, SOCKADDR_STRUCT_LEN
  syscall

  jmp   .return

.error:
  mov   rax, FAILURE_CODE

.return:
  ret

