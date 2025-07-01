section .data

section .text

; bans a user from the chat
; for now it only excludes the user from the chat but they are still able to 
; connect again
; @param  rdi: pointer to the user struct
; @return rax: return code
ban_user:
  sub   rsp, 0x10

  ; *** STACK USAGE *** ;
  ; [rsp]     -> pointer to the user struct
  ; [rsp+0x8] -> pointer to the boeuffer

  mov   [rsp], rdi
  mov   qword [rsp+0x8], 0

  cmp   rdi, 0
  jle   .error

  ; close connection
  mov   rsi, [rsp]
  mov   rdi, qword [rsi+USER_STRUCT_OFFSET_FD]
  call  close_socket

  mov   rsi, [rsp]
  mov   rdi, qword [rsi+USER_STRUCT_OFFSET_USERNAME]
  call  boeuf_create

  mov   qword [miaou_errno], MIAOU_ERROR_BOEUF
  cmp   rax, 0
  jl    .error

  mov   rdi, rax
  mov   rsi, log_ban
  call  boeuf_append

  mov   qword [miaou_errno], MIAOU_ERROR_BOEUF
  cmp   rax, 0
  jl    .error

  mov   [rsp+0x8], rax

  mov   rdi, [rsp+0x8]
  call  println

  mov   qword [miaou_errno], MIAOU_ERROR_PRINT
  cmp   rax, 0
  jl    .error

  ; send message to each connection
  mov   rdi, [rsp]
  mov   rdx, qword [rdi+USER_STRUCT_OFFSET_FD]

  mov   rdi, [rsp+0x8]
  mov   rsi, qword [server_fd]
  mov   rcx, [main_fds]
  call  send_message_to_all_fds
  cmp   rax, 0
  jl    .error

  ; clear fd
  mov   rsi, [rsp]
  mov   rdi, qword [rsi+USER_STRUCT_OFFSET_FD]
  mov   rsi, qword [main_fds]
  call  FD_CLR
  cmp   rax, 0
  jl    .error

  ; free boeuf buffer
  mov   rdi, [rsp+0x8]
  call  boeuf_free

  mov   qword [miaou_errno], MIAOU_ERROR_BOEUF
  cmp   rax, 0
  jl    .error

  ; free username
  mov   rsi, [rsp]
  mov   rdi, [rsi+USER_STRUCT_OFFSET_USERNAME]
  call  free

  mov   qword [miaou_errno], MIAOU_ERROR_FREE
  cmp   rax, 0
  jl    .error

  ; remove user from array
  mov   rsi, [rsp]
  mov   rdi, qword [rsi+USER_STRUCT_OFFSET_FD]
  call  remove_user_from_array
  cmp   rax, 0
  jl    .error

  ; free user struct
  mov   rdi, qword [rsp]
  call  free

  mov   qword [miaou_errno], MIAOU_ERROR_FREE
  cmp   rax, 0
  jl    .error

  mov   rax, SUCCESS_CODE

  jmp   .return

.error:
  mov   rdi, [rsp+0x8]
  test  rdi, rdi
  jz    .no_free

  call  boeuf_free

  mov   qword [miaou_errno], MIAOU_ERROR_FREE
  cmp   rax, 0
  jl    .error

.no_free:
  mov   rax, FAILURE_CODE

.return:
  add   rsp, 0x10
  ret

ban_user_by_username:
  ; TODO
  ret

