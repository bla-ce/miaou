%include "miaou.inc"

global _start

section .text
_start:
  sub   rsp, 0x10

  ; *** STACK USAGE *** ;
  ; [rsp]       -> length of message received
  ; [rsp+0x8]   -> counter to iterate over active connections

  ; init socket
  mov   rdi, qword [port] 
  call  server_init
  cmp   rax, 0
  jl    .error

  mov   qword [server_fd], rax

  mov   rdi, log_starting_server
  call  println
  cmp   rax, 0
  jl    .error

  ; malloc main set of file descriptors
  mov   rdi, FD_SET_STRUCT_LEN
  call  malloc
  cmp   rax, 0
  jl    .error

  mov   qword [main_fds], rax

  ; malloc read set of file descriptors
  mov   rdi, FD_SET_STRUCT_LEN
  call  malloc
  cmp   rax, 0
  jl    .error

  mov   qword [read_fds], rax

  ; init fd set
  mov   rdi, qword [main_fds]
  call  FD_ZERO
  cmp   rax, 0
  jl    .error

  ; set server file descriptor
  mov   rdi, qword [server_fd]
  mov   rsi, qword [main_fds]
  call  FD_SET
  cmp   rax, 0
  jl    .error

.loop:
  ; back up main fd set
  mov   rdi, qword [read_fds]
  mov   rsi, qword [main_fds]
  mov   rcx, FD_SET_STRUCT_LEN
  rep   movsb

  mov   rax, SYS_SELECT 
  mov   rdi, qword [max_connect]
  inc   rdi
  mov   rsi, qword [read_fds]
  xor   rdx, rdx
  xor   r10, r10
  xor   r8, r8
  syscall
  cmp   rax, 0
  jl    .error

  mov   qword [curr_fd], 0

.inner_loop:
  inc   qword [curr_fd]

  mov   rax, qword [max_connect]
  cmp   qword [curr_fd], rax
  jg    .inner_loop_end

  ; filter only active or new clients
  mov   rdi, qword [curr_fd]
  mov   rsi, qword [read_fds]
  call  FD_ISSET
  cmp   rax, 0
  jl    .error
  je   .inner_loop

  ; filter out the server which indicates a new connection
  mov   rax, qword [server_fd] 
  cmp   qword [curr_fd], rax
  jne   .read_msg

  ; add new client 
  mov   rdi, qword [server_fd]
  call  accept_connection
  cmp   rax, 0
  jl    .error

  mov   qword [client_fd], rax

  ; log new connection
  mov   rdi, log_new_connection
  call  println
  cmp   rax, 0
  jl    .error

  mov   rdi, qword [client_fd]
  mov   rsi, qword [main_fds]
  call  FD_SET
  cmp   rax, 0
  jl    .error

  mov   rax, SYS_WRITE
  mov   rdi, qword [client_fd]
  mov   rsi, msg
  mov   rdx, msg_len
  syscall
  cmp   rax, 0
  jl    .error

  jmp   .inner_loop

.read_msg:
  mov   rax, SYS_RECVFROM
  mov   rdi, qword [curr_fd]
  mov   rsi, buf
  mov   rdx, BUFSIZ
  xor   r10, r10
  xor   r8, r8
  xor   r9, r9
  syscall
  cmp   rax, 1
  jl    .clear_fd

  mov   qword [rsp], rax

  mov   rdi, buf
  mov   rsi, rax
  call  nprint
  cmp   rax, 0
  jl    .error

  ; send message to each connection
  mov   rax, qword [server_fd]
  mov   qword [rsp+0x8], rax

.send_loop:
  inc   qword [rsp+0x8]
  mov   rax, qword [max_connect]
  cmp   qword [rsp+0x8], rax
  jge   .send_loop_end

  mov   rax, qword [curr_fd]
  cmp   rax, qword [rsp+0x8]
  je    .send_loop

  mov   rdi, qword [rsp+0x8]
  mov   rsi, qword [main_fds]
  call  FD_ISSET
  cmp   rax, 0
  jl    .error
  je    .send_loop

  mov   rax, SYS_WRITE
  mov   rdi, qword [rsp+0x8]
  mov   rsi, buf
  mov   rdx, qword [rsp]
  syscall
  cmp   rax, 0
  jl    .error

  jmp   .send_loop

.send_loop_end:

  jmp   .inner_loop
  
.clear_fd:
  jmp   .inner_loop

.inner_loop_end:
  jmp   .loop

.end_loop:

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

port      dq 4747
read_fds  dq 0
main_fds  dq 0
server_fd dq 0
client_fd dq 0

curr_fd     dq 0
max_connect dq 10

BUFSIZ equ 1024
buf times BUFSIZ db 0

log_starting_server   db "[INFO] starting server...", NULL_CHAR
log_waiting           db "[INFO] waiting for connection...", NULL_CHAR
log_new_connection    db "[INFO] new connnection", NULL_CHAR
log_received_message  db "from client:", NULL_CHAR

