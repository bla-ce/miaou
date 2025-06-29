section .text
; allocates the specified number of bytes and initializes them to zero 
; @param  rdi: number of elements
; @param  rsi: size of element
; @return rax: chunk or null if failed
calloc:
  sub   rsp, 0x8

  test   rdi, rdi
  jz    .null

  test   rsi, rsi
  jz    .null

  ; get total size
  xor   rdx, rdx
  mov   rax, rdi
  mov   rbx, rsi
  mul   rbx

  ; call malloc
  mov   rdi, rax
  call  malloc
  cmp   rax, 0
  jl    .return
  je    .null

  mov   [rsp], rax

  ; rax points to the data
  ; go to the beginning of the header
  mov   rdi, rax
  call  get_chunk_header
  cmp   rax, 0
  jl    .error

  mov   rcx, qword [rax+CHUNK_OFFSET_SIZE]
  sub   rcx, CHUNK_METADATA_LEN

  ; go back to data and set bytes to 0
  mov   rdi, [rsp]
  mov   rsi, 0
  mov   rdx, rcx
  call  memset

  mov   rax, [rsp]

  jmp   .return

.error:
  mov   rax, FAILURE_CODE
  jmp   .return

.null:
  xor   rax, rax

.return:
  add   rsp, 0x8
  ret

