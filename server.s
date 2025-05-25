%include "miaou.inc"

global _start

section .text
_start:
  ; init socket
  mov   rdi, qword [port] 
  call  server_init
  cmp   rax, 0
  jl    .error

  mov   qword [fd], rax

  mov   rdi, log_starting_server
  call  println
  cmp   rax, 0
  jl    .error

  ; malloc set of file descriptors
  mov   rdi, FD_SET_STRUCT_LEN
  call  malloc
  cmp   rax, 0
  jl    .error

  mov   qword [main_fd], rax

  ; init fd set
  mov   rdi, qword [main_fd]
  call  FD_ZERO
  cmp   rax, 0
  jl    .error

  xor   r9, r9

.loop:
  ; set server file descriptor
  mov   rdi, r9
  mov   rsi, qword [main_fd]
  call  FD_SET
  cmp   rax, 0
  jl    .error

  inc   r9

  cmp   r9, 10
  jg    .loop_end

  jmp   .loop
.loop_end:

  mov   rdi, SUCCESS_CODE
  call  exit

.error:
  mov   rdi, FAILURE_CODE
  call  exit

; exits the program with code
; @param  rdi: exit code
exit:
  mov   rax, SYS_EXIT
  syscall

section .data
msg       db "Hello, Client!", NULL_CHAR
msg_len   equ $ - msg

fd        dq 0
port      dq 6969
main_fd   dq 0

backlog equ 10

BUFSIZ equ 1024
buf times BUFSIZ db 0

log_starting_server   db "[INFO] starting server...", NULL_CHAR
log_waiting           db "[INFO] waiting for connection...", NULL_CHAR
log_new_connection    db "[INFO] new connnection", NULL_CHAR
log_received_message  db "from client:", NULL_CHAR

