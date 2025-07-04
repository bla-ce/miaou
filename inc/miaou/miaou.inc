%include "ascii.inc"
%include "ban.inc"
%include "boeuf.inc"
%include "calloc.inc"
%include "constants.inc"
%include "error.inc"
%include "file.inc"
%include "free.inc"
%include "hash_table.inc"
%include "malloc.inc"
%include "memset.inc"
%include "mmap.inc"
%include "realloc.inc"
%include "select.inc"
%include "socket.inc"
%include "string.inc"
%include "syscall.inc"
%include "time.inc"

section .bss
_user_struct:
  _user_fd            resq 1
  _user_username      resq 1
  _user_color         resq 1
  _user_active        resq 1
  _user_last_message  resq 1
  _user_strikes       resq 1
_user_struct_end:

section .data
  logo  db  LINE_FEED
        db  "      /\_____/\     ", LINE_FEED
        db  "     /  o   o  \    ", LINE_FEED
        db  "    ( ==  ^  == )   ", LINE_FEED
        db  "     )         (    ", LINE_FEED
        db  "    (   MIAOU   )   ", LINE_FEED
        db  "   (             )  ", LINE_FEED
        db  "  (  (  )   (  )  ) ", LINE_FEED
        db  " (___(__)___(__)___)", LINE_FEED, NULL_CHAR

  USER_STRUCT_LEN equ _user_struct_end - _user_struct
  
  ; user offsets
  USER_STRUCT_OFFSET_FD           equ 0
  USER_STRUCT_OFFSET_USERNAME     equ USER_STRUCT_OFFSET_FD + 0x8
  USER_STRUCT_OFFSET_COLOR        equ USER_STRUCT_OFFSET_USERNAME + 0x8
  USER_STRUCT_OFFSET_ACTIVE       equ USER_STRUCT_OFFSET_COLOR + 0x8
  USER_STRUCT_OFFSET_LAST_MESSAGE equ USER_STRUCT_OFFSET_ACTIVE + 0x8
  USER_STRUCT_OFFSET_STRIKES      equ USER_STRUCT_OFFSET_LAST_MESSAGE + 0x8

  msg       db "Hello, user!", LINE_FEED
            db "Enter your username: ", NULL_CHAR
  msg_len   equ $-msg - 1

  RED_FG          db 0x1b, "[31m", NULL_CHAR
  GREEN_FG        db 0x1b, "[32m", NULL_CHAR
  YELLOW_FG       db 0x1b, "[33m", NULL_CHAR
  BLUE_FG         db 0x1b, "[34m", NULL_CHAR
  MAGENTA_FG      db 0x1b, "[35m", NULL_CHAR
  CYAN_FG         db 0x1b, "[36m", NULL_CHAR
  GRAY_FG         db 0x1b, "[90m", NULL_CHAR
  BRIGHT_RED_FG   db 0x1b, "[91m", NULL_CHAR
  BRIGHT_GREEN_FG db 0x1b, "[92m", NULL_CHAR
  DEFAULT_FG      db 0x1b, "[39m", NULL_CHAR

  colors  dq RED_FG, GREEN_FG, YELLOW_FG, BLUE_FG, MAGENTA_FG, CYAN_FG
          dq GRAY_FG, BRIGHT_RED_FG, BRIGHT_GREEN_FG

  uname_msg_delim db COLON, LINE_FEED, SPACE, SPACE, NULL_CHAR

  port      dq 4747
  read_fds  dq 0
  main_fds  dq 0
  server_fd dq 0
  user_fd   dq 0

  admin_username      db "admin", NULL_CHAR
  admin_username_len  equ $-admin_username - 1
  USERNAME_MIN_LENGTH equ 3
  USERNAME_MAX_LENGTH equ 32
  
  curr_fd     dq 0
  MAX_CONNECT equ 10
   
  BUFSIZ  equ 0x400
  buf     times BUFSIZ db 0

  RATE_LIMIT    equ 1   ; rate limiter in second
  STRIKES_LIMIT equ 10  ; after 10 strikes, user is banned

  ; array of users
  users               times MAX_CONNECT dq 0
  user_struct         dq 0
  active_connections  dq 1  ; one by default to include server
  
  log_starting_server   db "[INFO] starting server...", LINE_FEED, NULL_CHAR

  log_should_be_banned db "[WARN] user should be banned", NULL_CHAR

  log_connection_failed db "[ERROR] failed to connect to server", NULL_CHAR

  log_new_connection  db " joined the chat", LINE_FEED, NULL_CHAR
  log_close           db " left the chat", LINE_FEED, NULL_CHAR
  log_ban             db " has been banned from the chat", LINE_FEED, NULL_CHAR
  
  ban_no_user db "The user you are trying to ban is not in this chat", NULL_CHAR

  msg_invalid_uname db "The username you provided is invalid or is already in the chat", LINE_FEED
                    db "Please enter new username: 3 to 32 charachters without space", NULL_CHAR

  msg_invalid_uname_len equ $-msg_invalid_uname - 1

  close_msg     db "!close", LINE_FEED, NULL_CHAR
  close_msg_len equ $-close_msg - 1

  ban_msg     db "!ban", NULL_CHAR
  ban_msg_len equ $-ban_msg - 1

  shutdown_msg     db "!shutdown", LINE_FEED, NULL_CHAR
  shutdown_msg_len equ $-shutdown_msg - 1

