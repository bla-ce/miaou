%include "miaou.inc"

global _start

section .text
_start:
  sub   rsp, 0x28

  ; *** STACK USAGE *** ;
  ; [rsp]       -> length of message received
  ; [rsp+0x8]   -> counter to iterate over active connections
  ; [rsp+0x10]  -> pointer to the user struct sending the message
  ; [rsp+0x18]  -> pointer to the user username
  ; [rsp+0x20]  -> now timestamp

  ; init socket
  mov   rdi, qword [port] 
  call  server_init
  cmp   rax, 0
  jl    .error

  mov   qword [server_fd], rax

  ; print logo
  mov   rdi, logo
  call  println

  mov   qword [miaou_errno], MIAOU_ERROR_PRINT
  cmp   rax, 0
  jl    .error

  mov   rdi, log_starting_server
  call  println

  mov   qword [miaou_errno], MIAOU_ERROR_PRINT
  cmp   rax, 0
  jl    .error

  ; malloc main set of file descriptors
  mov   rdi, FD_SET_STRUCT_LEN
  call  malloc
  
  mov   qword [miaou_errno], MIAOU_ERROR_MALLOC
  cmp   rax, 0
  jl    .error

  mov   qword [main_fds], rax

  ; malloc read set of file descriptors
  mov   rdi, FD_SET_STRUCT_LEN
  call  malloc

  mov   qword [miaou_errno], MIAOU_ERROR_MALLOC
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

  ; set local input file descriptor
  mov   rdi, STDIN_FILENO
  mov   rsi, qword [main_fds]
  call  FD_SET
  cmp   rax, 0
  jl    .error

  ; create admin user
  mov   rdi, STDIN_FILENO
  call  create_admin_user
  cmp   rax, 0
  jl    .error
  
.loop:
  ; back up main fd set
  mov   rdi, qword [read_fds]
  mov   rsi, qword [main_fds]
  mov   rcx, FD_SET_STRUCT_LEN
  rep   movsb

  mov   rdi, MAX_CONNECT
  mov   rsi, qword [read_fds]
  call  read_select
  cmp   rax, 0
  jl    .error

  mov   qword [curr_fd], -1   ; curr_fd is incremented at the beginning

.inner_loop:
  inc   qword [curr_fd]

  mov   rax, MAX_CONNECT
  cmp   qword [curr_fd], rax
  jg    .inner_loop_end

  ; filter only active or new users
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

  ; add new user 
  mov   rdi, qword [server_fd]
  call  accept_connection
  cmp   rax, 0
  jl    .error

  mov   qword [user_fd], rax

  ; set the file descriptor
  mov   rdi, qword [user_fd]
  mov   rsi, qword [main_fds]
  call  FD_SET
  cmp   rax, 0
  jl    .error

  mov   rdi, qword [user_fd]
  call  create_user
  cmp   rax, 0
  jl    .error

  mov   [user_struct], rax
  
  ; send the welcome message
  mov   rdi, qword [user_fd]
  mov   rsi, msg
  mov   rdx, msg_len
  call  write_socket
  cmp   rax, 0
  jl    .error

  jmp   .inner_loop

.read_msg:
  mov   rdi, qword [curr_fd]
  call  get_user_by_fd
  cmp   rax, 0
  jle   .error  ; TODO: probably doing something different if 0
  
  mov   [rsp+0x10], rax

  ; get message from the user
  mov   rdi, qword [curr_fd]
  mov   rsi, buf
  mov   rdx, BUFSIZ
  call  read_socket
  cmp   rax, 0
  je    .clear_fd
  jl    .error

  mov   qword [rsp], rax

  ; check if user's username has been set
  mov   rax, [rsp+0x10]

  cmp   qword [rax+USER_STRUCT_OFFSET_USERNAME], 0
  jg    .read_and_send_message

  ; make sure length of the username is greater than 6
  mov   rax, qword [rsp]
  cmp   rax, USERNAME_MIN_LENGTH
  jg    .set_username

  ; send message requiring long enough username
  mov   rdi, qword [curr_fd]
  mov   rsi, msg_uname_too_short
  call  write_socket
  cmp   rax, 0
  jl    .error

  jmp   .inner_loop

.set_username:
  mov   rdi, [rsp+0x10]
  mov   rsi, buf
  mov   rdx, qword [rsp]
  dec   rdx
  call  set_user_username
  cmp   rax, 0
  jl    .error

  mov   rdi, buf
  mov   rsi, qword [rsp]
  dec   rsi
  call  boeuf_ncreate

  mov   qword [miaou_errno], MIAOU_ERROR_BOEUF
  cmp   rax, 0
  jl    .error

  mov   [rsp+0x18], rax

  mov   rdi, rax
  mov   rsi, log_new_connection
  call  boeuf_append

  mov   qword [miaou_errno], MIAOU_ERROR_BOEUF
  cmp   rax, 0
  jl    .error

  mov   [rsp+0x18], rax

  mov   rdi, rax
  call  println

  mov   qword [miaou_errno], MIAOU_ERROR_PRINT
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

  mov   qword [miaou_errno], MIAOU_ERROR_BOEUF
  cmp   rax, 0
  jl    .error

  jmp   .inner_loop

