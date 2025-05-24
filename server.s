%include "miaou.inc"

global _start

section .text
_start:
  ; init socket
  mov   rdi, qword [port] 
  call  socket_init
  cmp   rax, 0
  jl    .error

  mov   qword [fd], rax

  mov   rdi, rax
  call  accept_connection
  cmp   rax, 0
  jl    .error

.loop:
  jmp   .loop

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
msg   db "Hello, World!", NULL_CHAR
fd    dq 0
port  dq 6969

