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

  ; receive connect message from the server to connect
  mov   rax, SYS_READ
  mov   rdi, qword [server_fd]
  mov   rsi, buf
  mov   rdx, BUFSIZ
  syscall
  cmp   rax, 0
  jl    .error

  mov   rdi, buf
  call  print
  cmp   rax, 0
  jl    .error

  ; malloc read set of file descriptors
  mov   rdi, FD_SET_STRUCT_LEN
  call  malloc
  cmp   rax, 0
  jl    .error

  mov   qword [read_fds], rax

.loop:
  ; init fd set
  mov   rdi, qword [read_fds]
  call  FD_ZERO
  cmp   rax, 0
  jl    .error

  ; set local input file descriptor
  mov   rdi, STDIN_FILENO
  mov   rsi, qword [read_fds]
  call  FD_SET
  cmp   rax, 0
  jl    .error

  ; set server file descriptor
  mov   rdi, qword [server_fd]
  mov   rsi, qword [read_fds]
  call  FD_SET
  cmp   rax, 0
  jl    .error

  mov   rax, SYS_SELECT 
  mov   rdi, qword [server_fd]
  inc   rdi
  mov   rsi, qword [read_fds]
  xor   rdx, rdx
  xor   r10, r10
  xor   r8, r8
  syscall
  cmp   rax, 0
  jl    .error

  mov   rdi, qword [server_fd]
  mov   rsi, qword [read_fds]
  call  FD_ISSET
  cmp   rax, 0
  jl    .error
  je    .check_input

  mov   rax, SYS_RECVFROM
  mov   rdi, qword [server_fd]
  mov   rsi, buf
  mov   rdx, BUFSIZ
  xor   r10, r10
  xor   r8, r8
  xor   r9, r9
  syscall
  cmp   rax, 0
  jl    .error

  mov   rdi, buf
  call  println
  cmp   rax, 0
  jl    .error

.check_input:
  mov   rdi, STDIN_FILENO
  mov   rsi, qword [read_fds]
  call  FD_ISSET
  cmp   rax, 0
  jl    .error
  je    .loop

  mov   rax, SYS_READ
  mov   rdi, STDIN_FILENO
  mov   rsi, buf
  mov   rdx, BUFSIZ
  syscall
  cmp   rax, 0
  jl    .error

  cmp   byte [buf], LINE_FEED
  je    .loop

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
max_connect dq 2

send_message db "message: ", NULL_CHAR

