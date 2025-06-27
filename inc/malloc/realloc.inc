section .data

section .text

; changes the size of a memory chunk and copy its content
; @param    rdi: pointer to the memory chunk
; @param    rsi: new size
; @return   rax: pointer to the new memory chunk
realloc:
  sub   rsp, 0x20

  cmp   rdi, 0
  jl    .error
  je    .malloc

  cmp   rsi, 0
  jl    .error
  je    .free

  ; *** STACK USAGE *** ;
  ; [rsp]       -> pointer to the memory chunk
  ; [rsp+0x8]   -> new size of the memory chunk
  ; [rsp+0x10]  -> old size of the memory chunk
  ; [rsp+0x18]  -> pointer to the new memory chunk

  mov   [rsp], rdi
  mov   qword [rsp+0x8], rsi

  call  get_chunk_header
  cmp   rax, 0
  jl    .error

  mov   rdi, rax

  mov   rax, qword [rdi+CHUNK_OFFSET_SIZE]
  mov   qword [rsp+0x10], rax

  mov   rdi, qword [rsp+0x8]
  call  malloc
  cmp   rax, 0
  jl    .error

  mov   [rsp+0x18], rax

  ; get the smallest size to not copy too much data
  mov   rax, qword [rsp+0x8]
  mov   rbx, qword [rsp+0x10]
  cmp   rax, rbx
  cmovg rax, rbx

  mov   rdi, qword [rsp+0x18]
  mov   rsi, [rsp]
  mov   rcx, rax
  rep   movsb

  mov   rdi, [rsp]
  call  free
  cmp   rax, 0
  jl    .error

  mov   rax, [rsp+0x18]
  jmp   .return

.malloc:
  ; if the pointer is null, the call is equivalent to malloc
  mov   rdi, rsi
  call  malloc
  cmp   rax, 0
  jl    .error

  jmp   .return

.free:
  ; if size is 0, free the pointer
  call  free
  jmp   .return

.error:
  mov   rax, FAILURE_CODE

.return:
  add   rsp, 0x20
  ret