section .text;
; checks if a user is already in the chat
; @param  rdi: pointer to the username
; @return rax: user is in the chat
is_user_in_chat:
  sub   rsp, 0x20
  
  ; *** STACK USAGE *** ;
  ; [rsp]       -> pointer to the username
  ; [rsp+0x8]   -> pointer to the users array
  ; [rsp+0x10]  -> index
  ; [rsp+0x18]  -> length of the username

  mov   [rsp], rdi
  mov   qword [rsp+0x8], users
  mov   qword [rsp+0x10], 0

  mov   qword [miaou_errno], MIAOU_ERROR_INVALID_ARG
  cmp   rdi, 0
  jle   .error

  mov   rdi, [rsp]
  call  strlen

  mov   qword [miaou_errno], MIAOU_ERROR_STRLEN
  cmp   rax, 0
  jl    .error

  dec   rax ; remove the line feed
  mov   qword [rsp+0x18], rax

.loop:
  mov   rsi, [rsp+0x8]
  mov   rdi, [rsi]

  cmp   rdi, 0
  je    .null_struct

  ; get username
  mov   rsi, [rdi+USER_STRUCT_OFFSET_USERNAME]
  cmp   rsi, 0
  je    .null_struct

  mov   rdi, [rsp]
  mov   rdx, qword [rsp+0x18]
  call  strncmp
  cmp   rax, 0
  jl    .error

  cmp   rax, TRUE
  je    .in_chat

.null_struct:
  ; move to next struct
  mov   rdi, [rsp+0x8]
  add   rdi, 8
  mov   [rsp+0x8], rdi

  inc   qword [rsp+0x10]
  mov   rax, qword [rsp+0x10]
  cmp   rax, qword [active_connections]
  jg    .loop_end

  jmp   .loop

.loop_end:
  mov   rax, FALSE
  jmp   .return

.in_chat:
  mov   rax, TRUE
  jmp   .return
  
.error:
  mov   rax, FAILURE_CODE

.return:
  add   rsp, 0x20
  ret

; takes a pointer to the username of the user and checks if it's valid
; @param  rdi: pointer to the username
; @return rax: username is valid
validate_username:
  sub   rsp, 0x8

  ; *** STACK USAGE *** ;
  ; [rsp]     -> pointer to the username

  mov   [rsp], rdi

  mov   qword [miaou_errno], MIAOU_ERROR_INVALID_ARG
  cmp   rdi, 0
  jle   .error

  ; get length of the username
  call  strlen

  mov   qword [miaou_errno], MIAOU_ERROR_STRLEN
  cmp   rax, 0
  jl    .error

  cmp   rax, USERNAME_MIN_LENGTH
  jl    .invalid

  cmp   rax, USERNAME_MAX_LENGTH
  jg    .invalid

  ; make sure there is no space (only invalid character for now)
  mov   rdi, [rsp]

