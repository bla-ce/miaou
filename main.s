%include "miaou.inc"

global _start

section .text
_start:
  mov   rdi, msg
  call  println
  cmp   rax, 0
  jl    .error

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
  
msg db "Hello, World!", NULL_CHAR

