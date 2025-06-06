%include "miaou.inc"

global _start

section .text
_start:
  sub   rsp, 0x28

  ; *** STACK USAGE *** ;
  ; [rsp]       -> length of message received
  ; [rsp+0x8]   -> counter to iterate over active connections
  ; [rsp+0x10]  -> pointer to the client struct sending the message
  ; [rsp+0x18]  -> pointer to the client username
  ; [rsp+0x20]  -> now timestamp

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
  mov   rdi, MAX_CONNECT
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

  mov   rax, MAX_CONNECT
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

  mov   rbx, qword [active_connections]
  inc   rbx
  cmp   rbx, MAX_CONNECT
  jg    .inner_loop

  inc   qword [active_connections]

  ; add new client 
  mov   rdi, qword [server_fd]
  call  accept_connection
  cmp   rax, 0
  jl    .error

  mov   qword [client_fd], rax

  ; set the file descriptor
  mov   rdi, qword [client_fd]
  mov   rsi, qword [main_fds]
  call  FD_SET
  cmp   rax, 0
  jl    .error

  ; malloc new client
  mov   rdi, CLIENT_STRUCT_LEN
  call  malloc
  cmp   rax, 0
  jl    .error

  mov   [client_struct], rax

  call  now

  mov   rsi, [client_struct] 

  mov   rdi, qword [client_fd]
  mov   qword [rsi+CLIENT_STRUCT_OFFSET_FD], rdi
  mov   qword [rsi+CLIENT_STRUCT_OFFSET_ACTIVE], 1
  mov   qword [rsi+CLIENT_STRUCT_OFFSET_LAST_MESSAGE], rax
  mov   qword [rsi+CLIENT_STRUCT_OFFSET_STRIKES], 0

  ; username will be assigned later
  mov   qword [rsi+CLIENT_STRUCT_OFFSET_USERNAME], 0

  ; get color
  xor   rdx, rdx
  mov   rax, qword [active_connections]
  dec   rax
  mov   rbx, 8
  mul   rbx

  mov   rdi, colors
  add   rdi, rax

  mov   rsi, [rdi]
  mov   rdi, [client_struct]
  mov   qword [rdi+CLIENT_STRUCT_OFFSET_COLOR], rsi

  ; add client to the array
  mov   rdi, clients
  add   rdi, rax
  mov   rsi, [client_struct]
  mov   [rdi], rsi
  
  ; send the welcome message
  mov   rax, SYS_WRITE
  mov   rdi, qword [client_fd]
  mov   rsi, msg
  mov   rdx, msg_len
  syscall
  cmp   rax, 0
  jl    .error

  jmp   .inner_loop

.read_msg:
  mov   rdi, qword [curr_fd]
  call  get_client_by_fd
  cmp   rax, 0
  jle   .error  ; TODO: probably doing something different if 0
  
  mov   [rsp+0x10], rax

  ; get message from the client
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

  ; check if client's username has been set
  mov   rax, [rsp+0x10]

  cmp   qword [rax+CLIENT_STRUCT_OFFSET_USERNAME], 0
  jg    .read_and_send_message

  ; malloc string for username
  mov   rdi, qword [rsp]
  call  malloc
  cmp   rax, 0
  jl    .error

  ; set username
  mov   rdi, [rsp+0x10]
  mov   [rdi+CLIENT_STRUCT_OFFSET_USERNAME], rax

  mov   [rsp+0x18], rax

  ; copy username
  mov   rdi, rax
  mov   rsi, buf
  mov   rcx, qword [rsp]
  rep   movsb

  ; add null char
  mov   rax, NULL_CHAR
  stosb

  mov   rdi, [rsp+0x18]
  call  strlen
  cmp   rax, 0
  jl    .error

  mov   rdi, [rsp+0x18]
  mov   rsi, rax
  dec   rsi
  call  boeuf_ncreate
  cmp   rax, 0
  jl    .error

  mov   [rsp+0x18], rax

  mov   rdi, rax
  mov   rsi, log_new_connection
  call  boeuf_append
  cmp   rax, 0
  jl    .error

  mov   [rsp+0x18], rax

  mov   rdi, rax
  call  println
  cmp   rax, 0
  jl    .error

  ; send message to each connection
  mov   rdi, [rsp+0x18]
  mov   rsi, qword [server_fd]
  mov   rdx, qword [curr_fd]
  mov   rcx, [main_fds]
  call  send_message_to_all_fds
  cmp   rax, 0
  jl    .error

  mov   rdi, [rsp+0x18]
  call  boeuf_free
  cmp   rax, 0
  jl    .error

  jmp   .inner_loop