.read_and_send_message:
  ; check if user is admin
  mov   rax, qword [curr_fd]
  cmp   rax, STDIN_FILENO
  jne   .check_restrictions

  mov   rdi, buf
  mov   rsi, shutdown_msg
  mov   rdx, shutdown_msg_len
  call  strncmp
  je    .close_server

  mov   rdi, buf
  mov   rsi, ban_msg
  call  starts_with
  cmp   rax, TRUE
  jne   .message_allowed

  ; remove new line at the end of the buffer
  mov   rdi, buf
  add   rdi, qword [rsp]
  dec   rdi
  mov   rax, NULL_CHAR
  stosb

  ; get username
  mov   rdi, buf
  add   rdi, ban_msg_len
  inc   rdi

  ; get user by username
  call  get_user_by_username
  cmp   rax, 0
  je    .user_does_not_exist
  jl    .error

  ; ban user
  mov   rdi, rax
  call  ban_user
  cmp   rax, 0
  jl    .error

  jmp   .inner_loop

.user_does_not_exist:
  mov   rdi, ban_no_user
  call  println

  jmp   .inner_loop

.check_restrictions:
  ; check if user is closing the connection
  mov   rdi, buf
  mov   rsi, close_msg
  mov   rdx, close_msg_len
  call  strncmp
  je    .clear_fd

  ; check rate limiter
  call  now 

  mov   rdi, [rsp+0x10]
  mov   rbx, qword [rdi+USER_STRUCT_OFFSET_LAST_MESSAGE]

  mov   qword [rdi+USER_STRUCT_OFFSET_LAST_MESSAGE], rax

  sub   rax, rbx
  
  cmp   rax, RATE_LIMIT
  jge   .message_allowed

  ; add one strike
  inc   qword [rdi+USER_STRUCT_OFFSET_STRIKES]

  cmp   qword [rdi+USER_STRUCT_OFFSET_STRIKES], STRIKES_LIMIT
  jl    .no_ban   

  ; TODO: ban user
  mov   rdi, [rsp+0x10]
  call  ban_user

.no_ban:
  jmp   .inner_loop

.message_allowed:
  ; create boeuf buffer to display message
  ; get color
  mov   rdi, [rsp+0x10]
  mov   rsi, [rdi+USER_STRUCT_OFFSET_COLOR]
  mov   rdi, rsi
  call  boeuf_create

  mov   qword [miaou_errno], MIAOU_ERROR_BOEUF
  cmp   rax, 0
  jl    .error

  mov   [rsp+0x18], rax

  ; get username
  mov   rdi, [rsp+0x10]
  mov   rsi, [rdi+USER_STRUCT_OFFSET_USERNAME]
  mov   rdi, [rsp+0x18]
  call  boeuf_append

  mov   qword [miaou_errno], MIAOU_ERROR_BOEUF
  cmp   rax, 0
  jl    .error

  ; reset color
  mov   rdi, rax
  mov   rsi, DEFAULT_FG
  call  boeuf_append

  mov   qword [miaou_errno], MIAOU_ERROR_BOEUF

  mov   rdi, rax
  mov   rsi, uname_msg_delim
  call  boeuf_append

  mov   qword [miaou_errno], MIAOU_ERROR_BOEUF
  cmp   rax, 0
  jl    .error

  mov   rdi, rax
  mov   rsi, buf
  mov   rdx, qword [rsp]
  call  boeuf_nappend

  mov   qword [miaou_errno], MIAOU_ERROR_BOEUF
  cmp   rax, 0
  jl    .error
  
  mov   [rsp+0x18], rax

  ; if user is admin, skip print
  cmp   qword [curr_fd], STDIN_FILENO
  je    .skip_print

  mov   rdi, [rsp+0x18]
  call  println

  mov   qword [miaou_errno], MIAOU_ERROR_PRINT
  cmp   rax, 0
  jl    .error

.skip_print:
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

  mov   qword [miaou_errno], MIAOU_ERROR_BOEUF
  cmp   rax, 0
  jl    .error

  jmp   .inner_loop
  
.clear_fd:
  ; shutdown the server if admin leaves
  cmp   qword [curr_fd], 0
  je    .end_loop

  ; close connection
  mov   rdi, qword [curr_fd]
  call  close_socket

  mov   rsi, [rsp+0x10]
  mov   rdi, qword [rsi+USER_STRUCT_OFFSET_USERNAME]
  call  boeuf_create

  mov   qword [miaou_errno], MIAOU_ERROR_BOEUF
  cmp   rax, 0
  jl    .error

  mov   rdi, rax
  mov   rsi, log_close
  call  boeuf_append

  mov   qword [miaou_errno], MIAOU_ERROR_BOEUF
  cmp   rax, 0
  jl    .error

  mov   [rsp+0x18], rax

  mov   rdi, [rsp+0x18]
  call  println

  mov   qword [miaou_errno], MIAOU_ERROR_PRINT
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

  mov   qword [miaou_errno], MIAOU_ERROR_BOEUF
  cmp   rax, 0
  jl    .error

  ; free username
  mov   rsi, [rsp+0x10]
  mov   rdi, [rsi+USER_STRUCT_OFFSET_USERNAME]
  call  free

  mov   qword [miaou_errno], MIAOU_ERROR_FREE
  cmp   rax, 0
  jl    .error

  ; remove user from array
  mov   rdi, qword [curr_fd]
  call  remove_user_from_array
  cmp   rax, 0
  jl    .error

  ; free user struct
  mov   rdi, qword [rsp+0x10]
  call  free

  mov   qword [miaou_errno], MIAOU_ERROR_FREE
  cmp   rax, 0
  jl    .error

  jmp   .inner_loop

.inner_loop_end:
  jmp   .loop

.end_loop:

.close_server:
  ; TODO: free resources and close fd

  mov   rdi, SUCCESS_CODE
  call  exit

.error:
  call  exit

; exits the program with error code
exit:
  mov   rax, SYS_EXIT
  mov   rdi, qword [miaou_errno]
  syscall