.loop:
  mov   al, byte [rdi]
  movsx rax, al
  cmp   rax, SPACE
  je    .invalid

  cmp   rax, NULL_CHAR
  je    .loop_end

  inc   rdi
  jmp   .loop
.loop_end:

  ;  make sure the user is not already in the chat
  mov   rdi, [rsp]
  call  is_user_in_chat
  cmp   rax, 0
  jl    .error

  cmp   rax, TRUE
  je    .invalid

  mov   rax, TRUE

  jmp   .return

.invalid:
  mov   rax, FALSE
  jmp   .return

.error:
  mov   rax, FAILURE_CODE

.return:
  add   rsp, 0x8
  ret
  
; sends the string in rdi to all file descriptors set
; @param  rdi: pointer to the message
; @param  rsi: server file descriptor
; @param  rdx: current file descriptor
; @param  rcx: pointer to the file descriptor set
; @return rax: return code
send_message_to_all_fds:
  sub   rsp, 0x28

  mov   qword [miaou_errno], MIAOU_ERROR_INVALID_ARG
  cmp   rdi, 0
  jl    .error

  cmp   rsi, 0
  jl    .error

  cmp   rdx, 0
  jl    .error

  cmp   rcx, 0
  jl    .error

  ; *** STACK USAGE *** ;
  ; [rsp]       -> pointer to the message
  ; [rsp+0x8]   -> server file descriptor
  ; [rsp+0x10]  -> current file descriptor
  ; [rsp+0x18]  -> pointer to the file descriptor set
  ; [rsp+0x20]  -> length of the message

  mov   [rsp], rdi
  mov   [rsp+0x8], rsi
  mov   [rsp+0x10], rdx
  mov   [rsp+0x18], rcx

  mov   rdi, [rsp]
  call  strlen

  mov   qword [miaou_errno], MIAOU_ERROR_STRLEN
  cmp   rax, 0
  jl    .error

  mov   qword [rsp+0x20], rax

.send_loop:
  inc   qword [rsp+0x8]
  mov   rax, MAX_CONNECT
  cmp   qword [rsp+0x8], rax
  jge   .send_loop_end

  mov   rax, qword [rsp+0x10]
  cmp   rax, qword [rsp+0x8]
  je    .send_loop

  mov   rdi, qword [rsp+0x8]
  mov   rsi, [rsp+0x18]
  call  FD_ISSET
  cmp   rax, 0
  jl    .error
  je    .send_loop

  mov   rdi, qword [rsp+0x8]
  mov   rsi, [rsp]
  mov   rdx, qword [rsp+0x20]
  inc   rdx                     ; include null char
  call  write_socket
  cmp   rax, 0
  jl    .error

  jmp   .send_loop
.send_loop_end:

  mov   rax, SUCCESS_CODE
  jmp   .return

.error:
  mov   rax, FAILURE_CODE

.return:
  add   rsp, 0x28
  ret

; removes the user from the users array
; @param  rdi: file descriptor of the user
remove_user_from_array:
  mov   qword [miaou_errno], MIAOU_ERROR_INVALID_ARG
  cmp   rdi, 0
  jl    .error

  xor   rax, rax        ; index
  mov   rbx, rdi        ; store fd in rbx
  mov   rsi, users

.loop:
  mov   rdi, [rsi] 
  cmp   rdi, 0
  je    .null_struct

  cmp   qword [rdi+USER_STRUCT_OFFSET_FD], rbx
  je    .loop_end