.read_and_send_message:
  ; check if client is closing the connection
  mov   rdi, buf
  mov   rsi, close_msg
  mov   rdx, close_msg_len
  call  strncmp
  je    .clear_fd

  ; check rate limiter
  call  now 

  mov   rdi, [rsp+0x10]
  mov   rbx, qword [rdi+CLIENT_STRUCT_OFFSET_LAST_MESSAGE]

  mov   qword [rdi+CLIENT_STRUCT_OFFSET_LAST_MESSAGE], rax

  sub   rax, rbx
  
  cmp   rax, RATE_LIMIT
  jge   .message_allowed

  ; add one strike
  inc   qword [rdi+CLIENT_STRUCT_OFFSET_STRIKES]

  ; TODO: if strike is greater than 10, exclude user from the chat
  cmp   qword [rdi+CLIENT_STRUCT_OFFSET_STRIKES], STRIKES_LIMIT
  jl    .no_ban   

  jmp   .clear_fd

.no_ban:
  jmp   .inner_loop

.message_allowed:
  ; reset strikes count
  mov   rdi, [rsp+0x10]
  mov   qword [rdi+CLIENT_STRUCT_OFFSET_STRIKES], 0 

  ; create boeuf buffer to display message
  mov   rsi, [rdi+CLIENT_STRUCT_OFFSET_COLOR]
  mov   rdi, rsi
  call  boeuf_create
  cmp   rax, 0
  jl    .error

  mov   [rsp+0x18], rax

  ; get username
  mov   rdi, [rsp+0x10]
  mov   rsi, [rdi+CLIENT_STRUCT_OFFSET_USERNAME]
  mov   rdi, [rsp+0x18]
  call  boeuf_append
  cmp   rax, 0
  jl    .error

  mov   rdi, rax
  mov   rsi, DEFAULT_FG
  call  boeuf_append
  cmp   rax, 0
  jl    .error

  mov   rdi, rax
  mov   rsi, buf
  mov   rdx, qword [rsp]
  call  boeuf_nappend
  cmp   rax, 0
  jl    .error
  
  mov   [rsp+0x18], rax

  mov   rdi, rax
  call  println
  cmp   rax, 0
  jl    .error

  ; send message to each connection
  mov   rdi, [rsp+0x18]
  mov   rsi, qword [server_fd]
  mov   rdx, qword [curr_fd]
  mov   rcx, [main_fds]
  call  send_message_to_all_fds
  cmp   rax, 0
  jl    .error

  ; free boeuf buffer
  mov   rdi, [rsp+0x18]
  call  boeuf_free
  cmp   rax, 0
  jl    .error

  jmp   .inner_loop
  
.clear_fd:
  ; close connection
  mov   rax, SYS_CLOSE
  mov   rdi, qword [curr_fd]
  syscall

  mov   rsi, [rsp+0x10]
  mov   rdi, qword [rsi+CLIENT_STRUCT_OFFSET_USERNAME]
  call  strlen
  cmp   rax, 0
  jl    .error

  mov   rsi, [rsp+0x10]
  mov   rdi, qword [rsi+CLIENT_STRUCT_OFFSET_USERNAME]
  mov   rsi, rax
  dec   rsi
  call  boeuf_ncreate
  cmp   rax, 0
  jl    .error

  mov   rdi, rax
  mov   rsi, log_close
  call  boeuf_append
  cmp   rax, 0
  jl    .error

  mov   [rsp+0x18], rax

  mov   rdi, [rsp+0x18]
  call  println
  cmp   rax, 0
  jl    .error

  ; send message to each connection
  mov   rdi, [rsp+0x18]
  mov   rsi, qword [server_fd]
  mov   rdx, qword [curr_fd]
  mov   rcx, [main_fds]
  call  send_message_to_all_fds
  cmp   rax, 0
  jl    .error

  ; clear fd
  mov   rdi, qword [curr_fd]
  mov   rsi, qword [main_fds]
  call  FD_CLR
  cmp   rax, 0
  jl    .error

  ; free boeuf buffer
  mov   rdi, [rsp+0x18]
  call  boeuf_free
  cmp   rax, 0
  jl    .error

  ; free username
  mov   rsi, [rsp+0x10]
  mov   rdi, [rsi+CLIENT_STRUCT_OFFSET_USERNAME]
  call  free
  cmp   rax, 0
  jl    .error

  ; remove client from array
  mov   rdi, qword [curr_fd]
  call  remove_client_from_array
  cmp   rax, 0
  jl    .error

  ; free client struct
  mov   rdi, qword [rsp+0x10]
  call  free
  cmp   rax, 0
  jl    .error

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

