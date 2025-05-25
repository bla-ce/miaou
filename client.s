%include "miaou.inc"

global _start

section .text
_start:
  ; init client socket
  call  client_init
  cmp   rax, 0
  jl    .error

  mov   qword [server_fd], rax

  ; connect to the server
  mov   rdi, rax
  mov   rsi, qword [port]
  mov   rdx, AF_INET
  call  connect_to_socket
  cmp   rax, 0
  jl    .error

  ; receive hello message from the server
  mov   rax, SYS_READ
  mov   rdi, qword [server_fd]
  mov   rsi, buf
  mov   rdx, BUFSIZ
  syscall
  cmp   rax, 0
  jl    .error

  mov   rdi, buf
  call  println
  cmp   rax, 0
  jl    .error

.loop:
  mov   rax, SYS_READ
  mov   rdi, STDIN_FILENO
  mov   rsi, buf
  mov   rdx, BUFSIZ
  syscall
  cmp   rax, 0
  jl    .error

  mov   rdx, rax
  mov   rax, SYS_WRITE
  mov   rdi, qword [server_fd]
  mov   rsi, buf
  syscall
  cmp   rax, 0
  jl    .error
  
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
msg       db "Hello, World!", NULL_CHAR
server_fd dq 0
port      dq 6969

BUFSIZ equ 1024
buf times BUFSIZ db 0