.null_struct:
  ; move to next struct
  add   rsi, 8

  inc   rax
  cmp   rax, qword [active_connections]
  jg    .does_not_exist

  jmp   .loop

.loop_end:
  mov   qword [rsi], 0

  dec   qword [active_connections]

  jmp   .return

.does_not_exist:
  mov   rax, 0
  jmp   .return

.error:
  mov   rax, FAILURE_CODE

.return:
  ret

; returns the user struct with the username provided or return 0 if does not
; exist
; @param  rdi: pointer to the username
; @return rax: pointer to the user struct or null
get_user_by_username:
  sub   rsp, 0x18

  ; *** STACK USAGE *** ;
  ; [rsp]       -> pointer to the username
  ; [rsp+0x8]   -> index
  ; [rsp+0x10]  -> dynamic pointer to the user array

  mov   [rsp], rdi
  mov   qword [rsp+0x8], 0
  mov   qword [rsp+0x10], users

  mov   qword [miaou_errno], MIAOU_ERROR_INVALID_ARG
  cmp   rdi, 0
  jl    .error

.loop:
  mov   rsi, [rsp+0x10]
  mov   rdi, [rsi]

  cmp   rdi, 0
  je    .null_struct

  ; get username
  mov   rsi, [rdi+USER_STRUCT_OFFSET_USERNAME]
  mov   rdi, [rsp]
  call  strcmp
  cmp   rax, 0
  jl    .error

  cmp   rax, TRUE
  je    .loop_end

.null_struct:
  ; move to next struct
  mov   rdi, [rsp+0x10]
  add   rdi, 8
  mov   [rsp+0x10], rdi

  inc   qword [rsp+0x8]
  mov   rax, qword [rsp+0x8]
  cmp   rax, qword [active_connections]
  jg    .does_not_exist

  jmp   .loop

.loop_end:
  mov   rdi, [rsp+0x10]
  mov   rax, [rdi]
  jmp   .return

.does_not_exist:
  mov   rax, 0
  jmp   .return

.error:
  mov   rax, FAILURE_CODE

.return:
  add   rsp, 0x18
  ret

; returns the user struct with the file descriptor or return 0 if does not
; exist
; @param  rdi: file descriptor
; @return rax: pointer to the user struct or null
get_user_by_fd:
  mov   qword [miaou_errno], MIAOU_ERROR_INVALID_ARG
  cmp   rdi, 0
  jl    .error

  xor   rax, rax        ; index
  mov   rbx, rdi        ; store fd in rbx
  mov   rsi, users

.loop:
  mov   rdi, [rsi] 
  cmp   rdi, 0
  je    .null_struct

  cmp   qword [rdi+USER_STRUCT_OFFSET_FD], rbx
  je    .loop_end

.null_struct:
  ; move to next struct
  add   rsi, 8

  inc   rax
  cmp   rax, qword [active_connections]
  jg    .does_not_exist

  jmp   .loop

.loop_end:

  mov   rax, rdi
  jmp   .return

.does_not_exist:
  mov   rax, 0
  jmp   .return

.error:
  mov   rax, FAILURE_CODE

.return:
  ret

