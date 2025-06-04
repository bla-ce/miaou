%include "ascii.inc"
%include "boeuf.inc"
%include "calloc.inc"
%include "constants.inc"
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
_client_struct:
  _client_fd        resq 1
  _client_username  resq 1
  _client_color     resq 1
  _client_active    resq 1
_client_struct_end:

section .data
  CLIENT_STRUCT_LEN equ _client_struct_end - _client_struct
  
  ; client offsets
  CLIENT_STRUCT_OFFSET_FD       equ 0
  CLIENT_STRUCT_OFFSET_USERNAME equ CLIENT_STRUCT_OFFSET_FD + 0x8
  CLIENT_STRUCT_OFFSET_COLOR    equ CLIENT_STRUCT_OFFSET_USERNAME + 0x8
  CLIENT_STRUCT_OFFSET_ACTIVE   equ CLIENT_STRUCT_OFFSET_COLOR + 0x8

  msg       db "Hello, Client!", LINE_FEED
            db "Enter your username: ", NULL_CHAR
  msg_len   equ $ - msg

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
  
  port      dq 4747
  read_fds  dq 0
  main_fds  dq 0
  server_fd dq 0
  client_fd dq 0
  
  curr_fd     dq 0
  MAX_CONNECT equ 10
   
  BUFSIZ equ 0x400
  buf times BUFSIZ db 0

  ; array of clients
  clients             times MAX_CONNECT dq 0
  client_struct       dq 0
  active_connections  dq 0
  
  log_starting_server   db "[INFO] starting server...", NULL_CHAR
  log_waiting           db "[INFO] waiting for connections...", NULL_CHAR

  log_new_connection  db " joined the chat", NULL_CHAR
  log_close           db " left the chat", NULL_CHAR

  close_msg     db "!close", LINE_FEED, NULL_CHAR
  close_msg_len equ $ - close_msg - 1

section .text;

; removes the client from the clients array
; @param  rdi: file descriptor of the client
remove_client_from_array:
  cmp   rdi, 0
  jl    .error

  xor   rax, rax        ; index
  mov   rbx, rdi        ; store fd in rbx
  mov   rsi, clients

.loop:
  mov   rdi, [rsi] 
  cmp   rdi, 0
  je    .null_struct

  cmp   qword [rdi+CLIENT_STRUCT_OFFSET_FD], rbx
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

  jmp   .return

.does_not_exist:
  mov   rax, 0
  jmp   .return

.error:
  mov   rax, FAILURE_CODE

.return:
  ret

; returns the client struct with the file descriptor or return 0 if does not
; exist
; @param  rdi: file descriptor
; @return rax: pointer to the client struct or null
get_client_by_fd:
  cmp   rdi, 0
  jl    .error

  xor   rax, rax        ; index
  mov   rbx, rdi        ; store fd in rbx
  mov   rsi, clients

.loop:
  mov   rdi, [rsi] 
  cmp   rdi, 0
  je    .null_struct

  cmp   qword [rdi+CLIENT_STRUCT_OFFSET_FD], rbx
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