; creates a normal user from a user connection
; @param  rdi: file descriptor of the user
; @return rax: pointer to the user struct
create_user:
  sub   rsp, 0x10

  mov   qword [miaou_errno], MIAOU_ERROR_INVALID_ARG
  cmp   rdi, 0
  jl    .error

  ; *** STACK USAGE *** ;
  ; [rsp]     -> file descriptor of the server
  ; [rsp+0x8] -> pointer to the user user struct

  mov   qword [rsp], rdi
  mov   qword [rsp+0x8], 0

  ; malloc new user
  mov   rdi, USER_STRUCT_LEN
  call  malloc

  mov   qword [miaou_errno], MIAOU_ERROR_MALLOC
  cmp   rax, 0
  jl    .error

  mov   [rsp+0x8], rax

  call  now

  mov   rsi, [rsp+0x8] 

  mov   rdi, qword [rsp]
  mov   qword [rsi+USER_STRUCT_OFFSET_FD], rdi
  mov   qword [rsi+USER_STRUCT_OFFSET_ACTIVE], TRUE
  mov   qword [rsi+USER_STRUCT_OFFSET_LAST_MESSAGE], rax
  mov   qword [rsi+USER_STRUCT_OFFSET_STRIKES], 0

  ; username will be assigned later
  mov   qword [rsi+USER_STRUCT_OFFSET_USERNAME], 0

  ; get user position in array
  xor   rdx, rdx
  mov   rax, qword [active_connections]
  dec   rax
  mov   rbx, 8
  mul   rbx

  mov   rdi, colors
  add   rdi, rax
  mov   rsi, [rdi]
  mov   rdi, [rsp+0x8]

  mov   qword [rdi+USER_STRUCT_OFFSET_COLOR], rsi

  ; add user to the array
  mov   rdi, users
  add   rdi, rax
  mov   rsi, [rsp+0x8]
  mov   [rdi], rsi

  mov   rax, [rsp+0x8]

  jmp   .return

.error:
  mov   rdi, [rsp+0x8]
  test  rdi, rdi
  jz    .no_free

  call  free

.no_free:
  mov   rax, FAILURE_CODE

.return:
  add   rsp, 0x10
  ret

; creates an admin user from the server
; @param  rdi: file descriptor of the server
; @return rax: pointer to the user struct
create_admin_user:
  sub   rsp, 0x8

  ; *** STACK USAGE *** ;
  ; [rsp]     -> file descriptor of the server
  ; [rsp+0x8] -> pointer to the user struct

  ; create_user checks arg
  call  create_user
  cmp   rax, 0
  jl    .error

  mov   [rsp], rax

  mov   rdi, rax
  mov   rsi, admin_username
  mov   rdx, admin_username_len
  call  set_user_username
  cmp   rax, 0
  jl    .error

  mov   rax, [rsp]

  jmp   .return

.error:
  mov   rax, FAILURE_CODE

.return:
  add   rsp, 0x8
  ret

; sets the user's username
; @param  rdi: pointer to the user struct
; @param  rsi: pointer to the username
; @param  rdx: length of the username
; @return rax: return code
set_user_username:
  sub   rsp, 0x20

  mov   qword [miaou_errno], MIAOU_ERROR_INVALID_ARG
  cmp   rdi, 0
  jl    .error

  cmp   rsi, 0
  jl    .error
  
  cmp   rdx, 0
  jl    .error

  ; *** STACK USAGE *** ;
  ; [rsp]       -> pointer to the user struct
  ; [rsp+0x8]   -> pointer to the username
  ; [rsp+0x10]  -> length of the username
  ; [rsp+0x18]  -> pointer to the mallocd username

  mov   [rsp], rdi
  mov   [rsp+0x8], rsi
  mov   qword [rsp+0x10], rdx
  mov   qword [rsp+0x18], 0

  ; malloc string for username
  mov   rdi, qword [rsp+0x10]
  inc   rdi               ; add one for null char
  call  malloc

  mov   qword [miaou_errno], MIAOU_ERROR_MALLOC
  cmp   rax, 0
  jl    .error

  mov   [rsp+0x18], rax

  ; copy username
  mov   rdi, rax
  mov   rsi, [rsp+0x8]
  mov   rcx, [rsp+0x10]
  rep   movsb

  ; add null char
  mov   rax, NULL_CHAR
  stosb

  mov   rax, [rsp+0x18]

  ; set username
  mov   rdi, [rsp]
  mov   [rdi+USER_STRUCT_OFFSET_USERNAME], rax

  mov   rax, SUCCESS_CODE
  jmp   .return

.error:
  mov   rdi, [rsp+0x10]
  test  rdi, rdi
  jz    .no_free

  call  free

.no_free:
  mov   rax, FAILURE_CODE

.return:
  add   rsp, 0x20
  ret

